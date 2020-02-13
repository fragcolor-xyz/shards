/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2020 Giovanni Petrantoni */

#include "shared.hpp"
#include <atomic>
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/stack.hpp>
#include <mutex>
#include <shared_ptr>
#include <variant>

namespace chainblocks {
namespace channels {
struct MPMCChannel {
  boost::lockfree::queue<CBVar> data;
  boost::lockfree::stack<CBVar> recycle;
};

template <typename T> struct BroadcastChannel {
  std::atomic<T> data;
  std::atomic<uint64_t> version;
};

// During validation let's be smart about memory efficient types
using Channel = std::variant<MPMCChannel, BroadcastChannel<CBVar>,
                             BroadcastChannel<std::shared_ptr<OwnedVar>>>;

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
