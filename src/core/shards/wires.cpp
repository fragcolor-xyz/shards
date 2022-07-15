/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "foundation.hpp"
#include "shared.hpp"
#include <chrono>
#include <memory>
#include <set>
#if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
#include <taskflow/taskflow.hpp>
#endif

namespace shards {
enum RunWireMode { Inline, Detached, Stepped };

struct WireBase {
  typedef EnumInfo<RunWireMode> RunWireModeInfo;
  static inline RunWireModeInfo runWireModeInfo{"RunWireMode", CoreCC, 'runC'};
  static inline Type ModeType{{SHType::Enum, {.enumeration = {.vendorId = CoreCC, .typeId = 'runC'}}}};

  static inline Types WireTypes{{CoreInfo::WireType, CoreInfo::StringType, CoreInfo::NoneType}};

  static inline Types WireVarTypes{WireTypes, {CoreInfo::WireVarType}};

  static inline Parameters waitParamsInfo{
      {"Wire", SHCCSTR("The wire to wait."), {WireVarTypes}},
      {"Passthrough", SHCCSTR("The output of this shard will be its input."), {CoreInfo::BoolType}}};

  static inline Parameters stopWireParamsInfo{
      {"Wire", SHCCSTR("The wire to stop."), {WireVarTypes}},
      {"Passthrough", SHCCSTR("The output of this shard will be its input."), {CoreInfo::BoolType}}};

  static inline Parameters runWireParamsInfo{{"Wire", SHCCSTR("The wire to run."), {WireTypes}}};

  ParamVar wireref{};
  std::shared_ptr<SHWire> wire;
  bool passthrough{false};
  bool capturing{false};
  RunWireMode mode{RunWireMode::Inline};
  SHComposeResult wireValidation{};
  IterableExposedInfo exposedInfo{};

  void destroy() {
    shards::arrayFree(wireValidation.requiredInfo);
    shards::arrayFree(wireValidation.exposedInfo);
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  std::unordered_set<const SHWire *> &gatheringWires() {
#ifdef WIN32
    // we have to leak.. or windows tls emulation will crash at process end
    thread_local std::unordered_set<const SHWire *> *wires = new std::unordered_set<const SHWire *>();
    return *wires;
#else
    thread_local std::unordered_set<const SHWire *> wires;
    return wires;
#endif
  }

  void verifyAlreadyComposed(const SHInstanceData &data, IterableExposedInfo &shared) {
    // verify input type
    if (!passthrough && mode != Stepped && data.inputType != wire->inputType && !wire->inputTypeForceNone) {
      SHLOG_ERROR("Previous wire composed type {} requested call type {}", *wire->inputType, data.inputType);
      throw ComposeError("Attempted to call an already composed wire with a "
                         "different input type! wire: " +
                         wire->name);
    }

    // ensure requirements match our input data
    for (auto &[req, _] : wire->requiredVariables) {
      // find each in shared
      auto name = req;
      auto res = std::find_if(shared.begin(), shared.end(), [name](SHExposedTypeInfo &x) {
        std::string_view xNameView(x.name);
        return name == xNameView;
      });
      if (res == shared.end()) {
        SHLOG_ERROR("Previous wire composed missing required variable: {}", req);
        throw ComposeError("Attempted to call an already composed wire (" + wire->name + ") with a missing required variable");
      }
    }

    // also restore deep reqs
    if (data.requiredVariables) {
      auto reqs = reinterpret_cast<decltype(SHWire::deepRequirements) *>(data.requiredVariables);
      for (auto &req : wire->deepRequirements) {
        (*reqs).insert(req);
      }
    }
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    // Free any previous result!
    arrayFree(wireValidation.requiredInfo);
    arrayFree(wireValidation.exposedInfo);

    // Actualize the wire here, if we are deserialized
    // wire might already be populated!
    if (!wire) {
      if (wireref->valueType == SHType::Wire) {
        wire = SHWire::sharedFromRef(wireref->payload.wireValue);
      } else if (wireref->valueType == String) {
        wire = GetGlobals().GlobalWires[wireref->payload.stringValue];
      } else {
        wire = nullptr;
        SHLOG_DEBUG("WireBase::compose on a null wire");
      }
    }

    // Easy case, no wire...
    if (!wire)
      return data.inputType;

    assert(data.wire);

    if (wire.get() == data.wire) {
      SHLOG_DEBUG("WireBase::compose early return, data.wire == wire, name: {}", wire->name);
      return data.inputType; // we don't know yet...
    }

    wire->mesh = data.wire->mesh;

    auto mesh = data.wire->mesh.lock();
    assert(mesh);

    // TODO FIXME, wireloader/wire runner might access this from threads
    if (mesh->visitedWires.count(wire.get())) {
      // but visited does not mean composed...
      if (wire->composedHash.valueType != None) {
        IterableExposedInfo shared(data.shared);
        verifyAlreadyComposed(data, shared);
      }
      return mesh->visitedWires[wire.get()];
    }

    // avoid stack-overflow
    if (wire->isRoot || gatheringWires().count(wire.get())) {
      SHLOG_DEBUG("WireBase::compose early return, wire is being visited, name: {}", wire->name);
      return data.inputType; // we don't know yet...
    }

    SHLOG_TRACE("WireBase::compose, source: {} composing: {} inputType: {}", data.wire->name, wire->name, data.inputType);

    // we can add early in this case!
    // useful for Resume/Start
    if (passthrough) {
      auto [_, done] = mesh->visitedWires.emplace(wire.get(), data.inputType);
      if (done) {
        SHLOG_TRACE("Pre-Marking as composed: {} ptr: {}", wire->name, (void *)wire.get());
      }
    } else if (mode == Stepped) {
      auto [_, done] = mesh->visitedWires.emplace(wire.get(), CoreInfo::AnyType);
      if (done) {
        SHLOG_TRACE("Pre-Marking as composed: {} ptr: {}", wire->name, (void *)wire.get());
      }
    }

    // and the subject here
    gatheringWires().insert(wire.get());
    DEFER(gatheringWires().erase(wire.get()));

    auto dataCopy = data;
    dataCopy.wire = wire.get();
    IterableExposedInfo shared(data.shared);
    IterableExposedInfo sharedCopy;
    if (mode == RunWireMode::Detached && !capturing) {
      // keep only globals
      auto end = std::remove_if(shared.begin(), shared.end(), [](const SHExposedTypeInfo &x) { return !x.global; });
      sharedCopy = IterableExposedInfo(shared.begin(), end);
    } else {
      // we allow Detached but they need to be referenced during warmup
      sharedCopy = shared;
    }

    dataCopy.shared = sharedCopy;

    SHTypeInfo wireOutput;
    // make sure to compose only once...
    if (wire->composedHash.valueType == None) {
      SHLOG_TRACE("Running {} compose", wire->name);

      wireValidation = composeWire(
          wire.get(),
          [](const Shard *errorShard, const char *errorTxt, bool nonfatalWarning, void *userData) {
            if (!nonfatalWarning) {
              SHLOG_ERROR("RunWire: failed inner wire validation, error: {}", errorTxt);
              throw ComposeError("RunWire: failed inner wire validation");
            } else {
              SHLOG_INFO("RunWire: warning during inner wire validation: {}", errorTxt);
            }
          },
          this, dataCopy);
      wire->composedHash = Var(1, 1); // no need to hash properly here
      wireOutput = wireValidation.outputType;
      IterableExposedInfo exposing(wireValidation.exposedInfo);
      // keep only globals
      exposedInfo = IterableExposedInfo(
          exposing.begin(), std::remove_if(exposing.begin(), exposing.end(), [](SHExposedTypeInfo &x) { return !x.global; }));

      // also store deep reqs
      if (data.requiredVariables) {
        auto reqs = reinterpret_cast<decltype(SHWire::deepRequirements) *>(data.requiredVariables);
        for (auto &req : (*reqs)) {
          wire->deepRequirements.insert(req);
        }
      }

      SHLOG_TRACE("Wire {} composed", wire->name);
    } else {
      SHLOG_TRACE("Skipping {} compose", wire->name);

      verifyAlreadyComposed(data, shared);

      // write output type
      wireOutput = wire->outputType;
    }

    auto outputType = data.inputType;

    if (!passthrough) {
      if (mode == Inline)
        outputType = wireOutput;
      else if (mode == Stepped)
        outputType = CoreInfo::AnyType; // unpredictable
      else
        outputType = data.inputType;
    }

    if (!passthrough && mode != Stepped) {
      auto [_, done] = mesh->visitedWires.emplace(wire.get(), outputType);
      if (done) {
        SHLOG_TRACE("Marking as composed: {} ptr: {} inputType: {} outputType: {}", wire->name, (void *)wire.get(),
                    *wire->inputType, wire->outputType);
      }
    }

    return outputType;
  }

  void cleanup() { wireref.cleanup(); }

  void warmup(SHContext *ctx) { wireref.warmup(ctx); }

  // Use state to mark the dependency for serialization as well!

  SHVar getState() {
    if (wire) {
      return Var(wire);
    } else {
      SHLOG_TRACE("getState no wire was avail");
      return Var::Empty;
    }
  }

  void setState(SHVar state) {
    if (state.valueType == SHType::Wire) {
      wire = SHWire::sharedFromRef(state.payload.wireValue);
    }
  }
};

struct Wait : public WireBase {
  SHOptionalString help() {
    return SHCCSTR("Waits for another wire to complete before resuming "
                   "execution of the current wire.");
  }

