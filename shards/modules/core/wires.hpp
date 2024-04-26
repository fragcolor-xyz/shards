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

  void cleanup(SHContext *context) { wireref.cleanup(); }

  void warmup(SHContext *ctx) { wireref.warmup(ctx); }

  void resolveWire();

  void checkWireMesh(const std::string &rootName, SHMesh *currentMesh) {
    auto wireMesh = wire->mesh.lock();
    if (currentMesh && wireMesh && currentMesh != wireMesh.get()) {
      SHLOG_ERROR("Wire and current wire are not part of the same mesh, wire: {}, current: {}", wire->name, rootName);
      throw ActivationError("Wire and current wire are not part of the same mesh");
    }
  }

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

  void cleanup(SHContext *context) {
    _mesh.reset();

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

    WireBase::cleanup(context);
  }

  void wireOnStop(const SHWire::OnStopEvent &e) {
    SHLOG_TRACE("BaseRunner: wireOnStop {}", wire->name);

    if (e.wire == wire.get()) {
      for (auto &v : _vars) {
        // notice, this should be already destroyed by the wire releaseVariable
        std::string_view name = v.variableName(); // notice this calls a strlen!
        destroyVar(wire->getVariable(toSWL(name)));
      }
    }
  }

  std::shared_ptr<SHMesh> _mesh;

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

      auto mesh = wire->mesh.lock();
      if (mesh) {
        onStopConnection = mesh->dispatcher.sink<SHWire::OnStopEvent>().connect<&BaseRunner::wireOnStop>(this);
      }
    }

    _mesh = ctx->main->mesh.lock();

    // Fixup the wire id, since compose may be skipped, but id changed
    if (wire) {
      wire->id = ctx->currentWire()->id;
    }
  }

  void doWarmup(SHContext *context) {
    if (mode == RunWireMode::Inline && wire && wire->wireUsers.count(this) == 0) {
      wire->wireUsers.emplace(this);
      wire->warmup(context);
    }
  }

  bool _restart{false};
  void activateDetached(SHContext *context, const SHVar &input) {
    assert(_mesh);

    if (!shards::isRunning(wire.get())) {
      // stop in case we need to clean up
      stop(wire.get());

      if (capturing) {
        for (auto &v : _vars) {
          auto &var = v.get();
          std::string_view name = v.variableName(); // notice this calls a strlen!
          cloneVar(wire->getVariable(toSWL(name)), var);
        }
      }

      // Assign wire id again, since it's reset on stop
      wire->id = context->currentWire()->id;

      // validated during infer not here! (false)
      _mesh->schedule(wire, input, false);

      SHWire *rootWire = context->rootWire();
      _mesh->dispatcher.trigger(SHWire::OnWireDetachedEvent{
          .wire = rootWire,
          .childWire = wire.get(),
      });

      // also mark this wire as fully detached if needed
      // this means stopping this Shard will not stop the wire
      if (detached)
        wire->detached = true;
    } else if (_restart) {
      _mesh->remove(wire);
      // simply tail call activate again
      activateDetached(context, input);
    }
  }

  void activateStepMode(SHContext *context, const SHVar &input) {
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

      // Assign wire id again, since it's reset on stop
      wire->id = context->currentWire()->id;
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
};

template <bool INPUT_PASSTHROUGH, RunWireMode WIRE_MODE> struct RunWire : public BaseRunner {
  void setup() {
    passthrough = INPUT_PASSTHROUGH;
    mode = WIRE_MODE;
    capturing = WIRE_MODE == RunWireMode::Async; // if Detach we need to capture
    detached = WIRE_MODE == RunWireMode::Async;  // in RunWire we always detach
  }

  static inline Parameters DetachParamsInfo{
      {"Wire", SHCCSTR("The wire to run."), {WireTypes}},
      {"Restart",
       SHCCSTR("If on activation the wire should be restarted from scratch even if it was still running."),
       {CoreInfo::BoolType}}};

  static SHParametersInfo parameters() {
    if constexpr (WIRE_MODE == RunWireMode::Async) {
      return DetachParamsInfo;
    } else {
      return runWireParamsInfo;
    }
  }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      wireref = value;
      break;
    case 1:
      if constexpr (WIRE_MODE == RunWireMode::Async) {
        _restart = value.payload.boolValue;
        break;
      }
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return wireref;
    case 1: {
      if constexpr (WIRE_MODE == RunWireMode::Async) {
        return Var(_restart);
      }
    }
    default:
      break;
    }
    return Var::Empty;
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    if (WIRE_MODE == RunWireMode::Inline && data.onWorkerThread) {
      throw ComposeError("Cannot run a wire inline (Do) on a worker thread");
    }

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
        if (unlikely(runRes.state == SHRunWireOutputState::Returned)) {
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
        if (unlikely(runRes.state == SHRunWireOutputState::Returned)) {
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
