/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#ifndef CB_BLOCKSCHAIN_HPP
#define CB_BLOCKSCHAIN_HPP

#include "runtime.hpp"

namespace chainblocks {
class Chain {
public:
  template <typename String>
  Chain(String name) : _name(name), _looped(false), _unsafe(false) {}
  Chain() : _looped(false), _unsafe(false) {}

  template <typename String, typename... Vars>
  Chain &block(String name, Vars... params) {
    std::string blockName(name);
    auto blk = createBlock(blockName.c_str());
    blk->setup(blk);

    std::vector<Var> vars = {Var(params)...};
    for (size_t i = 0; i < vars.size(); i++) {
      blk->setParam(blk, i, vars[i]);
    }

    _blocks.push_back(blk);
    return *this;
  }

  Chain &looped(bool looped) {
    _looped = looped;
    return *this;
  }

  Chain &unsafe(bool unsafe) {
    _unsafe = unsafe;
    return *this;
  }

  template <typename String> Chain &name(String name) {
    _name = name;
    return *this;
  }

  operator CBChain *() {
    auto chain = new CBChain(_name.c_str());
    chain->looped = _looped;
    for (auto blk : _blocks) {
      chain->addBlock(blk);
    }
    // blocks are unique so drain them here
    _blocks.clear();
    return chain;
  }

  operator CBVar() {
    CBVar res{};
    res.valueType = Seq;
    for (auto blk : _blocks) {
      CBVar blkVar{};
      blkVar.valueType = Block;
      blkVar.payload.blockValue = blk;
      stbds_arrpush(res.payload.seqValue, blkVar);
    }
    // blocks are unique so drain them here
    _blocks.clear();
    return res;
  }

private:
  std::string _name;
  bool _looped;
  bool _unsafe;
  std::vector<CBlock *> _blocks;
};

void blockschain_test() {
  auto chain1 = std::unique_ptr<CBChain>(Chain("test-chain")
                                             .looped(true)
                                             .block("Const", 1)
                                             .block("Log")
                                             .block("Math.Add", 2)
                                             .block("Assert.Is", 3, true));
  assert(chain1->blocks.size() == 4);
}
} // namespace chainblocks

#endif
