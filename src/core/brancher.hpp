#ifndef AC3ADB13_86B0_4D83_9DA4_AF8BC421F010
#define AC3ADB13_86B0_4D83_9DA4_AF8BC421F010

#include "foundation.hpp"
#include "common_types.hpp"
#include "runtime.hpp"
#include "shards.h"
#include "shards.hpp"

namespace shards {

enum BranchFailureBehavior { Everything, Known, Ignore };
DECL_ENUM_INFO(BranchFailureBehavior, BranchFailure, 'brcB');

// Runs wires in a separate mesh while capturing variables
struct Brancher {
  static inline shards::Types RunnableTypes{CoreInfo::NoneType, CoreInfo::WireType, Type::SeqOf(CoreInfo::WireType),
                                            CoreInfo::ShardRefSeqType, Type::SeqOf(CoreInfo::ShardRefSeqType)};

  std::shared_ptr<SHMesh> mesh;
  std::vector<std::shared_ptr<SHWire>> wires;
  bool captureAll = false;
  BranchFailureBehavior failureBehavior = BranchFailureBehavior::Everything;

private:
  std::unordered_map<std::string_view, SHExposedTypeInfo> _collectedRequirements;
  ExposedInfo _mergedRequirements;
  ExposedInfo _shared;

public:
  ~Brancher() { cleanup(); }

  // Add a sequence of shards as a looped wire
  void addLoopedShards(const SHVar &var) {
    assert(var.valueType == SHType::Seq);
    const SHSeq &seq = var.payload.seqValue;
    assert(seq.len == 0 || seq.elements[0].valueType == SHType::ShardRef);
    auto wire = wires.emplace_back(SHWire::make());
    wire->looped = true;
    ForEach(seq, [&](SHVar &v) {
      assert(v.valueType == SHType::ShardRef);
      wire->addShard(v.payload.shardValue);
    });
  }

  // Adds a wire
  void addWire(const SHVar &var) {
    assert(var.valueType == SHType::Wire);
    wires.emplace_back(*(std::shared_ptr<SHWire> *)var.payload.wireValue);
  }

  // Adds a single wire or sequence of shards as a looped wire
  void addRunnable(const SHVar &var) {
    if (var.valueType == SHType::Wire) {
      addWire(var);
    } else {
      addLoopedShards(var);
    }
  }

  // Sets the runnables (wires or shards)
  void setRunnables(const SHVar &var) {
    wires.clear();
    if (var.valueType != SHType::None) {
      if (var.valueType == SHType::Wire) {
        addRunnable(var);
      } else {
        assert(var.valueType == SHType::Seq);
        auto &seq = var.payload.seqValue;
        if (seq.len == 0)
          return;

        if (seq.elements[0].valueType == SHType::ShardRef) {
          // Single sequence of shards
          addLoopedShards(var);
        } else {
          // Multiple wires or sequences of shards
          for (uint32_t i = 0; i < seq.len; i++) {
            SHVar &v = seq.elements[i];
            addRunnable(v);
          }
        }
      }
    }
  }

  SHExposedTypesInfo requiredVariables() { return (SHExposedTypesInfo)_mergedRequirements; }

  void compose(const SHInstanceData &data) {
    if (!mesh)
      mesh = SHMesh::make();

    _collectedRequirements.clear();

    for (auto &wire : wires) {
      composeSubWire(data, wire);
    }

    // Merge requirements from compose result
    for (auto &wire : wires) {
      for (auto &req : wire->composeResult->requiredInfo) {
        _mergedRequirements.push_back(req);
      }
    }

    // Merge deep requirements
    if (captureAll) {
      for (auto &avail : data.shared) {
        SHLOG_TRACE("Branch: adding variable to requirements: {}", avail.name);
        _mergedRequirements.push_back(avail);
      }
    } else {
      for (auto &avail : data.shared) {
        auto it = _collectedRequirements.find(avail.name);
        if (it != _collectedRequirements.end()) {
          SHLOG_TRACE("Branch: adding variable to requirements: {}", avail.name);
          _mergedRequirements.push_back(it->second);
        }
      }
    }

    // Copy shared
    _shared = ExposedInfo(data.shared);
    mesh->instanceData.shared = (SHExposedTypesInfo)_shared;
  }

  void warmup(SHContext *context, const SHVar &input = Var::Empty) {
    // grab all the variables we need and reference them
    for (const auto &req : _mergedRequirements._innerInfo) {
      if (mesh->refs.count(req.name) == 0) {
        SHLOG_TRACE("Branch: referencing required variable: {}", req.name);
        auto vp = referenceVariable(context, req.name);
        mesh->refs[req.name] = vp;
      }
    }

    for (const auto &wire : wires) {
      mesh->schedule(wire, input, false);
    }
  }

  void cleanup() {
    if (mesh) {
      for (auto &[_, vp] : mesh->refs) {
        releaseVariable(vp);
      }

      mesh.reset();
    }
  }

  // Update the that the branched wires read
  void updateInputs(const SHVar &input = Var::Empty) {
    emplaceInputs([&](SHVar &outVar) { cloneVar(outVar, input); });
  }

  // Update inputs directly without copying, should still transfer ownership
  template <typename T> void emplaceInputs(T &&cb) {
    for (auto &wire : wires) {
      cb(wire->currentInput);
    }
  }

  void activate() {
    if (!mesh->tick()) {
      switch (failureBehavior) {
      case BranchFailureBehavior::Ignore:
        break;
      case BranchFailureBehavior::Everything:
        throw ActivationError("Branched mesh had errors");
      case BranchFailureBehavior::Known:
        for (const auto &wire : wires) {
          auto failed = mesh->failedWires();
          if (std::count(failed.begin(), failed.end(), wire.get())) {
            throw ActivationError("Branched mesh had errors");
          }
        }
        break;
      }
    }
  }

private:
  void composeSubWire(const SHInstanceData &data, const std::shared_ptr<SHWire> &wire) {
    wire->mesh = mesh; // this is needed to be correct

    auto dataCopy = data;
    dataCopy.wire = wire.get();

    // Branch needs to capture all it needs, so we need deeper informations
    // this is triggered by populating requiredVariables variable
    dataCopy.requiredVariables = &_collectedRequirements;

    wire->composeResult = composeWire(
        wire.get(),
        [](const Shard *errorShard, const char *errorTxt, bool nonfatalWarning, void *userData) {
          if (!nonfatalWarning) {
            SHLOG_ERROR("Branch: failed inner wire validation, error: {}", errorTxt);
            throw ComposeError("RunWire: failed inner wire validation");
          } else {
            SHLOG_INFO("Branch: warning during inner wire validation: {}", errorTxt);
          }
        },
        this, dataCopy);
  }
};
} // namespace shards

#endif /* AC3ADB13_86B0_4D83_9DA4_AF8BC421F010 */
