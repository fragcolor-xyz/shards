#ifndef C81B8E95_5381_4841_8931_F7EE0CB8EEEA
#define C81B8E95_5381_4841_8931_F7EE0CB8EEEA

#include <shards/common_types.hpp>
#include <shards/shards.hpp>

namespace shards {

// Converts either a Wire/ShardsSeq into a std::shared_ptr<SHWire>
struct IntoWire {
  static inline shards::Types RunnableTypes{CoreInfo::NoneType, CoreInfo::WireType, CoreInfo::ShardRefSeqType};

private:
  std::shared_ptr<SHWire> result;
  bool defaultLooped_ = true;
  const char *defaultWireName_ = "inline-wire";

public:
  // Adds a single wire or sequence of shards as a looped wire
  IntoWire &var(const SHVar &var) {
    if (var.valueType != SHType::None) {
      if (var.valueType == SHType::Wire) {
        wire(var);
      } else {
        shards(var);
      }
    } else {
      result.reset();
    }
    return *this;
  }

  // To make inline shards a looped or non-looped wire
  IntoWire &defaultLooped(bool looped) {
    defaultLooped_ = looped;
    return *this;
  }

  // The wire name to give inline shards
  IntoWire &defaultWireName(const char *name) {
    defaultWireName_ = name;
    return *this;
  }

  operator std::shared_ptr<SHWire> &() { return result; }

private:
  friend struct IntoWires;

  // Add a sequence of shards as a wire
  IntoWire &shards(const SHVar &var) {
    shassert(var.valueType == SHType::Seq);
    const SHSeq &seq = var.payload.seqValue;
    shassert(seq.len == 0 || seq.elements[0].valueType == SHType::ShardRef);
    auto wire = result = SHWire::make(defaultWireName_);
    wire->looped = defaultLooped_;
    ForEach(seq, [&](SHVar &v) {
      shassert(v.valueType == SHType::ShardRef);
      wire->addShard(v.payload.shardValue);
    });
    return *this;
  }

  // Adds a wire
  IntoWire &wire(const SHVar &var) {
    shassert(var.valueType == SHType::Wire);
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
  IntoWires &var(const SHVar &var, IntoWire intoWire = IntoWire()) {
    if (var.valueType != SHType::None) {
      if (var.valueType == SHType::Wire) {
        results.push_back(intoWire.wire(var));
      } else {
        shassert(var.valueType == SHType::Seq);
        auto &seq = var.payload.seqValue;
        if (seq.len == 0)
          return *this;

        if (seq.elements[0].valueType == SHType::ShardRef) {
          // Single sequence of shards
          results.push_back(intoWire.shards(var));
        } else {
          // Multiple wires or sequences of shards
          for (uint32_t i = 0; i < seq.len; i++) {
            SHVar &v = seq.elements[i];
            results.push_back(intoWire.var(v));
          }
        }
      }
    }
    return *this;
  }
};
} // namespace shards

#endif /* C81B8E95_5381_4841_8931_F7EE0CB8EEEA */
