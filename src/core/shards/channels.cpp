/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

#include "shared.hpp"
#include <atomic>
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/stack.hpp>
#include <mutex>
#include <variant>

namespace shards {
namespace channels {

struct ChannelShared {
  SHTypeInfo type;
  std::atomic_bool closed;
};

struct DummyChannel : public ChannelShared {};

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
    std::scoped_lock<std::mutex> lock(submutex);
    return subscribers.emplace_back(_noCopy);
  }

protected:
  friend struct Broadcast;
  std::mutex submutex;
  std::list<MPMCChannel> subscribers;
  bool _noCopy = false;
};

using Channel = std::variant<DummyChannel, MPMCChannel, BroadcastChannel>;

class Globals {
private:
  static inline std::mutex ChannelsMutex;
  static inline std::unordered_map<std::string, Channel> Channels;

public:
  static Channel &get(const std::string &name) {
    std::scoped_lock<std::mutex> lock(ChannelsMutex);
    return Channels[name];
  }
};

struct Base {
  Channel *_channel = nullptr;
  std::string _name;
  bool _noCopy = false;

  static inline Parameters producerParams{{"Name", SHCCSTR("The name of the channel."), {CoreInfo::StringType}},
                                          {"NoCopy!!",
                                           SHCCSTR("Unsafe flag that will improve performance by not copying "
                                                   "values when sending them thru the channel."),
                                           {CoreInfo::BoolType}}};

  static inline Parameters consumerParams{
      {"Name", SHCCSTR("The name of the channel."), {CoreInfo::StringType}},
      {"Buffer", SHCCSTR("The amount of values to buffer before outputting them."), {CoreInfo::IntType}}};

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0: {
      _name = value.payload.stringValue;
    } break;
    case 1: {
      _noCopy = value.payload.boolValue;
    } break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_name);
    case 1:
      return Var(_noCopy);
    }
    return SHVar();
  }

  template <typename T> void verifyInputType(T &channel, const SHInstanceData &data) {
    if (channel.type.basicType != SHType::None && channel.type != data.inputType) {
      throw SHException("Produce attempted to change produced type: " + _name);
    }
  }
};

struct Produce : public Base {
  MPMCChannel *_mpchannel;

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static SHParametersInfo parameters() { return producerParams; }

  SHTypeInfo compose(const SHInstanceData &data) {
    auto &vchannel = Globals::get(_name);
    switch (vchannel.index()) {
    case 0: {
      vchannel.emplace<MPMCChannel>(_noCopy);
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
      throw SHException("Produce/Consume channel type expected.");
    }
    return data.inputType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    assert(_mpchannel);

    SHVar tmp{};

    // try to get from recycle bin
    // this might fail but we don't care//
    // if it fails will just allocate a brand new
    _mpchannel->recycle.pop(tmp);

    if (_noCopy) {
      tmp = input;
    } else {
      // this internally will reuse memory
      // yet it can be still slow for big vars
      cloneVar(tmp, input);
    }

    // enqueue for the stealing
    _mpchannel->data.push(tmp);

    return input;
  }
};

struct Broadcast : public Base {
  BroadcastChannel *_mpchannel;

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static SHParametersInfo parameters() { return producerParams; }

  SHTypeInfo compose(const SHInstanceData &data) {
    auto &vchannel = Globals::get(_name);
    switch (vchannel.index()) {
    case 0: {
      SHLOG_TRACE("Creating broadcast channel: {}", _name);

      vchannel.emplace<BroadcastChannel>(_noCopy);
      auto &channel = std::get<BroadcastChannel>(vchannel);
      // no cloning here, this is potentially dangerous if the type is dynamic
      channel.type = data.inputType;
      _mpchannel = &channel;
    } break;
    case 2: {
      SHLOG_TRACE("Subscribing to broadcast channel: {}", _name);

      auto &channel = std::get<BroadcastChannel>(vchannel);
      verifyInputType(channel, data);
      _mpchannel = &channel;
    } break;
    default:
      throw SHException("Broadcast: channel type expected.");
    }
    return data.inputType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    assert(_mpchannel);

    // we need to support subscriptions during run-time
    // so we need to lock this operation
    // furthermore we allow multiple broadcasters so the erase needs this
    std::scoped_lock<std::mutex> lock(_mpchannel->submutex);
    for (auto it = _mpchannel->subscribers.begin(); it != _mpchannel->subscribers.end();) {
      if (it->closed) {
        it = _mpchannel->subscribers.erase(it);
      } else {
        SHVar tmp{};

        // try to get from recycle bin
        // this might fail but we don't care//
        // if it fails will just allocate a brand new
        it->recycle.pop(tmp);

        if (_noCopy) {
          tmp = input;
        } else {
          // this internally will reuse memory
          cloneVar(tmp, input);
        }

        // enqueue for the stealing
        it->data.push(tmp);

        ++it;
      }
    }

    return input;
  }
};

struct BufferedConsumer {
  // utility to recycle memory and buffer
  // recycling is only for non blittable types basically
  std::vector<SHVar> buffer;

  void recycle(MPMCChannel *channel) {
    // send previous values to recycle
    for (auto &var : buffer) {
      channel->recycle.push(var);
    }
    buffer.clear();
  }