  // we don't need OwnedVar here
  // we keep the wire referenced!
  SHVar _output{};
  SHExposedTypeInfo _requiredWire{};

  void cleanup() {
    if (wireref.isVariable())
      wire = nullptr;
    WireBase::cleanup();
  }

  static SHParametersInfo parameters() { return waitParamsInfo; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      wireref = value;
      break;
    case 1:
      passthrough = value.payload.boolValue;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return wireref;
    case 1:
      return Var(passthrough);
    default:
      return Var::Empty;
    }
  }

  SHExposedTypesInfo requiredVariables() {
    if (wireref.isVariable()) {
      _requiredWire = SHExposedTypeInfo{wireref.variableName(), SHCCSTR("The wire to run."), CoreInfo::WireType};
      return {&_requiredWire, 1, 0};
    } else {
      return {};
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (unlikely(!wire && wireref.isVariable())) {
      auto vwire = wireref.get();
      if (vwire.valueType == SHType::Wire) {
        wire = SHWire::sharedFromRef(vwire.payload.wireValue);
      } else if (vwire.valueType == String) {
        wire = GetGlobals().GlobalWires[vwire.payload.stringValue];
      } else {
        wire = nullptr;
      }
    }

    if (unlikely(!wire)) {
      SHLOG_WARNING("Wait's wire is void");
      return input;
    } else {
      // Make sure to actually wait only if the wire is running on another context.
      while (wire->context != context && isRunning(wire.get())) {
        SH_SUSPEND(context, 0);
      }

      if (wire->finishedError.size() > 0) {
        // if the wire has errors we need to propagate them
        // we can avoid interruption using Maybe shards
        throw ActivationError(wire->finishedError);
      }

      if (passthrough) {
        return input;
      } else {
        // no clone
        _output = wire->finishedOutput;
        return _output;
      }
    }
  }
};

struct StopWire : public WireBase {
  SHOptionalString help() { return SHCCSTR("Stops another wire. If no wire is given, stops the current wire."); }

  void setup() { passthrough = true; }

  OwnedVar _output{};
  SHExposedTypeInfo _requiredWire{};

  SHTypeInfo _inputType{};

  SHTypeInfo compose(const SHInstanceData &data) {
    _inputType = data.inputType;
    WireBase::compose(data);
    return data.inputType;
  }

  void composed(const SHWire *wire, const SHComposeResult *result) {
    if (!wire && wireref->valueType == None && _inputType != result->outputType) {
      SHLOG_ERROR("Stop input and wire output type mismatch, Stop input must "
                  "be the same type of the wire's output (regular flow), "
                  "wire: {} expected: {}",
                  wire->name, wire->outputType);
      throw ComposeError("Stop input and wire output type mismatch");
    }
  }

  void cleanup() {
    if (wireref.isVariable())
      wire = nullptr;
    WireBase::cleanup();
  }

  static SHParametersInfo parameters() { return stopWireParamsInfo; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      wireref = value;
      break;
    case 1:
      passthrough = value.payload.boolValue;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return wireref;
    case 1:
      return Var(passthrough);
    default:
      return Var::Empty;
    }
  }

  SHExposedTypesInfo requiredVariables() {
    if (wireref.isVariable()) {
      _requiredWire = SHExposedTypeInfo{wireref.variableName(), SHCCSTR("The wire to run."), CoreInfo::WireType};
      return {&_requiredWire, 1, 0};
    } else {
      return {};
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (unlikely(!wire && wireref.isVariable())) {
      auto vwire = wireref.get();
      if (vwire.valueType == SHType::Wire) {
        wire = SHWire::sharedFromRef(vwire.payload.wireValue);
      } else if (vwire.valueType == String) {
        wire = GetGlobals().GlobalWires[vwire.payload.stringValue];
      } else {
        wire = nullptr;
      }
    }

    if (unlikely(!wire)) {
      // in this case we stop the current wire
      context->stopFlow(input);
      return input;
    } else {
      shards::stop(wire.get());
      if (passthrough) {
        return input;
      } else {
        _output = wire->finishedOutput;
        return _output;
      }
    }
  }
};

struct Resume : public WireBase {
  std::deque<ParamVar> _vars;

  static SHOptionalString help() {
    return SHCCSTR("Resumes a given wire and suspends the current one. In "
                   "other words, switches flow execution to another wire.");
  }

