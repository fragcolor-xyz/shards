/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#ifndef CB_BLOCKSCHAIN_HPP
#define CB_BLOCKSCHAIN_HPP

#include "runtime.hpp"

namespace chainblocks {
class Blockvar {
public:
  Blockvar() {
    // None
  }
};

class Blockschain {
public:
  Blockschain(std::string name) : _name(name), _looped(false) {}

  template <typename... Vars>
  Blockschain &block(std::string name, Vars... params) {
    auto blk = createBlock(name.c_str());
    _blocks.push_back(blk);
    return *this;
  }

  Blockschain &looped(bool isLooped) {
    _looped = isLooped;
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
  std::vector<CBlock *> _blocks;
};

void blockschain_test() {
  auto chain1 = std::unique_ptr<CBChain>(Blockschain("test-chain")
                                             .looped(true)
                                             .block("Const", 1)
                                             .block("Log")
                                             .block("Math.Add", 2)
                                             .block("Assert.Is", 3, true));
  assert(chain1->blocks.size() == 4);
}
} // namespace chainblocks

#endif
