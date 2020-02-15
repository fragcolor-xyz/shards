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

struct ChannelShared {
  CBTypeInfo type;
  std::atomic_bool closed;
};

struct DummyChannel : public ChannelShared {};

struct MPMCChannel : public ChannelShared {
  // A single source to seal data from
  boost::lockfree::queue<CBVar> data{16};
  boost::lockfree::stack<CBVar> recycle{16};
};

struct BroadcastChannel : public ChannelShared {
  // ideally like a water flow/pipe, no real ownership
  // add water, drink water
  struct Box {
    CBVar *var;
    uint64_t version;
  };
  std::atomic<Box> data;
};

using Channel = std::variant<DummyChannel, MPMCChannel, BroadcastChannel>;

class Globals {
private:
  static inline std::mutex ChannelsMutex;
  static inline std::unordered_map<std::string, Channel> Channels;

public:
  static Channel &get(const std::string &name) {
    std::unique_lock<std::mutex> lock(ChannelsMutex);
    return Channels[name];
  }
};

struct Base {
  Channel *_channel = nullptr;
  std::string _name;

  template <typename T>
  void verifyInputType(T &channel, const CBInstanceData &data) {
    if (channel.type.basicType != CBType::None &&
        channel.type != data.inputType) {
      throw CBException("Produce attempted to change produced type: " + _name);
    }
  }
};

struct Produce : public Base {
  MPMCChannel *_mpchannel;

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  CBTypeInfo compose(const CBInstanceData &data) {
    auto &vchannel = Globals::get(_name);
    switch (vchannel.index()) {
    case 0: {
      vchannel.emplace<MPMCChannel>();
      auto &channel = std::get<MPMCChannel>(vchannel);
      // no cloning here, this is potentially dangerous if the type is dynamic
      channel.type = data.inputType;
      _mpchannel = &channel;
    } break;
    case 1: {
      auto &channel = std::get<MPMCChannel>(vchannel);
      verifyInputType(channel, data);
      _mpchannel = &channel;
    } break;
    default:
      throw CBException("Produce/Consume channel type expected.");
    }
    return data.inputType;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    assert(_mpchannel);

    CBVar tmp{};

    // try to get from recycle bin
    // this might fail but we don't care//
    // if it fails will just allocate a brand new
    _mpchannel->recycle.pop(tmp);

    // this internally will reuse memory
    cloneVar(tmp, input);

    // enqueue for the stealing
    _mpchannel->data.push(tmp);

    return input;
  }
};

struct Consume : public Base {
  MPMCChannel *_mpchannel;
  CBVar _output{};
  bool acquired = false;

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  CBTypeInfo compose(const CBInstanceData &data) {
    auto &vchannel = Globals::get(_name);
    switch (vchannel.index()) {
    case 1: {
      auto &channel = std::get<MPMCChannel>(vchannel);
      _mpchannel = &channel;
      return channel.type;
    } break;
    default:
      throw CBException("Produce/Consume channel type expected.");
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    assert(_mpchannel);

    // send previous value to recycler
    if (acquired)
      _mpchannel->recycle.push(_output);

    // everytime we are scheduled we try to pop a value
    while (!_mpchannel->data.pop(_output)) {
      // check also for channel completion
      if (_mpchannel->closed) {
        return StopChain;
      }
      cbpause(0.0);
    }

    acquired = true;

    return _output;
  }
};

struct Complete : public Base {
  ChannelShared *_mpchannel;

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  CBTypeInfo compose(const CBInstanceData &data) {
    auto &vchannel = Globals::get(_name);
    switch (vchannel.index()) {
    case 1: {
      auto &channel = std::get<MPMCChannel>(vchannel);
      _mpchannel = &channel;
    } break;
    case 2: {
      auto &channel = std::get<BroadcastChannel>(vchannel);
      _mpchannel = &channel;
    } break;
    default:
      throw CBException("Expected a valid channel.");
    }
    return data.inputType;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    assert(_mpchannel);

    if (_mpchannel->closed.exchange(true)) {
      LOG(INFO) << "Complete called on an already closed channel: " << _name;
    }

    return input;
  }
};

typedef BlockWrapper<Produce> ProduceBlock;
typedef BlockWrapper<Consume> ConsumeBlock;
typedef BlockWrapper<Complete> CompleteBlock;

void registerBlocks() {
  registerBlock("Produce", &ProduceBlock::create);
  registerBlock("Consume", &ConsumeBlock::create);
  registerBlock("Complete", &CompleteBlock::create);
}
} // namespace channels
} // namespace chainblocks