  void setup() {
    // we use those during WireBase::compose
    passthrough = true;
    mode = RunWireMode::Detached;
    capturing = true;
  }

  static inline Parameters params{{"Wire", SHCCSTR("The name of the wire to switch to."), {WireTypes}}};

  static SHParametersInfo parameters() { return params; }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  SHExposedTypesInfo _mergedReqs;
  void destroy() {
    arrayFree(_mergedReqs);
    WireBase::destroy();
  }
  SHExposedTypesInfo requiredVariables() { return _mergedReqs; }

  SHTypeInfo compose(const SHInstanceData &data) {
    // Start/Resume need to capture all it needs, so we need deeper informations
    // this is triggered by populating requiredVariables variable
    auto dataCopy = data;
    std::unordered_map<std::string_view, SHExposedTypeInfo> requirements;
    dataCopy.requiredVariables = &requirements;

    WireBase::compose(dataCopy);

    // build the list of variables to capture and inject into spawned chain
    _vars.clear();
    arrayResize(_mergedReqs, 0);
    for (auto &avail : data.shared) {
      auto it = requirements.find(avail.name);
      if (it != requirements.end() && !avail.global) {
        SHLOG_TRACE("Start/Resume: adding variable to requirements: {}, wire {}", avail.name, wire->name);
        SHVar ctxVar{};
        ctxVar.valueType = ContextVar;
        ctxVar.payload.stringValue = avail.name;
        auto &p = _vars.emplace_back();
        p = ctxVar;

        arrayPush(_mergedReqs, it->second);
      }
    }

    return data.inputType;
  }

  void setParam(int index, const SHVar &value) { wireref = value; }

  SHVar getParam(int index) { return wireref; }

  // NO cleanup, other wires reference this wire but should not stop it
  // An arbitrary wire should be able to resume it!
  // Cleanup mechanics still to figure, for now ref count of the actual wire
  // symbol, TODO maybe use SHVar refcount!

  void warmup(SHContext *context) {
    WireBase::warmup(context);

    for (auto &v : _vars) {
      v.warmup(context);
    }
  }

  void cleanup() {
    for (auto &v : _vars) {
      v.cleanup();
    }

    WireBase::cleanup();
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto current = context->wireStack.back();

    auto p_wire = [&] {
      if (!wire) {
        if (current->resumer) {
          SHLOG_TRACE("Resume, wire not found, using resumer: {}", current->resumer->name);
          return current->resumer;
        } else {
          throw ActivationError("Resume, wire not found.");
        }
      } else {
        return wire.get();
      }
    }();

    // assign the new wire as current wire on the flow
    context->flow->wire = p_wire;

    // Allow to re run wires
    if (shards::hasEnded(p_wire)) {
      shards::stop(p_wire);
    }

    // Prepare if no callc was called
    if (!p_wire->coro) {
      p_wire->mesh = context->main->mesh;
      shards::prepare(p_wire, context->flow);
    }

    // we should be valid as this shard should be dependent on current
    // do this here as stop/prepare might overwrite
    if (p_wire->resumer == nullptr)
      p_wire->resumer = current;

    // capture variables
    for (auto &v : _vars) {
      auto &var = v.get();
      auto &v_ref = p_wire->variables[v.variableName()];
      assert(v_ref.refcount > 0);
      cloneVar(v_ref, var);
    }

    // Start it if not started
    if (!shards::isRunning(p_wire)) {
      shards::start(p_wire, input);
    }

    // And normally we just delegate the Mesh + SHFlow
    // the following will suspend this current wire
    // and in mesh tick when re-evaluated tick will
    // resume with the wire we just set above!
    shards::suspend(context, 0);

    // We will end here when we get resumed!

    // reset resumer
    p_wire->resumer = nullptr;

    return input;
  }
};

struct Start : public Resume {
  static SHOptionalString help() {
    return SHCCSTR("Starts a given wire and suspends the current one. In "
                   "other words, switches flow execution to another wire.");
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto current = context->wireStack.back();

    auto p_wire = [&] {
      if (!wire) {
        if (current->resumer) {
          SHLOG_TRACE("Start, wire not found, using resumer: {}", current->resumer->name);
          return current->resumer;
        } else {
          throw ActivationError("Start, wire not found.");
        }
      } else {
        return wire.get();
      }
    }();

    // assign the new wire as current wire on the flow
    context->flow->wire = p_wire;

    // ensure wire is not running, we start from top
    shards::stop(p_wire);

    // Prepare
    p_wire->mesh = context->main->mesh;
    shards::prepare(p_wire, context->flow);

    // we should be valid as this shard should be dependent on current
    // do this here as stop/prepare might overwrite
    if (p_wire->resumer == nullptr)
      p_wire->resumer = current;

    // capture variables
    for (auto &v : _vars) {
      auto &var = v.get();
      auto &v_ref = p_wire->variables[v.variableName()];
      assert(v_ref.refcount > 0);
      cloneVar(v_ref, var);
    }

    // Start
    shards::start(p_wire, input);

    // And normally we just delegate the Mesh + SHFlow
    // the following will suspend this current wire
    // and in mesh tick when re-evaluated tick will
    // resume with the wire we just set above!
    shards::suspend(context, 0);

    // We will end here when we get resumed!

    // reset resumer
    p_wire->resumer = nullptr;

    return input;
  }
};

struct Recur : public WireBase {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  SHTypeInfo compose(const SHInstanceData &data) {
    // set current wire as `wire`
    _wwire = data.wire->shared_from_this();

    // find all variables to store in current wire
    // use vector in the end.. cos slightly faster
    _vars.clear();
    for (auto &shared : data.shared) {
      if (!shared.global) {
        SHVar ctxVar{};
        ctxVar.valueType = ContextVar;
        ctxVar.payload.stringValue = shared.name;
        auto &p = _vars.emplace_back();
        p = ctxVar;
      }
    }
    _len = _vars.size();
    return WireBase::compose(data);
  }

  void warmup(SHContext *ctx) {
    _storage.resize(_len);
    for (auto &v : _vars) {
      v.warmup(ctx);
    }
    auto swire = _wwire.lock();
    assert(swire);
    _wire = swire.get();
  }

  void cleanup() {
    for (auto &v : _vars) {
      v.cleanup();
    }
    // force releasing resources
    for (size_t i = 0; i < _len; i++) {
      // must release on capacity
      for (uint32_t j = 0; j < _storage[i].cap; j++) {
        destroyVar(_storage[i].elements[j]);
      }
      arrayFree(_storage[i]);
    }
    _storage.resize(0);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    // store _vars
    for (size_t i = 0; i < _len; i++) {
      const auto len = _storage[i].len;
      arrayResize(_storage[i], len + 1);
      cloneVar(_storage[i].elements[len], _vars[i].get());
    }

    // (Do self)
    // Run within the root flow
    auto runRes = runSubWire(_wire, context, input);
    if (unlikely(runRes.state == Failed)) {
      // meaning there was an exception while
      // running the sub wire, stop the parent too
      context->stopFlow(runRes.output);
    }

    // restore _vars
    for (size_t i = 0; i < _len; i++) {
      auto pops = arrayPop<SHSeq, SHVar>(_storage[i]);
      cloneVar(_vars[i].get(), pops);
    }

    return runRes.output;
  }

