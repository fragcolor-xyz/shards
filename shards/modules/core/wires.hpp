#ifndef EAE30273_C65E_49F0_BEF0_69BE363EAF61
#define EAE30273_C65E_49F0_BEF0_69BE363EAF61

#include <cassert>
#include <shards/core/foundation.hpp>
#include <shards/core/shared.hpp>
#include <shards/common_types.hpp>
#include <memory>
#include <deque>

#include <entt/entt.hpp>

namespace shards {

enum RunWireMode { Inline, Async, Stepped };

struct WireBase {
  DECL_ENUM_INFO(RunWireMode, RunWireMode, 'runc');

  static inline Types WireTypes{{CoreInfo::WireType, CoreInfo::StringType, CoreInfo::NoneType}};

  static inline Types WireVarTypes{WireTypes, {CoreInfo::WireVarType}};

  static inline Parameters waitParamsInfo{
      {"Wire", SHCCSTR("The wire to wait."), {WireVarTypes}},
      {"Passthrough", SHCCSTR("The output of this shard will be its input."), {CoreInfo::BoolType}},
      {"Timeout",
       SHCCSTR("The optional amount of time in seconds to wait for the wire to complete, if the time elapses the wire will be "
               "stopped and the flow will fail with a timeout error."),
       {CoreInfo::FloatType, CoreInfo::FloatVarType, CoreInfo::NoneType}}};

  static inline Parameters stopWireParamsInfo{
      {"Wire", SHCCSTR("The wire to stop."), {WireVarTypes}},
      {"Passthrough", SHCCSTR("The output of this shard will be its input."), {CoreInfo::BoolType}}};

  static inline Parameters runWireParamsInfo{{"Wire", SHCCSTR("The wire to run."), {WireTypes}}};

  ParamVar wireref{};
  std::shared_ptr<SHWire> wire;

  bool passthrough{false};

  // if false, means the Shard is not going to activate the wire, but just wait for it to complete usually or similar
  bool activating{true};
  bool capturing{false};
  bool detached{false};

  RunWireMode mode{RunWireMode::Inline};

  IterableExposedInfo exposedInfo{};

  void resetComposition();

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  std::unordered_set<const SHWire *> &gatheringWires();

  void verifyAlreadyComposed(const SHInstanceData &data, IterableExposedInfo &shared);

  SHTypeInfo compose(const SHInstanceData &data);

  void cleanup() { wireref.cleanup(); }

  void warmup(SHContext *ctx) { wireref.warmup(ctx); }

  void resolveWire();

  // Use state to mark the dependency for serialization as well!

  SHVar getState() {
    if (wire) {
      return Var(wire);
    } else {
      // SHLOG_TRACE("getState no wire was avail");
      return Var::Empty;
    }
  }

  void setState(SHVar state) {
    if (state.valueType == SHType::Wire) {
      wire = SHWire::sharedFromRef(state.payload.wireValue);
    }
  }
};

struct BaseRunner : public WireBase {
  std::deque<ParamVar> _vars;

  SHExposedTypesInfo _mergedReqs;
  void destroy() { arrayFree(_mergedReqs); }

  SHTypeInfo compose(const SHInstanceData &data) {
    resolveWire();
    assert(wire && "wire should be set at this point");
    // Start/Resume need to capture all it needs, so we need deeper informations
    // this is triggered by populating requiredVariables variable
    auto dataCopy = data;
    dataCopy.requiredVariables = &wire->requirements;

    auto res = WireBase::compose(dataCopy);

    // build the list of variables to capture and inject into spawned chain
    _vars.clear();
    arrayResize(_mergedReqs, 0);
    for (auto &avail : data.shared) {
      auto it = wire->requirements.find(avail.name);
      if (it != wire->requirements.end()) {
        if (!avail.global) {
          // Capture if not global as we need to copy it!
          SHLOG_TRACE("BaseRunner: adding variable to requirements: {}, wire {}", avail.name, wire->name);
          SHVar ctxVar{};
          ctxVar.valueType = SHType::ContextVar;
          ctxVar.payload.stringValue = avail.name;
          ctxVar.payload.stringLen = strlen(avail.name);
          auto &p = _vars.emplace_back();
          p = ctxVar;
        }

        arrayPush(_mergedReqs, it->second);
      }
    }

    return res;
  }

