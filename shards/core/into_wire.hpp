#ifndef C81B8E95_5381_4841_8931_F7EE0CB8EEEA
#define C81B8E95_5381_4841_8931_F7EE0CB8EEEA

#include <shards/common_types.hpp>
#include <shards/shards.hpp>

namespace shards {

// Converts either a Wire/ShardsSeq into a std::shared_ptr<SHWire>
struct IntoWire {
  static inline shards::Types RunnableTypes{CoreInfo::NoneType, CoreInfo::WireType, CoreInfo::ShardRefSeqType};

  std::shared_ptr<SHWire> result;

  // Adds a single wire or sequence of shards as a looped wire
  IntoWire &runnable(const SHVar &var) {
    if (var.valueType != SHType::None) {
      if (var.valueType == SHType::Wire) {
        wire(var);
      } else {
        loopedShards(var);
      }
    } else {
      result.reset();
    }
    return *this;
  }

  operator std::shared_ptr<SHWire> &() { return result; }

private:
  friend struct IntoWires;

  // Add a sequence of shards as a looped wire
  IntoWire &loopedShards(const SHVar &var) {
    assert(var.valueType == SHType::Seq);
    const SHSeq &seq = var.payload.seqValue;
    assert(seq.len == 0 || seq.elements[0].valueType == SHType::ShardRef);
    auto wire = result = SHWire::make("looped-shards");
    wire->looped = true;
    ForEach(seq, [&](SHVar &v) {
      assert(v.valueType == SHType::ShardRef);
      wire->addShard(v.payload.shardValue);
    });
    return *this;
  }

  // Adds a wire
  IntoWire &wire(const SHVar &var) {
    assert(var.valueType == SHType::Wire);
    result = *(std::shared_ptr<SHWire> *)var.payload.wireValue;
    return *this;
  }
};

// Converts either a Wire/ShardsSeq/[ShardsSeq]/[Wires] into a vector of std::shared_ptr<SHWire>
struct IntoWires {
  static inline shards::Types RunnableTypes{CoreInfo::NoneType, CoreInfo::WireType, Type::SeqOf(CoreInfo::WireType),
                                            CoreInfo::ShardRefSeqType, Type::SeqOf(CoreInfo::ShardRefSeqType)};

  std::vector<std::shared_ptr<SHWire>> &results;
  IntoWires(std::vector<std::shared_ptr<SHWire>> &results) : results(results) {}

  // Sets the runnables (wires or shards)
  void runnables(const SHVar &var) {
    if (var.valueType != SHType::None) {
      if (var.valueType == SHType::Wire) {
        results.push_back(IntoWire{}.wire(var));
      } else {
        assert(var.valueType == SHType::Seq);
        auto &seq = var.payload.seqValue;
        if (seq.len == 0)
          return;

        if (seq.elements[0].valueType == SHType::ShardRef) {
          // Single sequence of shards
          results.push_back(IntoWire{}.loopedShards(var));
        } else {
          // Multiple wires or sequences of shards
          for (uint32_t i = 0; i < seq.len; i++) {
            SHVar &v = seq.elements[i];
            results.push_back(IntoWire{}.runnable(v));
          }
        }
      }
    }
  }
};
} // namespace shards

#endif /* C81B8E95_5381_4841_8931_F7EE0CB8EEEA */