  std::weak_ptr<SHWire> _wwire;
  SHWire *_wire;
  std::deque<ParamVar> _vars;
  size_t _len; // cache it to have nothing on stack from us
  std::vector<SHSeq> _storage;
};

struct BaseRunner : public WireBase {
  std::deque<ParamVar> _vars;

  SHExposedTypesInfo _mergedReqs;
  void destroy() {
    arrayFree(_mergedReqs);
    WireBase::destroy();
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    // Start/Resume need to capture all it needs, so we need deeper informations
    // this is triggered by populating requiredVariables variable
    auto dataCopy = data;
    std::unordered_map<std::string_view, SHExposedTypeInfo> requirements;
    if (capturing) {
      dataCopy.requiredVariables = &requirements;
    }

    auto res = WireBase::compose(dataCopy);

    if (capturing) {
      // build the list of variables to capture and inject into spawned chain
      _vars.clear();
      arrayResize(_mergedReqs, 0);
      for (auto &avail : data.shared) {
        auto it = requirements.find(avail.name);
        if (it != requirements.end() && !avail.global) {
          SHLOG_TRACE("Detach: adding variable to requirements: {}, wire {}", avail.name, wire->name);
          SHVar ctxVar{};
          ctxVar.valueType = ContextVar;
          ctxVar.payload.stringValue = avail.name;
          auto &p = _vars.emplace_back();
          p = ctxVar;

          arrayPush(_mergedReqs, it->second);
        }
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

  SHExposedTypesInfo requiredVariables() { return capturing ? _mergedReqs : SHExposedTypesInfo(wireValidation.requiredInfo); }

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
      } else {
        shards::stop(wire.get());
      }
    }

    WireBase::cleanup();
  }

  void warmup(SHContext *ctx) {
    if (capturing) {
      for (auto &v : _vars) {
        v.warmup(ctx);
      }

      wire->onStop.clear();
      wire->onStop.emplace_back([this]() {
        for (auto &v : _vars) {
          // notice, this should be already destroyed by the wire releaseVariable
          destroyVar(wire->variables[v.variableName()]);
        }
      });
    }
  }

  void doWarmup(SHContext *context) {
    if (mode == RunWireMode::Inline && wire && wire->wireUsers.count(this) == 0) {
      wire->wireUsers.emplace(this);
      wire->warmup(context);
    }
  }