  // Only wire runners should expose variables to the context
  SHExposedTypesInfo exposedVariables() {
    // Only inline mode ensures that variables will be really avail
    // step and detach will run at different timing
    SHExposedTypesInfo empty{};
    return mode == RunWireMode::Inline ? SHExposedTypesInfo(exposedInfo) : empty;
  }

  SHExposedTypesInfo requiredVariables() { return _mergedReqs; }

  std::optional<entt::connection> onStopConnection;

  void cleanup() {
    if (capturing) {
      for (auto &v : _vars) {
        v.cleanup();
      }
    }

    if (wire) {
      if (mode == RunWireMode::Inline && wire->wireUsers.count(this) != 0) {
        wire->wireUsers.erase(this);
        wire->cleanup();
      } else if (!wire->detached) {
        // but avoid stopping if detached and scheduled
        shards::stop(wire.get());
      }
    }

    if (onStopConnection) {
      // noticed that if we are `scheduled` then we don't watch for the onStop event anymore
      // indeed we assume that the wire will just releaseVariable eventually
      onStopConnection->release();
      onStopConnection.reset();
    }

    WireBase::cleanup();
  }

  void wireOnStop(const SHWire::OnStopEvent &e) {
    SHLOG_TRACE("BaseRunner: wireOnStop {}", wire->name);

    assert(e.wire == wire.get());

    for (auto &v : _vars) {
      // notice, this should be already destroyed by the wire releaseVariable
      destroyVar(wire->variables[v.variableName()]);
    }
  }

  void warmup(SHContext *ctx) {
    if (capturing) {
      assert(wire && "wire should be set at this point");

      for (auto &v : _vars) {
        SHLOG_TRACE("BaseRunner: warming up variable: {}, wire: {}", v.variableName(), wire->name);
        v.warmup(ctx);
      }

      if (onStopConnection) {
        onStopConnection->release();
        onStopConnection.reset();
      }

      onStopConnection = wire->dispatcher.sink<SHWire::OnStopEvent>().connect<&BaseRunner::wireOnStop>(this);
    }
  }

  void doWarmup(SHContext *context) {
    if (mode == RunWireMode::Inline && wire && wire->wireUsers.count(this) == 0) {
      wire->wireUsers.emplace(this);
      wire->warmup(context);
    }
  }

  void activateDetached(SHContext *context, const SHVar &input) {
    if (!shards::isRunning(wire.get())) {
      // stop in case we need to clean up
      stop(wire.get());

      if (capturing) {
        for (auto &v : _vars) {
          auto &var = v.get();
          cloneVar(wire->variables[v.variableName()], var);
        }
      }

      // validated during infer not here! (false)
      auto mesh = context->main->mesh.lock();
      if (mesh)
        mesh->schedule(wire, input, false);
      // also mark this wire as fully detached if needed
      // this means stopping this Shard will not stop the wire
      if (detached)
        wire->detached = true;
    }
  }

