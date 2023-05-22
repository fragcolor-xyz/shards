/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

#include <shards/core/shared.hpp>

namespace shards {
namespace reflection {
struct Shards {
  SHOptionalString help() {
    return SHCCSTR("Given a wire as input it will recurse deep inside it and gather all "
                   "shards in the wire, its sub-wires and sub-flows.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::WireType; }
  static SHTypesInfo outputTypes() { return CoreInfo::ShardRefSeqType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    std::vector<ShardInfo> shards;
    _output.clear();
    auto wire = SHWire::sharedFromRef(input.payload.wireValue);
    gatherShards(wire.get(), shards);
    for (auto &item : shards) {
      _output.emplace_back(const_cast<Shard *>(item.shard));
    }
    return Var(_output);
  }

  std::vector<Var> _output;
};

struct Name {
  static SHTypesInfo inputTypes() { return CoreInfo::ShardRefType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }
  SHVar activate(SHContext *context, const SHVar &input) {
    Shard *blk = input.payload.shardValue;
    _name.assign(blk->name(blk));
    return Var(_name);
  }
  std::string _name;
};
} // namespace reflection
} // namespace shards
SHARDS_REGISTER_FN(reflection) {
  REGISTER_SHARD("Reflect.Shards", shards::reflection::Shards);
  REGISTER_SHARD("Reflect.Name", shards::reflection::Name);
}