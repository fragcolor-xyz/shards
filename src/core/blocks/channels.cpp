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
  MPMCChannel(bool noCopy) : ChannelShared(), _noCopy(noCopy) {}

  // no real cleanups happens in Produce/Consume to keep things simple
  // and without locks
  ~MPMCChannel() {
    if (!_noCopy) {
      CBVar tmp{};
      while (data.pop(tmp)) {
        destroyVar(tmp);
      }
      while (recycle.pop(tmp)) {
        destroyVar(tmp);
      }
    }
  }

  // A single source to steal data from
  boost::lockfree::queue<CBVar> data{16};
  boost::lockfree::stack<CBVar> recycle{16};

private:
  bool _noCopy;
};

struct Broadcast;
class BroadcastChannel : public ChannelShared {
public:
  BroadcastChannel(bool noCopy) : ChannelShared(), _noCopy(noCopy) {}

  MPMCChannel &subscribe() {
    // we automatically cleanup based on the closed flag of the inner channel
    std::unique_lock<std::mutex> lock(submutex);
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
    std::unique_lock<std::mutex> lock(ChannelsMutex);
    return Channels[name];
  }
};

struct Base {
  Channel *_channel = nullptr;
  std::string _name;
  bool _noCopy = false;

  static inline Parameters producerParams{
      {"Name", "The name of the channel.", {CoreInfo::StringType}},
      {"NoCopy!!",
       "Unsafe flag that will improve performance by not copying values when "
       "sending them thru the channel.",
       {CoreInfo::BoolType}}};

  static inline Parameters consumerParams{
      {"Name", "The name of the channel.", {CoreInfo::StringType}},
      {"Buffer",
       "The amount of values to buffer before outputting them.",
       {CoreInfo::IntType}}};

  void setParam(int index, CBVar value) {
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

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_name);
    case 1:
      return Var(_noCopy);
    }
    return CBVar();
  }

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

  static CBParametersInfo parameters() { return producerParams; }

  CBTypeInfo compose(const CBInstanceData &data) {
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

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static CBParametersInfo parameters() { return producerParams; }

  CBTypeInfo compose(const CBInstanceData &data) {
    auto &vchannel = Globals::get(_name);
    switch (vchannel.index()) {
    case 0: {
      vchannel.emplace<BroadcastChannel>(_noCopy);
      auto &channel = std::get<BroadcastChannel>(vchannel);
      // no cloning here, this is potentially dangerous if the type is dynamic
      channel.type = data.inputType;
      _mpchannel = &channel;
    } break;
    case 2: {
      auto &channel = std::get<BroadcastChannel>(vchannel);
      verifyInputType(channel, data);
      _mpchannel = &channel;
    } break;
    default:
      throw CBException("Broadcast/Listen channel type expected.");
    }
    return data.inputType;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    assert(_mpchannel);

    // we need to support subscriptions during run-time
    // so we need to lock this operation
    // furthermore we allow multiple broadcasters so the erase needs this
    std::unique_lock<std::mutex> lock(_mpchannel->submutex);
    for (auto it = _mpchannel->subscribers.begin();
         it != _mpchannel->subscribers.end();) {
      if (it->closed) {
        it = _mpchannel->subscribers.erase(it);
      } else {
        CBVar tmp{};

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
  std::vector<CBVar> buffer;

  void recycle(MPMCChannel *channel) {
    // send previous values to recycle
    for (auto &var : buffer) {
      channel->recycle.push(var);
    }
    buffer.clear();
  }

  void add(CBVar &var) { buffer.push_back(var); }

  bool empty() { return buffer.size() == 0; }

  operator CBVar() {
    auto len = buffer.size();
    assert(len > 0);
    if (len > 1) {
      CBVar res{};
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
  CBTypeInfo _outType{};
  CBTypeInfo _seqType{};

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static CBParametersInfo parameters() { return consumerParams; }

  void setParam(int index, CBVar value) {
    if (index == 0)
      Base::setParam(index, value);
    else
      _bufferSize = value.payload.intValue;
  }

  CBVar getParam(int index) {
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
  CBTypeInfo compose(const CBInstanceData &data) {
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
    } break;
    default:
      throw CBException("Produce/Consume channel type expected.");
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    assert(_mpchannel);

    // send previous values to recycle
    _storage.recycle(_mpchannel);

    // reset buffer
    _current = _bufferSize;

    // everytime we are scheduled we try to pop a value
    while (_current--) {
      CBVar output{};
      while (!_mpchannel->data.pop(output)) {
        // check also for channel completion
        if (_mpchannel->closed) {
          if (!_storage.empty()) {
            return _storage;
          } else {
            return StopChain;
          }
        }
        suspend(context, 0);
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
      CBVar tmp{};
      while (_mpchannel->data.pop(tmp)) {
        destroyVar(tmp);
      }
      while (_mpchannel->recycle.pop(tmp)) {
        destroyVar(tmp);
      }
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    auto &vchannel = Globals::get(_name);
    switch (vchannel.index()) {
    case 2: {
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
    } break;
    default:
      throw CBException("Broadcast/Listen channel type expected.");
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    assert(_bchannel);
    assert(_mpchannel);

    // send previous values to recycle
    _storage.recycle(_mpchannel);

    // reset buffer
    _current = _bufferSize;

    // everytime we are scheduled we try to pop a value
    while (_current--) {
      CBVar output{};
      while (!_mpchannel->data.pop(output)) {
        // check also for channel completion
        if (_bchannel->closed) {
          if (!_storage.empty()) {
            return _storage;
          } else {
            return StopChain;
          }
        }
        suspend(context, 0);
      }

      // keep for recycling
      _storage.add(output);
    }

    return _storage;
  }
};

struct Complete : public Base {
  ChannelShared *_mpchannel;

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static CBParametersInfo parameters() { return consumerParams; }

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

void registerBlocks() {
  REGISTER_CBLOCK("Produce", Produce);
  REGISTER_CBLOCK("Broadcast", Broadcast);
  REGISTER_CBLOCK("Consume", Consume);
  REGISTER_CBLOCK("Listen", Listen);
  REGISTER_CBLOCK("Complete", Complete);
}
} // namespace channels
} // namespace chainblocks
