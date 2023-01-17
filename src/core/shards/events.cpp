/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

#include "shared.hpp"

namespace shards {
namespace Events {
struct Base {
  std::optional<std::reference_wrapper<entt::dispatcher>> _dispatcher;
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
    // Should we store types in a global map? and prevent type change? this allows receive to be predictable!
    return data.inputType;
  }
};

struct Send : Base {
  SHVar activate(SHContext *context, const SHVar &input) {
    assert(_dispatcher);

    auto &dispatcher = _dispatcher->get();
    auto current = context->wireStack.back();

    OwnedVar ownedInput = input;
    dispatcher.enqueue_hint(current->id, std::move(ownedInput));

    return input;
  }
};

struct Receive : Base {
  std::vector<OwnedVar> _events;
  std::vector<OwnedVar> _eventsOut;
  entt::connection _connection;

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }

  void onEvent(OwnedVar &event) { _events.push_back(event); }

  SHTypeInfo compose(const SHInstanceData &data) {
    Base::compose(data);
    assert(_dispatcher);
    _dispatcher->get().sink<OwnedVar>().connect<&Receive::onEvent>(this);
    return data.inputType;
  }

  void warmup(SHContext *context) {
    assert(_dispatcher);
    auto current = context->wireStack.back();
    if (_connection)
      _connection.release();
    _connection = _dispatcher->get().sink<OwnedVar>(current->id).connect<&Receive::onEvent>(this);
  }

  void cleanup() {
    assert(_dispatcher);
    if (_connection)
      _connection.release();
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    std::swap(_eventsOut, _events);
    _events.clear();
    return Var(_eventsOut);
  }
};

struct Update : Base {
  SHVar activate(SHContext *context, const SHVar &input) {
    assert(_dispatcher);

    auto &dispatcher = _dispatcher->get();

    dispatcher.update();

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