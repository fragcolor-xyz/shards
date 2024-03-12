#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>
#include <chrono>

namespace shards {
using namespace std::chrono_literals;

struct DoWire {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR(""); }

  PARAM_PARAMVAR(_wire, "Wire", "The wire to run", {shards::CoreInfo::WireType, shards::CoreInfo::WireVarType});
  PARAM_VAR(_expectedOutputType, "ExpectedOutputType", "The expected output type",
            {shards::CoreInfo::NoneType, shards::CoreInfo::TypeType});
  PARAM_IMPL(PARAM_IMPL_FOR(_wire), PARAM_IMPL_FOR(_expectedOutputType));

  using Clock = std::chrono::steady_clock;
  using TP = Clock::time_point;
  struct Cached {
    Cached(SHWireRef ref) { wire = SHWire::sharedFromRef(ref); }
    std::shared_ptr<SHWire> wire;
    TP lastUsage;
  };
  std::unordered_map<SHWireRef, Cached> _cache;

  ExposedInfo _environment;
  SHTypeInfo _inputType;

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  void gc() {
    TP now = Clock::now();
    for (auto it = _cache.begin(); it != _cache.end();) {
      auto delta = now - it->second.lastUsage;
      if (delta > 1s) {
        it = _cache.erase(it);
      } else {
        ++it;
      }
    }
  }

  void prepare(SHWireRef ref) {
    auto wire = SHWire::sharedFromRef(ref);
    if (!wire->composeResult) {
      SHInstanceData instanceData{
          .wire = wire.get(),
          .inputType = _inputType,
          .shared = (SHExposedTypesInfo)_environment,
      };
      wire->composeResult = composeWire(wire.get(), nullptr, nullptr, instanceData);
      if (!_expectedOutputType->isNone()) {
        SHTypeInfo &expectedType = *_expectedOutputType->payload.typeValue;
        if (!matchTypes(wire->outputType, expectedType, false, true, true)) {
          throw ActivationError(
              fmt::format("DoByKey: Wire {} should output type {} but got {}", wire->name, expectedType, wire->outputType));
        }
      }
    }
  }

  Cached &getCache(SHWireRef wire) {
    auto it = _cache.find(wire);
    if (it == _cache.end()) {
      prepare(wire);
      it = _cache.emplace(wire, wire).first;
    }
    return it->second;
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    _cache.clear();

    for (auto &e : data.shared)
      _environment.push_back(e);
    _inputType = data.inputType;

    if (!_expectedOutputType->isNone()) {
      SHTypeInfo &expectedType = *_expectedOutputType->payload.typeValue;
      return expectedType;
    } else {
      return CoreInfo::AnyType;
    }
  }

  void activateStepMode(const std::shared_ptr<SHWire> &wire, SHContext *context, const SHVar &input) {
    // Allow to re run wires
    if (shards::hasEnded(wire.get())) {
      // stop the root
      if (!shards::stop(wire.get())) {
        throw ActivationError(fmt::format("Step: errors while running wire {}", wire->name));
      }
    }

    // Prepare if no callc was called
    if (!coroutineValid(wire->coro)) {
      wire->mesh = context->main->mesh;
      // pre-set wire context with our context
      // this is used to copy wireStack over to the new one
      wire->context = context;
      // Notice we don't share our flow!
      // let the wire create one by passing null
      shards::prepare(wire.get(), nullptr);
      if (wire->state == SHWire::Failed) {
        throw ActivationError(fmt::format("Step: wire {} warmup failed", wire->name));
      }
    }

    shassert(wire->context && "wire context should be valid at this point");

    // Starting
    if (!shards::isRunning(wire.get())) {
      shards::start(wire.get(), input);
    } else {
      // Update input
      wire->currentInput = input;
    }

    // Tick the wire on the flow that this Step wire created in prepare
    SHDuration now = SHClock::now().time_since_epoch();
    shards::tick(wire->context->flow->wire, now);
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    Var &wireVar = (Var &)_wire.get();

    SHWireRef wireRef = wireVar.payload.wireValue;
    auto &cached = getCache(wireRef);

    // Run within the root flow
    auto runRes = runSubWire(cached.wire.get(), shContext, input);
    if (unlikely(runRes.state == SHRunWireOutputState::Failed)) {
      // When an error happens during inline execution, propagate the error to the parent wire
      SHLOG_ERROR("Wire {} failed", cached.wire->name);
      shContext->cancelFlow("Wire failed");
    } else {
      // we don't want to propagate a (Return)
      if (unlikely(runRes.state == SHRunWireOutputState::Returned)) {
        shContext->continueFlow();
      }
    }

    gc();
    return runRes.output;
  }
};
} // namespace shards

SHARDS_REGISTER_FN(wires_dynamic) {
  using namespace shards;
  REGISTER_SHARD("DoWire", DoWire);
}