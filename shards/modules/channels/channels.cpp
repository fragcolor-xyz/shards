/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

#include "channels.hpp"
#include <shards/core/runtime.hpp>
#include <memory>
#include <mutex>
#include <shared_mutex>

namespace shards {
namespace channels {
struct Base {
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
  std::shared_ptr<Channel> _channel;
  MPMCChannel *_mpChannel;

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static SHParametersInfo parameters() { return producerParams; }

  SHTypeInfo compose(const SHInstanceData &data) {
    _channel = get(_name);
    switch (_channel->index()) {
    case 0: {
      _channel->emplace<MPMCChannel>(_noCopy);
      auto &channel = std::get<MPMCChannel>(*_channel);
      // no cloning here, this is potentially dangerous if the type is dynamic
      channel.type = data.inputType;
      _mpChannel = &channel;
    } break;
    case 1: {
      auto &channel = std::get<MPMCChannel>(*_channel);
      verifyInputType(channel, data);
      _mpChannel = &channel;
    } break;
    default:
      _channel.reset();
      SHLOG_ERROR("Channel type expected for channel {}.", _name);
      throw SHException("Produce/Consume channel type expected.");
    }
    return data.inputType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    assert(_mpChannel);

    SHVar tmp{};

    // try to get from recycle bin
    // this might fail but we don't care
    // if it fails will just initialize a new variable
    _mpChannel->recycle.pop(tmp);

    if (_noCopy) {
      tmp = input;
    } else {
      // this internally will reuse memory
      // yet it can be still slow for big vars
      cloneVar(tmp, input);
    }

    // enqueue for the stealing
    _mpChannel->data.push(tmp);

    return input;
  }
};

struct Broadcast : public Base {
  std::shared_ptr<Channel> _channel;
  BroadcastChannel *_mpChannel;

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static SHParametersInfo parameters() { return producerParams; }