  void activateDetached(SHContext *context, const SHVar &input) {
    if (capturing) {
      for (auto &v : _vars) {
        auto &var = v.get();
        cloneVar(wire->variables[v.variableName()], var);
      }
    }

    if (!shards::isRunning(wire.get())) {
      // validated during infer not here! (false)
      auto mesh = context->main->mesh.lock();
      if (mesh)
        mesh->schedule(wire, input, false);
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
    }

    // Tick the wire on the flow that this Step wire created
    SHDuration now = SHClock::now().time_since_epoch();
    shards::tick(wire->context->flow->wire, now, input);
  }
};

template <bool INPUT_PASSTHROUGH, RunWireMode WIRE_MODE> struct RunWire : public BaseRunner {
  void setup() {
    passthrough = INPUT_PASSTHROUGH;
    mode = WIRE_MODE;
    capturing = WIRE_MODE == RunWireMode::Detached;
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
    if (unlikely(runRes.state == Failed)) {
      // meaning there was an exception while
      // running the sub wire, stop the parent too
      context->stopFlow(runRes.output);
      return runRes.output;
    } else {
      if (runRes.state == Restarted) {
        inputCopy = context->getFlowStorage();
        context->continueFlow();
        SH_SUSPEND(context, 0.0);
        goto run_wire_loop;
      } else if (context->shouldContinue()) {
        SH_SUSPEND(context, 0.0);
        goto run_wire_loop;
      } else {
        // we don't want to propagate a (Return)
        if (unlikely(runRes.state == Stopped)) {
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
    if constexpr (WIRE_MODE == RunWireMode::Detached) {
      activateDetached(context, input);
      return input;
    } else if constexpr (WIRE_MODE == RunWireMode::Stepped) {
      activateStepMode(context, input);
      if constexpr (INPUT_PASSTHROUGH) {
        return input;
      } else {
        return wire->previousOutput;
      }
    } else {
      // Run within the root flow
      auto runRes = runSubWire(wire.get(), context, input);
      if (unlikely(runRes.state == Failed)) {
        // meaning there was an exception while
        // running the sub wire, stop the parent too
        context->stopFlow(runRes.output);
        return runRes.output;
      } else {
        // we don't want to propagate a (Return)
        if (unlikely(runRes.state == Stopped)) {
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
};

struct WireNotFound : public ActivationError {
  WireNotFound() : ActivationError("Could not find a wire to run") {}
};

template <class T> struct BaseLoader : public BaseRunner {
  SHTypeInfo _inputTypeCopy{};
  IterableExposedInfo _sharedCopy;

  SHTypeInfo compose(const SHInstanceData &data) {
    BaseRunner::compose(data);

    _inputTypeCopy = data.inputType;
    const IterableExposedInfo sharedStb(data.shared);
    // copy shared
    _sharedCopy = sharedStb;

    if (mode == RunWireMode::Inline || mode == RunWireMode::Stepped) {
      // If inline allow wires to receive a result
      return CoreInfo::AnyType;
    } else {
      return data.inputType;
    }
  }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 1:
      mode = RunWireMode(value.payload.enumValue);
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 1:
      return Var::Enum(mode, CoreCC, 'runC');
    default:
      break;
    }
    return Var::Empty;
  }

  void cleanup() { BaseRunner::cleanup(); }

  SHVar activateWire(SHContext *context, const SHVar &input) {
    if (unlikely(!wire))
      throw WireNotFound();

    if (mode == RunWireMode::Detached) {
      activateDetached(context, input);
    } else if (mode == RunWireMode::Stepped) {
      activateStepMode(context, input);
      return wire->previousOutput;
    } else {
      // Run within the root flow
      const auto runRes = runSubWire(wire.get(), context, input);
      if (likely(runRes.state != Failed)) {
        return runRes.output;
      }
    }

    return input;
  }
};

struct WireLoader : public BaseLoader<WireLoader> {
  ShardsVar _onReloadShards{};
  ShardsVar _onErrorShards{};

  static inline Parameters params{{"Provider", SHCCSTR("The shards wire provider."), {WireProvider::ProviderOrNone}},
                                  {"Mode",
                                   SHCCSTR("The way to run the wire. Inline: will run the sub wire "
                                           "inline within the root wire, a pause in the child wire will "
                                           "pause the root too; Detached: will run the wire separately in "
                                           "the same mesh, a pause in this wire will not pause the root; "
                                           "Stepped: the wire will run as a child, the root will tick the "
                                           "wire every activation of this shard and so a child pause "
                                           "won't pause the root."),
                                   {ModeType}},
                                  {"OnReload",
                                   SHCCSTR("Shards to execute when the wire is reloaded, the input of "
                                           "this flow will be the reloaded wire."),
                                   {CoreInfo::ShardsOrNone}},
                                  {"OnError",
                                   SHCCSTR("Shards to execute when a wire reload failed, the input of "
                                           "this flow will be the error message."),
                                   {CoreInfo::ShardsOrNone}}};

  static SHParametersInfo parameters() { return params; }

  SHWireProvider *_provider;

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0: {
      cleanup(); // stop current
      if (value.valueType == Object) {
        _provider = (SHWireProvider *)value.payload.objectValue;
      } else {
        _provider = nullptr;
      }
    } break;
    case 1: {
      BaseLoader<WireLoader>::setParam(index, value);
    } break;
    case 2: {
      _onReloadShards = value;
    } break;
    case 3: {
      _onErrorShards = value;
    } break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      if (_provider) {
        return Var::Object(_provider, CoreCC, 'chnp');
      } else {
        return Var();
      }
    case 1:
      return BaseLoader<WireLoader>::getParam(index);
    case 2:
      return _onReloadShards;
    case 3:
      return _onErrorShards;
    default: {
      return Var::Empty;
    }
    }
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    SHInstanceData data2 = data;
    data2.inputType = CoreInfo::WireType;
    _onReloadShards.compose(data2);
    _onErrorShards.compose(data);
    return BaseLoader<WireLoader>::compose(data);
  }

  void cleanup() {
    BaseLoader<WireLoader>::cleanup();
    _onReloadShards.cleanup();
    _onErrorShards.cleanup();
    if (_provider)
      _provider->reset(_provider);
  }

  void warmup(SHContext *context) {
    BaseLoader<WireLoader>::warmup(context);
    _onReloadShards.warmup(context);
    _onErrorShards.warmup(context);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (unlikely(!_provider))
      return input;

    if (unlikely(!_provider->ready(_provider))) {
      SHInstanceData data{};
      data.inputType = _inputTypeCopy;
      data.shared = _sharedCopy;
      data.wire = context->wireStack.back();
      assert(data.wire->mesh.lock());
      _provider->setup(_provider, GetGlobals().RootPath.c_str(), data);
    }

    if (unlikely(_provider->updated(_provider))) {
      auto update = _provider->acquire(_provider);
      if (unlikely(update.error != nullptr)) {
        SHLOG_ERROR("Failed to reload a wire via WireLoader, reason: {}", update.error);
        SHVar output{};
        _onErrorShards.activate(context, Var(update.error), output);
      } else {
        if (wire) {
          // stop and release previous version
          shards::stop(wire.get());
        }

        // but let the provider release the pointer!
        wire.reset(update.wire, [&](SHWire *x) { _provider->release(_provider, x); });
        doWarmup(context);
        SHLOG_INFO("Wire {} has been reloaded", update.wire->name);
        SHVar output{};
        _onReloadShards.activate(context, Var(wire), output);
      }
    }

    try {
      return BaseLoader<WireLoader>::activateWire(context, input);
    } catch (const WireNotFound &ex) {
      // let's ignore wire not found in this case
      return input;
    }
  }
};

struct WireRunner : public BaseLoader<WireRunner> {
  static inline Parameters params{{"Wire", SHCCSTR("The wire variable to compose and run."), {CoreInfo::WireVarType}},
                                  {"Mode",
                                   SHCCSTR("The way to run the wire. Inline: will run the sub wire "
                                           "inline within the root wire, a pause in the child wire will "
                                           "pause the root too; Detached: will run the wire separately in "
                                           "the same mesh, a pause in this wire will not pause the root; "
                                           "Stepped: the wire will run as a child, the root will tick the "
                                           "wire every activation of this shard and so a child pause "
                                           "won't pause the root."),
                                   {ModeType}}};

  static SHParametersInfo parameters() { return params; }

  ParamVar _wire{};
  SHVar _wireHash{};
  SHWire *_wirePtr = nullptr;
  SHExposedTypeInfo _requiredWire{};

  void setParam(int index, const SHVar &value) {
    if (index == 0) {
      _wire = value;
    } else {
      BaseLoader<WireRunner>::setParam(index, value);
    }
  }

  SHVar getParam(int index) {
    if (index == 0) {
      return _wire;
    } else {
      return BaseLoader<WireRunner>::getParam(index);
    }
  }

  void cleanup() {
    BaseLoader<WireRunner>::cleanup();
    _wire.cleanup();
    _wirePtr = nullptr;
  }

  void warmup(SHContext *context) {
    BaseLoader<WireRunner>::warmup(context);
    _wire.warmup(context);
  }

  SHExposedTypesInfo requiredVariables() {
    if (_wire.isVariable()) {
      _requiredWire = SHExposedTypeInfo{_wire.variableName(), SHCCSTR("The wire to run."), CoreInfo::WireType};
      return {&_requiredWire, 1, 0};
    } else {
      return {};
    }
  }

  void doCompose(SHContext *context) {
    SHInstanceData data{};
    data.inputType = _inputTypeCopy;
    data.shared = _sharedCopy;
    data.wire = context->wireStack.back();
    wire->mesh = context->main->mesh;

    // avoid stack-overflow
    if (gatheringWires().count(wire.get()))
      return; // we don't know yet...

    gatheringWires().insert(wire.get());
    DEFER(gatheringWires().erase(wire.get()));

    // We need to validate the sub wire to figure it out!
    auto res = composeWire(
        wire.get(),
        [](const Shard *errorShard, const char *errorTxt, bool nonfatalWarning, void *userData) {
          if (!nonfatalWarning) {
            SHLOG_ERROR("RunWire: failed inner wire validation, error: {}", errorTxt);
            throw SHException("RunWire: failed inner wire validation");
          } else {
            SHLOG_INFO("RunWire: warning during inner wire validation: {}", errorTxt);
          }
        },
        this, data);

    shards::arrayFree(res.exposedInfo);
    shards::arrayFree(res.requiredInfo);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto wireVar = _wire.get();
    wire = SHWire::sharedFromRef(wireVar.payload.wireValue);
    if (unlikely(!wire))
      return input;

    if (_wireHash.valueType == None || _wireHash != wire->composedHash || _wirePtr != wire.get()) {
      // Compose and hash in a thread
      await(
          context,
          [this, context, wireVar]() {
            doCompose(context);
            wire->composedHash = shards::hash(wireVar);
          },
          [] {});

      _wireHash = wire->composedHash;
      _wirePtr = wire.get();
      doWarmup(context);
    }

    return BaseLoader<WireRunner>::activateWire(context, input);
  }
};

enum class WaitUntil {
  FirstSuccess, // will wait until the first success and stop any other
                // pending operation
  AllSuccess,   // will wait until all complete, will stop and fail on any
                // failure
  SomeSuccess   // will wait until all complete but won't fail if some of the
                // wires failed
};

struct ManyWire : public std::enable_shared_from_this<ManyWire> {
  uint32_t index;
  std::shared_ptr<SHWire> wire;
  std::shared_ptr<SHMesh> mesh; // used only if MT
  bool done;
};

struct ParallelBase : public WireBase {
  typedef EnumInfo<WaitUntil> WaitUntilInfo;
  static inline WaitUntilInfo waitUntilInfo{"WaitUntil", CoreCC, 'tryM'};
  static inline Type WaitUntilType{{SHType::Enum, {.enumeration = {.vendorId = CoreCC, .typeId = 'tryM'}}}};

  static inline Parameters _params{
      {"Wire", SHCCSTR("The wire to spawn and try to run many times concurrently."), WireBase::WireVarTypes},
      {"Policy", SHCCSTR("The execution policy in terms of wires success."), {WaitUntilType}},
      {"Threads", SHCCSTR("The number of cpu threads to use."), {CoreInfo::IntType}},
      {"Coroutines", SHCCSTR("The number of coroutines to run on each thread."), {CoreInfo::IntType}}};

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      wireref = value;
      break;
    case 1:
      _policy = WaitUntil(value.payload.enumValue);
      break;
    case 2:
      _threads = std::max(int64_t(1), value.payload.intValue);
      break;
    case 3:
      _coros = std::max(int64_t(1), value.payload.intValue);
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return wireref;
    case 1:
      return Var::Enum(_policy, CoreCC, 'tryM');
    case 2:
      return Var(_threads);
    case 3:
      return Var(_coros);
    default:
      return Var::Empty;
    }
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    if (_threads > 1) {
      mode = RunWireMode::Detached;
    } else {
      mode = RunWireMode::Inline;
    }

    WireBase::compose(data); // discard the result, we do our thing here

    _pool.reset(new WireDoppelgangerPool<ManyWire>(SHWire::weakRef(wire)));

    const IterableExposedInfo shared(data.shared);
    // copy shared
    _sharedCopy = shared;

    return CoreInfo::NoneType; // not complete
  }

  struct Composer {
    ParallelBase &server;
    SHContext *context;

    void compose(SHWire *wire) {
      SHInstanceData data{};
      data.inputType = server._inputType;
      data.shared = server._sharedCopy;
      data.wire = context->wireStack.back();
      wire->mesh = context->main->mesh;
      auto res = composeWire(
          wire,
          [](const struct Shard *errorShard, const char *errorTxt, SHBool nonfatalWarning, void *userData) {
            if (!nonfatalWarning) {
              SHLOG_ERROR(errorTxt);
              throw ActivationError("Http.Server handler wire compose failed");
            } else {
              SHLOG_WARNING(errorTxt);
            }
          },
          nullptr, data);
      arrayFree(res.exposedInfo);
      arrayFree(res.requiredInfo);
    }
  } _composer{*this};

  void warmup(SHContext *context) {
#if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
    if (_threads > 1) {
      const auto threads = std::min(_threads, int64_t(std::thread::hardware_concurrency()));
      if (!_exec || _exec->num_workers() != (size_t(threads))) {
        _exec.reset(new tf::Executor(size_t(threads)));
      }
    }
#endif
    _composer.context = context;
  }

  void cleanup() {
    for (auto &v : _outputs) {
      destroyVar(v);
    }
    _outputs.clear();
    for (auto &cref : _wires) {
      if (cref->mesh) {
        cref->mesh->terminate();
      }
      stop(cref->wire.get());
      _pool->release(cref);
    }
    _wires.clear();
  }

  virtual SHVar getInput(const std::shared_ptr<ManyWire> &mc, const SHVar &input) = 0;

  virtual size_t getLength(const SHVar &input) = 0;

  SHVar activate(SHContext *context, const SHVar &input) {
    auto mesh = context->main->mesh.lock();
    auto len = getLength(input);

    _outputs.resize(len);
    _wires.resize(len);
    Defer cleanups([this]() {
      for (auto &cref : _wires) {
        if (cref) {
          if (cref->mesh) {
            cref->mesh->terminate();
          }
          stop(cref->wire.get());
          _pool->release(cref);
        }
      }
      _wires.clear();
    });

    for (uint32_t i = 0; i < len; i++) {
      _wires[i] = _pool->acquire(_composer);
      _wires[i]->index = i;
      _wires[i]->done = false;
    }

    size_t succeeded = 0;
    size_t failed = 0;

    // wait according to policy
    while (true) {
      const auto _suspend_state = shards::suspend(context, 0);
      if (unlikely(_suspend_state != SHWireState::Continue)) {
        return Var::Empty;
      } else {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
        {
#else
        if (_threads == 1) {
#endif
          // advance our wires and check
          for (auto it = _wires.begin(); it != _wires.end(); ++it) {
            auto &cref = *it;
            if (cref->done)
              continue;

            // Prepare and start if no callc was called
            if (!cref->wire->coro) {
              cref->wire->mesh = context->main->mesh;
              // pre-set wire context with our context
              // this is used to copy wireStack over to the new one
              cref->wire->context = context;
              // Notice we don't share our flow!
              // let the wire create one by passing null
              shards::prepare(cref->wire.get(), nullptr);
              shards::start(cref->wire.get(), getInput(cref, input));
            }

            // Tick the wire on the flow that this wire created
            SHDuration now = SHClock::now().time_since_epoch();
            shards::tick(cref->wire->context->flow->wire, now, getInput(cref, input));

            if (!isRunning(cref->wire.get())) {
              if (cref->wire->state == SHWire::State::Ended) {
                if (_policy == WaitUntil::FirstSuccess) {
                  // success, next call clones, make sure to destroy
                  stop(cref->wire.get(), &_outputs[0]);
                  return _outputs[0];
                } else {
                  stop(cref->wire.get(), &_outputs[succeeded]);
                  succeeded++;
                }
              } else {
                stop(cref->wire.get());
                failed++;
              }
              cref->done = true;
            }
          }
        }
#if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
        else {
          // multithreaded
          tf::Taskflow flow;

          flow.for_each_dynamic(
              _wires.begin(), _wires.end(),
              [this, input](auto &cref) {
                // skip if failed or ended
                if (cref->done)
                  return;

                // Prepare and start if no callc was called
                if (!cref->wire->coro) {
                  if (!cref->mesh) {
                    cref->mesh = SHMesh::make();
                  }
                  cref->wire->mesh = cref->mesh;
                  // Notice we don't share our flow!
                  // let the wire create one by passing null
                  shards::prepare(cref->wire.get(), nullptr);
                  shards::start(cref->wire.get(), getInput(cref, input));
                }

                // Tick the wire on the flow that this wire created
                SHDuration now = SHClock::now().time_since_epoch();
                shards::tick(cref->wire->context->flow->wire, now, getInput(cref, input));
                // also tick the mesh
                cref->mesh->tick();
              },
              _coros);

          _exec->run(flow).get();

          for (auto it = _wires.begin(); it != _wires.end(); ++it) {
            auto &cref = *it;
            if (!cref->done && !isRunning(cref->wire.get())) {
              if (cref->wire->state == SHWire::State::Ended) {
                if (_policy == WaitUntil::FirstSuccess) {
                  // success, next call clones, make sure to destroy
                  stop(cref->wire.get(), &_outputs[0]);
                  return _outputs[0];
                } else {
                  stop(cref->wire.get(), &_outputs[succeeded]);
                  succeeded++;
                }
              } else {
                stop(cref->wire.get());
                failed++;
              }
              cref->done = true;
            }
          }
        }
#endif

        if ((succeeded + failed) == len) {
          if (unlikely(succeeded == 0)) {
            throw ActivationError("TryMany, failed all wires!");
          } else {
            // all ended let's apply policy here
            if (_policy == WaitUntil::SomeSuccess) {
              return Var(_outputs.data(), succeeded);
            } else {
              assert(_policy == WaitUntil::AllSuccess);
              if (len == succeeded) {
                return Var(_outputs.data(), succeeded);
              } else {
                throw ActivationError("TryMany, failed some wires!");
              }
            }
          }
        }
      }
    }
  }

protected:
  WaitUntil _policy{WaitUntil::AllSuccess};
  std::unique_ptr<WireDoppelgangerPool<ManyWire>> _pool;
  IterableExposedInfo _sharedCopy;
  Type _outputSeqType;
  Types _outputTypes;
  SHTypeInfo _inputType{};
  std::vector<SHVar> _outputs;
  std::vector<std::shared_ptr<ManyWire>> _wires;
  int64_t _threads{1};
  int64_t _coros{1};
#if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
  std::unique_ptr<tf::Executor> _exec;
#endif
};

struct TryMany : public ParallelBase {
  static SHTypesInfo inputTypes() { return CoreInfo::AnySeqType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnySeqType; }

  SHTypeInfo compose(const SHInstanceData &data) {
    ParallelBase::compose(data);

    if (data.inputType.seqTypes.len == 1) {
      // copy single input type
      _inputType = data.inputType.seqTypes.elements[0];
    } else {
      // else just mark as generic any
      _inputType = CoreInfo::AnyType;
    }

    if (_policy == WaitUntil::FirstSuccess) {
      // single result
      return wire->outputType;
    } else {
      // seq result
      _outputTypes = Types({wire->outputType});
      _outputSeqType = Type::SeqOf(_outputTypes);
      return _outputSeqType;
    }
  }

  SHVar getInput(const std::shared_ptr<ManyWire> &mc, const SHVar &input) override {
    return input.payload.seqValue.elements[mc->index];
  }

  size_t getLength(const SHVar &input) override { return size_t(input.payload.seqValue.len); }
};

struct Expand : public ParallelBase {
  int64_t _width{10};

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnySeqType; }

  static inline Parameters _params{{{"Size", SHCCSTR("The maximum expansion size."), {CoreInfo::IntType}}},
                                   ParallelBase::_params};

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    if (index == 0) {
      _width = std::max(int64_t(1), value.payload.intValue);
    } else {
      ParallelBase::setParam(index - 1, value);
    }
  }

  SHVar getParam(int index) {
    if (index == 0) {
      return Var(_width);
    } else {
      return ParallelBase::getParam(index - 1);
    }
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    ParallelBase::compose(data);

    // input
    _inputType = data.inputType;

    // output
    _outputTypes = Types({wire->outputType});
    _outputSeqType = Type::SeqOf(_outputTypes);
    return _outputSeqType;
  }

  SHVar getInput(const std::shared_ptr<ManyWire> &mc, const SHVar &input) override { return input; }

  size_t getLength(const SHVar &input) override { return size_t(_width); }
};

struct Spawn : public WireBase {
  Spawn() {
    mode = RunWireMode::Detached;
    capturing = true;
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::WireType; }