  void add(SHVar &var) { buffer.push_back(var); }

  bool empty() { return buffer.size() == 0; }

  operator SHVar() {
    auto len = buffer.size();
    assert(len > 0);
    if (len > 1) {
      SHVar res{};
      res.valueType = Seq;
      res.payload.seqValue.elements = &buffer[0];
      res.payload.seqValue.len = buffer.size();
      return res;
    } else {
      return buffer[0];
    }
  }
};

struct Consumers : public Base {
  MPMCChannel *_mpchannel;
  BufferedConsumer _storage;
  int64_t _bufferSize = 1;
  int64_t _current = 1;
  SHTypeInfo _outType{};
  SHTypeInfo _seqType{};

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static SHParametersInfo parameters() { return consumerParams; }

  void setParam(int index, const SHVar &value) {
    if (index == 0)
      Base::setParam(index, value);
    else
      _bufferSize = value.payload.intValue;
  }

  SHVar getParam(int index) {
    if (index == 0)
      return Base::getParam(index);
    else
      return Var(_bufferSize);
  }

  void cleanup() {
    // reset buffer counter
    _current = _bufferSize;
    // cleanup storage
    if (_mpchannel)
      _storage.recycle(_mpchannel);
  }
};

struct Consume : public Consumers {
  SHTypeInfo compose(const SHInstanceData &data) {
    auto &vchannel = Globals::get(_name);
    switch (vchannel.index()) {
    case 1: {
      auto &channel = std::get<MPMCChannel>(vchannel);
      _mpchannel = &channel;
      _outType = channel.type;
      if (_bufferSize == 1) {
        return _outType;
      } else {
        _seqType.basicType = Seq;
        _seqType.seqTypes.elements = &_outType;
        _seqType.seqTypes.len = 1;
        return _seqType;
      }
    };
    default:
      throw SHException("Produce/Consume channel type expected.");
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    assert(_mpchannel);

    // send previous values to recycle
    _storage.recycle(_mpchannel);

    // reset buffer
    _current = _bufferSize;

    // everytime we are scheduled we try to pop a value
    while (_current--) {
      SHVar output{};
      while (!_mpchannel->data.pop(output)) {
        // check also for channel completion
        if (_mpchannel->closed) {
          if (!_storage.empty()) {
            return _storage;
          } else {
            context->stopFlow(Var::Empty);
            return Var::Empty;
          }
        }
        SH_SUSPEND(context, 0);
      }

      // keep for recycling
      _storage.add(output);
    }

    return _storage;
  }
};

struct Listen : public Consumers {
  BroadcastChannel *_bchannel;

  void destroy() {
    if (_mpchannel) {
      _mpchannel->closed = true;
      // also try clear here, to make broadcast removal faster
      SHVar tmp{};
      while (_mpchannel->data.pop(tmp)) {
        destroyVar(tmp);
      }
      while (_mpchannel->recycle.pop(tmp)) {
        destroyVar(tmp);
      }
    }
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    auto &vchannel = Globals::get(_name);
    switch (vchannel.index()) {
    case 2: {
      SHLOG_TRACE("Listening broadcast channel: {}", _name);

      auto &channel = std::get<BroadcastChannel>(vchannel);
      auto &sub = channel.subscribe();
      _bchannel = &channel;
      _mpchannel = &sub;
      _outType = channel.type;
      if (_bufferSize == 1) {
        return _outType;
      } else {
        _seqType.basicType = Seq;
        _seqType.seqTypes.elements = &_outType;
        _seqType.seqTypes.len = 1;
        return _seqType;
      }
    };
    default:
      throw SHException("Listen: channel type expected.");
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    assert(_bchannel);
    assert(_mpchannel);

    // send previous values to recycle
    _storage.recycle(_mpchannel);

    // reset buffer
    _current = _bufferSize;

    // everytime we are scheduled we try to pop a value
    while (_current--) {
      SHVar output{};
      while (!_mpchannel->data.pop(output)) {
        // check also for channel completion
        if (_bchannel->closed) {
          if (!_storage.empty()) {
            return _storage;
          } else {
            context->stopFlow(Var::Empty);
            return Var::Empty;
          }
        }
        SH_SUSPEND(context, 0);
      }

      // keep for recycling
      _storage.add(output);
    }

    return _storage;
  }
};

struct Complete : public Base {
  ChannelShared *_mpchannel;

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static SHParametersInfo parameters() { return consumerParams; }

  SHTypeInfo compose(const SHInstanceData &data) {
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
      throw SHException("Expected a valid channel.");
    }
    return data.inputType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    assert(_mpchannel);

    if (_mpchannel->closed.exchange(true)) {
      SHLOG_INFO("Complete called on an already closed channel: {}", _name);
    }

    return input;
  }
};

void registerShards() {
  REGISTER_SHARD("Produce", Produce);
  REGISTER_SHARD("Broadcast", Broadcast);
  REGISTER_SHARD("Consume", Consume);
  REGISTER_SHARD("Listen", Listen);
  REGISTER_SHARD("Complete", Complete);
}
} // namespace channels
} // namespace shards
