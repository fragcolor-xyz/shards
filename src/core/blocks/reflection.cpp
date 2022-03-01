/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

#include "shared.hpp"

namespace chainblocks {
namespace reflection {
struct Blocks {
  CBOptionalString help() {
    return CBCCSTR("Given a chain as input it will recurse deep inside it and gather all "
                   "blocks in the chain, its sub-chains and sub-flows.");
  }

  static CBTypesInfo inputTypes() { return CoreInfo::ChainType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BlockSeqType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    std::vector<CBlockInfo> blocks;
    _output.clear();
    auto chain = CBChain::sharedFromRef(input.payload.chainValue);
    gatherBlocks(chain.get(), blocks);
    for (auto &item : blocks) {
      _output.emplace_back(const_cast<CBlock *>(item.block));
    }
    return Var(_output);
  }

  std::vector<Var> _output;
};

struct Name {
  static CBTypesInfo inputTypes() { return CoreInfo::BlockType; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }
  CBVar activate(CBContext *context, const CBVar &input) {
    CBlock *blk = input.payload.blockValue;
    _name.assign(blk->name(blk));
    return Var(_name);
  }
  std::string _name;
};

void registerBlocks() {
  REGISTER_CBLOCK("Reflect.Blocks", Blocks);
  REGISTER_CBLOCK("Reflect.Name", Name);
}
} // namespace reflection
} // namespace chainblocks