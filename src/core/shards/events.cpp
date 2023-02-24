/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

#include "shared.hpp"

namespace shards {
namespace Events {
struct Base {
  std::optional<std::reference_wrapper<EventDispatcher>> _dispatcher;
  std::string _eventName{"MyEvent"};

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static inline Parameters params{{"Name", SHCCSTR("The name of the event dispatcher to use."), {CoreInfo::StringType}}};

  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _eventName = value.payload.stringValue;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_eventName);
    default:
      return Var::Empty;
    }
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    _dispatcher = GetGlobals().Dispatchers[_eventName];
    return data.inputType;
  }
};

struct Send : Base {
  SHTypeInfo compose(const SHInstanceData &data) {
    Base::compose(data);

    // when we send we store the type of the event
    auto currentType = (*_dispatcher).get().type;
    if (currentType.basicType != SHType::None) {
      if (!matchTypes(data.inputType, currentType, false, true)) {
        SHLOG_ERROR("Event type mismatch, expected {} got {}", currentType, data.inputType);
        throw ComposeError("Event type mismatch");
      }
    } else {
      // we store the type of the event
      (*_dispatcher).get().type = data.inputType;
    }

    return data.inputType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    assert(_dispatcher);

    auto &dispatcher = _dispatcher->get();
    auto id = findId(context);

    OwnedVar ownedInput = input;
    dispatcher->enqueue_hint(id, std::move(ownedInput));

    return input;
  }
};

struct Receive : Base {
  SeqVar _eventsIn;
  SeqVar _eventsOut;

  entt::connection _connection;

  Type singleType;
  Type outputType;

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnySeqType; }

  void onEvent(OwnedVar &event) {
    auto e = _eventsIn.emplace_back();
    cloneVar(e, event);
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    Base::compose(data);

    // prevent none
    auto currentType = (*_dispatcher).get().type;
    if (currentType.basicType == SHType::None) {
      SHLOG_ERROR("Event type not set for event: {}, use Events.Send first", _eventName);
      throw ComposeError("Event type not set");
    }

    // fixup type
    singleType = currentType;
    outputType = Type::SeqOf(singleType);

    return outputType;
  }

  void warmup(SHContext *context) {
    assert(_dispatcher);
    auto id = findId(context);
    if (_connection)
      _connection.release();
    _connection = _dispatcher->get()->sink<OwnedVar>(id).connect<&Receive::onEvent>(this);
  }

  void cleanup() {
    assert(_dispatcher);
    if (_connection)
      _connection.release();
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    std::swap(_eventsOut, _eventsIn);
    _eventsIn.clear();
    return Var(_eventsOut);
  }
};

struct Update : Base {
  SHVar activate(SHContext *context, const SHVar &input) {
    assert(_dispatcher);

    auto &dispatcher = _dispatcher->get();

    dispatcher->update();

    return input;
  }
};

void registerShards() {
  REGISTER_SHARD("Events.Send", Send);
  REGISTER_SHARD("Events.Receive", Receive);
  REGISTER_SHARD("Events.Update", Update);
}
} // namespace Events
} // namespace shards