  static inline Parameters _params{
      {"Wire", SHCCSTR("The wire to spawn and try to run many times concurrently."), WireBase::WireVarTypes}};

  static SHParametersInfo parameters() { return _params; }

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
      return Var::Empty;
    }
  }

  SHExposedTypesInfo _mergedReqs;
  void destroy() {
    arrayFree(_mergedReqs);
    WireBase::destroy();
  }
  SHExposedTypesInfo requiredVariables() { return _mergedReqs; }

  SHTypeInfo compose(const SHInstanceData &data) {
    // Spawn needs to capture all it needs, so we need deeper informations
    // this is triggered by populating requiredVariables variable
    auto dataCopy = data;
    std::unordered_map<std::string_view, SHExposedTypeInfo> requirements;
    dataCopy.requiredVariables = &requirements;

    WireBase::compose(dataCopy); // discard the result, we do our thing here

    // build the list of variables to capture and inject into spawned chain
    _vars.clear();
    arrayResize(_mergedReqs, 0);
    for (auto &avail : data.shared) {
      auto it = requirements.find(avail.name);
      if (it != requirements.end() && !avail.global) {
        SHLOG_TRACE("Spawn: adding variable to requirements: {}, wire {}", avail.name, wire->name);
        SHVar ctxVar{};
        ctxVar.valueType = ContextVar;
        ctxVar.payload.stringValue = avail.name;
        auto &p = _vars.emplace_back();
        p = ctxVar;

        arrayPush(_mergedReqs, it->second);
      }
    }

    // wire should be populated now and such
    _pool.reset(new WireDoppelgangerPool<ManyWire>(SHWire::weakRef(wire)));

    // copy shared
    const IterableExposedInfo shared(data.shared);
    _sharedCopy = shared;
    // copy input type
    _inputType = data.inputType;

    return CoreInfo::WireType;
  }