  void activateStepMode(SHContext *context, const SHVar &input) {
    // Allow to re run wires
    if (shards::hasEnded(wire.get())) {
      // stop the root
      if (!shards::stop(wire.get())) {
        throw ActivationError("Stepped sub-wire did not end normally.");
      }
    }

    // Prepare if no callc was called
    if (!wire->coro) {
      wire->mesh = context->main->mesh;
      // pre-set wire context with our context
      // this is used to copy wireStack over to the new one
      wire->context = context;
      // Notice we don't share our flow!
      // let the wire create one by passing null
      shards::prepare(wire.get(), nullptr);
    }

    // Starting
    if (!shards::isRunning(wire.get())) {
      shards::start(wire.get(), input);
    } else {
      // Update input
      wire->currentInput = input;
    }

    // Tick the wire on the flow that this Step wire created
    SHDuration now = SHClock::now().time_since_epoch();
    if(wire->context->flow->wire)
      shards::tick(wire->context->flow->wire, now);
  }
};

template <bool INPUT_PASSTHROUGH, RunWireMode WIRE_MODE> struct RunWire : public BaseRunner {
  void setup() {
    passthrough = INPUT_PASSTHROUGH;
    mode = WIRE_MODE;
    capturing = WIRE_MODE == RunWireMode::Async; // if Detach we need to capture
    detached = WIRE_MODE == RunWireMode::Async;  // in RunWire we always detach
  }

  static SHParametersInfo parameters() { return runWireParamsInfo; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      wireref = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return wireref;
    default:
      break;
    }
    return Var::Empty;
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    auto res = BaseRunner::compose(data);
    if (!wire) {
      OVERRIDE_ACTIVATE(data, activateNil);
    } else if (wire->looped && WIRE_MODE == RunWireMode::Inline) {
      OVERRIDE_ACTIVATE(data, activateLoop);
    } else {
      OVERRIDE_ACTIVATE(data, activate);
    }
    return res;
  }

  void warmup(SHContext *context) {
    BaseRunner::warmup(context);

    doWarmup(context);
  }

  SHVar activateNil(SHContext *, const SHVar &input) { return input; }

  SHVar activateLoop(SHContext *context, const SHVar &input) {
    auto inputCopy = input;
  run_wire_loop:
    auto runRes = runSubWire(wire.get(), context, inputCopy);
    if (unlikely(runRes.state == SHRunWireOutputState::Failed)) {
      // meaning there was an exception while
      // running the sub wire, stop the parent too
      context->stopFlow(runRes.output);
      return runRes.output;
    } else {
      if (runRes.state == SHRunWireOutputState::Restarted) {
        inputCopy = context->getFlowStorage();
        context->continueFlow();
        SH_SUSPEND(context, 0.0);
        goto run_wire_loop;
      } else if (context->shouldContinue()) {
        SH_SUSPEND(context, 0.0);
        goto run_wire_loop;
      } else {
        // we don't want to propagate a (Return)
        if (unlikely(runRes.state == SHRunWireOutputState::Stopped)) {
          context->continueFlow();
        }

        if constexpr (INPUT_PASSTHROUGH) {
          return input;
        } else {
          return runRes.output;
        }
      }
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if constexpr (WIRE_MODE == RunWireMode::Async) {
      activateDetached(context, input);
      return input;
    } else if constexpr (WIRE_MODE == RunWireMode::Stepped) {
      activateStepMode(context, input);
      if constexpr (INPUT_PASSTHROUGH) {
        return input;
      } else {
        return wire->previousOutput;
      }
    } else if constexpr (WIRE_MODE == RunWireMode::Inline) {
      // Run within the root flow
      auto runRes = runSubWire(wire.get(), context, input);
      if (unlikely(runRes.state == SHRunWireOutputState::Failed)) {
        // When an error happens during inline execution, propagate the error to the parent wire
        SHLOG_ERROR("Wire {} failed", wire->name);
        context->cancelFlow("Wire failed");
        return runRes.output;
      } else {
        // we don't want to propagate a (Return)
        if (unlikely(runRes.state == SHRunWireOutputState::Stopped)) {
          context->continueFlow();
        }

        if constexpr (INPUT_PASSTHROUGH) {
          return input;
        } else {
          return runRes.output;
        }
      }
    } else {
      throw std::logic_error("invalid path");
    }
  }
};

} // namespace shards

#endif /* EAE30273_C65E_49F0_BEF0_69BE363EAF61 */
