/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2020 Giovanni Petrantoni */

#include "shared.hpp"
#include <atomic>
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/stack.hpp>
#include <mutex>
#include <variant>

namespace chainblocks {
namespace channels {
struct MPMCChannel {
  boost::lockfree::queue<CBVar> data;
  boost::lockfree::stack<CBVar> recycle;
};

struct BroadcastChannel {
  // like a water flow/pipe, no real ownership
  // add water, drink water
  struct Box {
    CBVar *var;
    uint64_t version;
  };
  std::atomic<Box> data;
};

using Channel = std::variant<MPMCChannel, BroadcastChannel>;

class Globals {
private:
  static inline std::mutex ChannelsMutex;
  static inline std::unordered_map<std::string, Channel> Channels;

public:
  Channel &get(const std::string &name) {
    std::unique_lock<std::mutex> lock(ChannelsMutex);
    return Channels[name];
  }
};
} // namespace channels
} // namespace chainblocks