  struct Composer {
    Spawn &server;
    SHContext *context;

    void compose(SHWire *wire) {
      SHLOG_TRACE("Spawn::Composer::compose {}", wire->name);
      SHInstanceData data{};
      data.inputType = server._inputType;
      data.shared = server._sharedCopy;
      data.wire = context->wireStack.back();
      wire->mesh = context->main->mesh;
      auto res = composeWire(
          wire,
          [](const struct Shard *errorShard, const char *errorTxt, SHBool nonfatalWarning, void *userData) {
            if (!nonfatalWarning) {
              SHLOG_ERROR(errorTxt);
              throw ActivationError("Spawn handler wire compose failed");
            } else {
              SHLOG_WARNING(errorTxt);
            }
          },
          nullptr, data);
      arrayFree(res.exposedInfo);
      arrayFree(res.requiredInfo);
    }
  } _composer{*this};

  void warmup(SHContext *context) {
    WireBase::warmup(context);

    _composer.context = context;

    for (auto &v : _vars) {
      v.warmup(context);
    }
  }

  void cleanup() {
    for (auto &v : _vars) {
      v.cleanup();
    }

    _composer.context = nullptr;

    WireBase::cleanup();
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto mesh = context->main->mesh.lock();
    auto c = _pool->acquire(_composer);
    c->wire->onStop.clear(); // we have a fresh recycled wire here
    std::weak_ptr<ManyWire> wc(c);
    c->wire->onStop.emplace_back([this, wc]() {
      if (auto c = wc.lock()) {
        for (auto &v : _vars) {
          // notice, this should be already destroyed by the wire releaseVariable
          destroyVar(c->wire->variables[v.variableName()]);
        }
        _pool->release(c);
      }
    });

    for (auto &v : _vars) {
      auto &var = v.get();
      cloneVar(c->wire->variables[v.variableName()], var);
    }

    mesh->schedule(c->wire, input, false);
    return Var(c->wire); // notice this is "weak"
  }

