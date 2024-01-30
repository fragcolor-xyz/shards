#ifndef SH_CORE_SHARDS_CHANNELS
#define SH_CORE_SHARDS_CHANNELS

#include <shards/core/shared.hpp>
#include <atomic>
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/stack.hpp>
#include <memory>
#include <mutex>
#include <variant>

namespace shards {
namespace channels {

struct ChannelShared {
  SHTypeInfo type;
  std::atomic_bool closed;

  virtual void clear() = 0;
};

struct DummyChannel : public ChannelShared {
  virtual void clear() override {}
};

struct MPMCChannel : public ChannelShared {
  MPMCChannel(bool noCopy) : ChannelShared(), _noCopy(noCopy) {}

  // no real cleanups happens in Produce/Consume to keep things simple
  // and without locks
  ~MPMCChannel() {
    if (!_noCopy) {
      SHVar tmp{};
      while (data.pop(tmp)) {
        destroyVar(tmp);
      }
      while (recycle.pop(tmp)) {
        destroyVar(tmp);
      }
    }
  }

  virtual void clear() override {
    SHVar tmp{};
    while (data.pop(tmp)) {
      destroyVar(tmp);
    }
    while (recycle.pop(tmp)) {
      destroyVar(tmp);
    }
  }

  // A single source to steal data from
  boost::lockfree::queue<SHVar> data{16};
  boost::lockfree::stack<SHVar> recycle{16};

private:
  bool _noCopy;
};

struct Broadcast;
class BroadcastChannel : public ChannelShared {
public:
  BroadcastChannel(bool noCopy) : ChannelShared(), _noCopy(noCopy) {}

  MPMCChannel &subscribe() {
    // we automatically cleanup based on the closed flag of the inner channel
    std::unique_lock<std::mutex> lock(subMutex);
    return subscribers.emplace_back(_noCopy);
  }

  virtual void clear() override {
    std::unique_lock<std::mutex> lock(subMutex);
    for (auto &sub : subscribers) {
      sub.clear();
    }
    subscribers.clear();
  }

protected:
  friend struct Broadcast;
  std::mutex subMutex;
  std::list<MPMCChannel> subscribers;
  bool _noCopy = false;
};

using Channel = std::variant<DummyChannel, MPMCChannel, BroadcastChannel>;
std::shared_ptr<Channel> get(const std::string &name);

} // namespace channels
} // namespace shards

#endif