  SHTypeInfo compose(const SHInstanceData &data) {
    _channel = get(_name);
    switch (_channel->index()) {
    case 0: {
      SHLOG_TRACE("Creating broadcast channel: {}", _name);

      _channel->emplace<BroadcastChannel>(_noCopy);
      auto &channel = std::get<BroadcastChannel>(*_channel);
      // no cloning here, this is potentially dangerous if the type is dynamic
      channel.type = data.inputType;
      _mpChannel = &channel;
    } break;
    case 2: {
      SHLOG_TRACE("Subscribing to broadcast channel: {}", _name);

      auto &channel = std::get<BroadcastChannel>(*_channel);
      verifyInputType(channel, data);
      _mpChannel = &channel;
    } break;
    default:
      _channel.reset();
      throw SHException("Broadcast: channel type expected.");
    }
    return data.inputType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    assert(_mpChannel);

    // we need to support subscriptions during run-time
    // so we need to lock this operation
    // furthermore we allow multiple broadcasters so the erase needs this
    // but we use suspend instead of kernel locking!

    std::unique_lock<std::mutex> lock(_mpChannel->subMutex, std::defer_lock);

    // try to lock, if we can't we suspend
    while (!lock.try_lock()) {
      SH_SUSPEND(context, 0);
    }

    // we locked it!
    for (auto it = _mpChannel->subscribers.begin(); it != _mpChannel->subscribers.end();) {
      if (it->closed) {
        it = _mpChannel->subscribers.erase(it);
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

    // we are done, exiting and going out of scope will unlock

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
      res.valueType = SHType::Seq;
      res.payload.seqValue.elements = &buffer[0];
      res.payload.seqValue.len = uint32_t(buffer.size());
      return res;
    } else {
      return buffer[0];
    }
  }
};

struct Consumers : public Base {
  std::shared_ptr<Channel> _channel;
  MPMCChannel *_mpChannel;
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
    if (_mpChannel)
      _storage.recycle(_mpChannel);
  }
};

struct Consume : public Consumers {
  SHTypeInfo compose(const SHInstanceData &data) {
    _channel = get(_name);
    switch (_channel->index()) {
    case 1: {
      auto &channel = std::get<MPMCChannel>(*_channel);
      _mpChannel = &channel;
      _outType = channel.type;
      if (_bufferSize == 1) {
        return _outType;
      } else {
        _seqType.basicType = SHType::Seq;
        _seqType.seqTypes.elements = &_outType;
        _seqType.seqTypes.len = 1;
        return _seqType;
      }
    };
    default:
      _channel.reset();
      SHLOG_ERROR("Channel type expected for channel {}.", _name);
      throw SHException("Produce/Consume channel type expected.");
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    assert(_mpChannel);

    // send previous values to recycle
    _storage.recycle(_mpChannel);

    // reset buffer
    _current = _bufferSize;

    // blocking; and
    // everytime we are resumed we try to pop a value
    while (_current--) {
      SHVar output{};
      while (!_mpChannel->data.pop(output)) {
        // check also for channel completion
        if (_mpChannel->closed) {
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
  BroadcastChannel *_bChannel;

  void destroy() {
    if (_mpChannel) {
      _mpChannel->closed = true;
    }
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    _channel = get(_name);
    switch (_channel->index()) {
    case 2: {
      SHLOG_TRACE("Listening broadcast channel: {}", _name);

      auto &channel = std::get<BroadcastChannel>(*_channel);
      auto &sub = channel.subscribe();
      _bChannel = &channel;
      _mpChannel = &sub;
      _outType = channel.type;
      if (_bufferSize == 1) {
        return _outType;
      } else {
        _seqType.basicType = SHType::Seq;
        _seqType.seqTypes.elements = &_outType;
        _seqType.seqTypes.len = 1;
        return _seqType;
      }
    };
    default:
      _channel.reset();
      throw SHException("Listen: channel type expected.");
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    assert(_bChannel);
    assert(_mpChannel);

    // send previous values to recycle
    _storage.recycle(_mpChannel);

    // reset buffer
    _current = _bufferSize;

    // suspending; and
    // everytime we are resumed we try to pop a value
    while (_current--) {
      SHVar output{};
      while (!_mpChannel->data.pop(output)) {
        // check also for channel completion
        if (_bChannel->closed) {
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
  std::shared_ptr<Channel> _channel;
  ChannelShared *_mpChannel;

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static SHParametersInfo parameters() { return consumerParams; }

  SHTypeInfo compose(const SHInstanceData &data) {
    _channel = get(_name);
    switch (_channel->index()) {
    case 1: {
      auto &channel = std::get<MPMCChannel>(*_channel);
      _mpChannel = &channel;
    } break;
    case 2: {
      auto &channel = std::get<BroadcastChannel>(*_channel);
      _mpChannel = &channel;
    } break;
    default:
      _channel.reset();
      throw SHException("Expected a valid channel.");
    }
    return data.inputType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    assert(_mpChannel);

    if (_mpChannel->closed.exchange(true)) {
      SHLOG_INFO("Complete called on an already closed channel: {}", _name);
    }

    return input;
  }
};

std::shared_ptr<Channel> get(const std::string &name) {
  static std::unordered_map<std::string, std::weak_ptr<Channel>> channels;
  static std::shared_mutex mutex;

  std::shared_lock<decltype(mutex)> _l(mutex);
  auto it = channels.find(name);
  if (it == channels.end()) {
    _l.unlock();
    std::scoped_lock<decltype(mutex)> _l1(mutex);
    auto sp = std::make_shared<Channel>();
    channels[name] = sp;
    return sp;
  } else {
    std::shared_ptr<Channel> sp = it->second.lock();
    if (!sp) {
      _l.unlock();
      std::scoped_lock<decltype(mutex)> _l1(mutex);
      sp = std::make_shared<Channel>();
      channels[name] = sp;
    }
    return sp;
  }
}
} // namespace channels
} // namespace shards
SHARDS_REGISTER_FN(channels) {
  using namespace shards::channels;
  REGISTER_SHARD("Produce", Produce);
  REGISTER_SHARD("Broadcast", Broadcast);
  REGISTER_SHARD("Consume", Consume);
  REGISTER_SHARD("Listen", Listen);
  REGISTER_SHARD("Complete", Complete);
}