  std::unique_ptr<WireDoppelgangerPool<ManyWire>> _pool;
  IterableExposedInfo _sharedCopy;
  SHTypeInfo _inputType{};
  std::deque<ParamVar> _vars;
};

struct Branch {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static SHParametersInfo parameters() {
    static Parameters params{{"Wires",
                              SHCCSTR("The wires to schedule and run on this branch."),
                              {CoreInfo::WireType, CoreInfo::WireSeqType, CoreInfo::NoneType}}};
    return params;
  }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _wires = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _wires;
    default:
      return Var::Empty;
    }
  }

  SHOptionalString help() {
    return SHCCSTR("A branch is a child mesh that runs and is ticked when this shard is "
                   "activated, wires on this mesh will inherit all of the available "
                   "exposed variables in the activator wire.");
  }

  void composeSubWire(const SHInstanceData &data, SHWireRef &wireref) {
    auto wire = SHWire::sharedFromRef(wireref);
    wire->mesh = _mesh; // this is needed to be correct

    auto dataCopy = data;
    dataCopy.wire = wire.get();
    dataCopy.inputType = data.inputType;

    // Branch needs to capture all it needs, so we need deeper informations
    // this is triggered by populating requiredVariables variable
    std::unordered_map<std::string_view, SHExposedTypeInfo> requirements;
    dataCopy.requiredVariables = &requirements;

    auto cr = composeWire(
        wire.get(),
        [](const Shard *errorShard, const char *errorTxt, bool nonfatalWarning, void *userData) {
          if (!nonfatalWarning) {
            SHLOG_ERROR("Branch: failed inner wire validation, error: {}", errorTxt);
            throw ComposeError("RunWire: failed inner wire validation");
          } else {
            SHLOG_INFO("Branch: warning during inner wire validation: {}", errorTxt);
          }
        },
        this, dataCopy);

    _runWires.emplace_back(wire);

    // add to merged requirements
    for (auto &req : cr.requiredInfo) {
      arrayPush(_mergedReqs, req);
    }

    for (auto &avail : data.shared) {
      auto it = requirements.find(avail.name);
      if (it != requirements.end()) {
        SHLOG_TRACE("Branch: adding variable to requirements: {}, wire {}", avail.name, wire->name);
        arrayPush(_mergedReqs, it->second);
      }
    }

    arrayFree(cr.requiredInfo);
    arrayFree(cr.exposedInfo);
  }

  void destroy() { arrayFree(_mergedReqs); }

  SHTypeInfo compose(const SHInstanceData &data) {
    // release any old info
    destroy();

    _runWires.clear();
    if (_wires.valueType == SHType::Seq) {
      for (auto &wire : _wires) {
        composeSubWire(data, wire.payload.wireValue);
      }
    } else if (_wires.valueType == SHType::Wire) {
      composeSubWire(data, _wires.payload.wireValue);
    }

    const IterableExposedInfo shared(data.shared);
    // copy shared
    _sharedCopy = shared;
    _mesh->instanceData.shared = _sharedCopy;

    return data.inputType;
  }

  SHExposedTypesInfo requiredVariables() { return _mergedReqs; }

  void warmup(SHContext *context) {
    // grab all the variables we need and reference them
    for (const auto &req : _mergedReqs) {
      if (_mesh->refs.count(req.name) == 0) {
        SHLOG_TRACE("Branch: referencing required variable: {}", req.name);
        auto vp = referenceVariable(context, req.name);
        _mesh->refs[req.name] = vp;
      } else {
        SHLOG_TRACE("Branch: could not find required variable to reference: {}", req.name);
      }
    }

    for (const auto &wire : _runWires) {
      _mesh->schedule(wire, Var::Empty, false);
    }
  }

  void cleanup() {
    for (auto &[_, vp] : _mesh->refs) {
      releaseVariable(vp);
    }

    // this will also clear refs
    _mesh->terminate();
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (!_mesh->tick(input)) {
      // the mesh had errors in this case
      throw ActivationError("Branched mesh had errors");
    }
    return input;
  }

private:
  OwnedVar _wires{Var::Empty};
  std::shared_ptr<SHMesh> _mesh = SHMesh::make();
  IterableExposedInfo _sharedCopy;
  SHExposedTypesInfo _mergedReqs;
  std::vector<std::shared_ptr<SHWire>> _runWires;
};

void registerWiresShards() {
  using RunWireDo = RunWire<false, RunWireMode::Inline>;
  using RunWireDispatch = RunWire<true, RunWireMode::Inline>;
  using RunWireDetach = RunWire<true, RunWireMode::Detached>;
  using RunWireStep = RunWire<false, RunWireMode::Stepped>;
  REGISTER_SHARD("Resume", Resume);
  REGISTER_SHARD("Start", Start);
  REGISTER_SHARD("Wait", Wait);
  REGISTER_SHARD("Stop", StopWire);
  REGISTER_SHARD("Do", RunWireDo);
  REGISTER_SHARD("Dispatch", RunWireDispatch);
  REGISTER_SHARD("Detach", RunWireDetach);
  REGISTER_SHARD("Step", RunWireStep);
  REGISTER_SHARD("WireLoader", WireLoader);
  REGISTER_SHARD("WireRunner", WireRunner);
  REGISTER_SHARD("Recur", Recur);
  REGISTER_SHARD("TryMany", TryMany);
  REGISTER_SHARD("Spawn", Spawn);
  REGISTER_SHARD("Expand", Expand);
  REGISTER_SHARD("Branch", Branch);
}
}; // namespace shards

#ifndef __EMSCRIPTEN__
// this is a hack to fix a linker issue with taskflow...
/*
duplicate symbol 'thread-local initialization routine for
tf::Executor::_per_thread' in: libcb_static.a(wires.cpp.o)
    libcb_static.a(genetic.cpp.o)
*/
#include "genetic.hpp"

#endif