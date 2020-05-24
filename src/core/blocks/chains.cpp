/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#include "foundation.hpp"
#include "shared.hpp"
#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

namespace chainblocks {
enum RunChainMode { Inline, Detached, Stepped };

struct ChainBase {
  typedef EnumInfo<RunChainMode> RunChainModeInfo;
  static inline RunChainModeInfo runChainModeInfo{"RunChainMode", 'frag',
                                                  'runC'};
  static inline Type ModeType{
      {CBType::Enum, {.enumeration = {.vendorId = 'frag', .typeId = 'runC'}}}};

  static inline Types ChainTypes{
      {CoreInfo::ChainType, CoreInfo::StringType, CoreInfo::NoneType}};

  static inline Types ChainVarTypes{ChainTypes, {CoreInfo::ChainVarType}};

  static inline ParamsInfo waitChainParamsInfo = ParamsInfo(
      ParamsInfo::Param("Chain", "The chain to wait.", ChainVarTypes),
      ParamsInfo::Param("Once",
                        "Runs this sub-chain only once within the parent chain "
                        "execution cycle.",
                        CoreInfo::BoolType),
      ParamsInfo::Param("Passthrough",
                        "The input of this block will be the output.",
                        CoreInfo::BoolType));

  static inline ParamsInfo stopChainParamsInfo = ParamsInfo(
      ParamsInfo::Param("Chain", "The chain to wait.", ChainVarTypes),
      ParamsInfo::Param("Passthrough",
                        "The input of this block will be the output.",
                        CoreInfo::BoolType));

  static inline ParamsInfo runChainParamsInfo = ParamsInfo(
      ParamsInfo::Param("Chain", "The chain to run.", ChainTypes),
      ParamsInfo::Param("Once",
                        "Runs this sub-chain only once within the parent chain "
                        "execution cycle.",
                        CoreInfo::BoolType),
      ParamsInfo::Param(
          "Passthrough",
          "The input of this block will be the output. Not used if Detached.",
          CoreInfo::BoolType),
      ParamsInfo::Param(
          "Mode",
          "The way to run the chain. Inline: will run the sub chain inline "
          "within the root chain, a pause in the child chain will pause the "
          "root "
          "too; Detached: will run the chain separately in the same node, a "
          "pause in this chain will not pause the root; Stepped: the chain "
          "will "
          "run as a child, the root will tick the chain every activation of "
          "this "
          "block and so a child pause won't pause the root.",
          ModeType));

  static inline ParamsInfo chainloaderParamsInfo = ParamsInfo(
      ParamsInfo::Param(
          "File", "The chainblocks lisp file of the chain to run and watch.",
          CoreInfo::StringType),
      ParamsInfo::Param("Once",
                        "Runs this sub-chain only once within the parent chain "
                        "execution cycle.",
                        CoreInfo::BoolType),
      ParamsInfo::Param(
          "Mode",
          "The way to run the chain. Inline: will run the sub chain inline "
          "within the root chain, a pause in the child chain will pause the "
          "root "
          "too; Detached: will run the chain separately in the same node, a "
          "pause in this chain will not pause the root; Stepped: the chain "
          "will "
          "run as a child, the root will tick the chain every activation of "
          "this "
          "block and so a child pause won't pause the root.",
          ModeType));

  static inline ParamsInfo chainOnlyParamsInfo =
      ParamsInfo(ParamsInfo::Param("Chain", "The chain to run.", ChainTypes));

  ParamVar chainref{};
  std::shared_ptr<CBChain> chain;
  bool once{false};
  bool doneOnce{false};
  bool passthrough{false};
  RunChainMode mode{RunChainMode::Inline};
  CBValidationResult chainValidation{};

  void destroy() {
    chainblocks::arrayFree(chainValidation.exposedInfo);
    chainblocks::arrayFree(chainValidation.requiredInfo);
  }

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static inline thread_local std::set<CBChain *> visiting;

  CBTypeInfo compose(const CBInstanceData &data) {
    // Free any previous result!
    chainblocks::arrayFree(chainValidation.exposedInfo);
    chainblocks::arrayFree(chainValidation.requiredInfo);

    // Actualize the chain here, if we are deserialized
    // chain might already be populated!
    if (!chain) {
      if (chainref->valueType == CBType::Chain) {
        chain = CBChain::sharedFromRef(chainref->payload.chainValue);
      } else if (chainref->valueType == String) {
        chain = Globals::GlobalChains[chainref->payload.stringValue];
      } else {
        chain = nullptr;
        LOG(DEBUG) << "ChainBase::compose on a null chain";
      }
    }

    // Easy case, no chain...
    if (!chain)
      return data.inputType;

    assert(data.chain);

    if (chain.get() == data.chain) {
      LOG(DEBUG)
          << "ChainBase::compose early return, data.chain == chain, name: "
          << chain->name;
      return data.inputType; // we don't know yet...
    }

    chain->node = data.chain->node;

    auto node = data.chain->node.lock();
    assert(node);

    // TODO FIXME, chainloader/chain runner might access this from threads
    if (node->visitedChains.count(chain.get())) {
      // TODO FIXME, we need to verify input and shared here...
      // but visited does not mean composed...
      return node->visitedChains[chain.get()];
    }

    // avoid stackoverflow
    if (visiting.count(chain.get())) {
      LOG(DEBUG)
          << "ChainBase::compose early return, chain is being visited, name: "
          << chain->name;
      return data.inputType; // we don't know yet...
    }

    LOG(TRACE) << "ChainBase::compose, source: " << data.chain->name
               << " composing: " << chain->name
               << " input basic type: " << type2Name(data.inputType.basicType);

    // we can add early in this case!
    // useful for Resume/Start
    if (passthrough) {
      auto [_, done] = node->visitedChains.emplace(chain.get(), data.inputType);
      if (done) {
        LOG(TRACE) << "Pre-Marking as composed: " << chain->name
                   << " ptr: " << chain.get();
      }
    } else if (mode == Stepped) {
      auto [_, done] =
          node->visitedChains.emplace(chain.get(), CoreInfo::AnyType);
      if (done) {
        LOG(TRACE) << "Pre-Marking as composed: " << chain->name
                   << " ptr: " << chain.get();
      }
    }

    // and the subject here
    visiting.insert(chain.get());
    DEFER(visiting.erase(chain.get()));

    auto dataCopy = data;
    dataCopy.chain = chain.get();

    CBTypeInfo chainOutput;
    // make sure to compose only once...
    if (chain->composedHash == 0) {
      LOG(TRACE) << "Running " << chain->name << " compose.";
      chainValidation = composeChain(
          chain.get(),
          [](const CBlock *errorBlock, const char *errorTxt,
             bool nonfatalWarning, void *userData) {
            if (!nonfatalWarning) {
              LOG(ERROR) << "RunChain: failed inner chain validation, error: "
                         << errorTxt;
              throw ComposeError("RunChain: failed inner chain validation");
            } else {
              LOG(INFO) << "RunChain: warning during inner chain validation: "
                        << errorTxt;
            }
          },
          this, dataCopy);
      chain->composedHash = 1; // no need to hash properly here
      chainOutput = chainValidation.outputType;
      LOG(TRACE) << "Chain " << chain->name << " composed.";
    } else {
      LOG(TRACE) << "Skipping " << chain->name << " compose.";
      // verify input type
      if (!passthrough && mode != Stepped &&
          data.inputType != chain->inputType) {
        LOG(ERROR) << "Previous chain composed type "
                   << type2Name(chain->inputType.basicType)
                   << " requested call type "
                   << type2Name(data.inputType.basicType);
        throw ComposeError("Attempted to call an already composed chain with a "
                           "different input type! chain: " +
                           chain->name);
      }

      // write output type
      chainOutput = chain->outputType;

      // ensure requirements match our input data
      IterableExposedInfo shared(data.shared);
      for (auto req : chain->requiredVariables) {
        // find each in shared
        auto res = std::find_if(shared.begin(), shared.end(),
                                [&](CBExposedTypeInfo &x) {
                                  std::string_view vx(x.name);
                                  return req == vx;
                                });
        if (res == shared.end()) {
          throw ComposeError("Attempted to call an already composed chain (" +
                             chain->name +
                             ") with "
                             "a missing required variable: " +
                             req);
        }
      }
    }

    auto outputType = data.inputType;

    if (!passthrough) {
      if (mode == Inline)
        outputType = chainOutput;
      else if (mode == Stepped)
        outputType = CoreInfo::AnyType; // unpredictable
      else
        outputType = data.inputType;
    }

    if (!passthrough && mode != Stepped) {
      auto [_, done] = node->visitedChains.emplace(chain.get(), outputType);
      if (done) {
        LOG(TRACE) << "Marking as composed: " << chain->name
                   << " ptr: " << chain.get() << " inputType "
                   << type2Name(chain->inputType.basicType) << " outputType "
                   << type2Name(chain->outputType.basicType);
      }
    }

    return outputType;
  }

  void cleanup() { chainref.cleanup(); }

  void warmup(CBContext *ctx) { chainref.warmup(ctx); }

  // Use state to mark the dependency for serialization as well!

  CBVar getState() {
    if (chain) {
      return Var(chain);
    } else {
      LOG(TRACE) << "getState no chain was avail";
      return Var::Empty;
    }
  }

  void setState(CBVar state) {
    if (state.valueType == CBType::Chain) {
      chain = CBChain::sharedFromRef(state.payload.chainValue);
    }
  }
};

struct WaitChain : public ChainBase {
  OwnedVar _output{};
  CBExposedTypeInfo _requiredChain{};

  void cleanup() {
    if (chainref.isVariable())
      chain = nullptr;
    ChainBase::cleanup();
    doneOnce = false;
  }

  static CBParametersInfo parameters() {
    return CBParametersInfo(waitChainParamsInfo);
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      chainref = value;
      break;
    case 1:
      once = value.payload.boolValue;
      break;
    case 2:
      passthrough = value.payload.boolValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return chainref;
    case 1:
      return Var(once);
    case 2:
      return Var(passthrough);
    default:
      return Var::Empty;
    }
  }

  CBExposedTypesInfo requiredVariables() {
    if (chainref.isVariable()) {
      _requiredChain = CBExposedTypeInfo{chainref.variableName(),
                                         "The chain to run.",
                                         CoreInfo::ChainType,
                                         false,
                                         false,
                                         false};
      return {&_requiredChain, 1, 0};
    } else {
      return {};
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(!chain && chainref.isVariable())) {
      auto vchain = chainref.get();
      if (vchain.valueType == CBType::Chain) {
        chain = CBChain::sharedFromRef(vchain.payload.chainValue);
      } else if (vchain.valueType == String) {
        chain = Globals::GlobalChains[vchain.payload.stringValue];
      } else {
        chain = nullptr;
      }
    }

    if (unlikely(!chain)) {
      LOG(WARNING) << "WaitChain's chain is void.";
      return input;
    } else if (!doneOnce) {
      if (once)
        doneOnce = true;

      while (isRunning(chain.get())) {
        suspend(context, 0);
      }

      if (passthrough) {
        return input;
      } else {
        // ownedvar, will clone
        _output = chain->finishedOutput;
        return _output;
      }
    } else {
      return input;
    }
  }
};

struct StopChain : public ChainBase {
  void setup() { passthrough = true; }

  OwnedVar _output{};
  CBExposedTypeInfo _requiredChain{};

  void cleanup() {
    if (chainref.isVariable())
      chain = nullptr;
    ChainBase::cleanup();
  }

  static CBParametersInfo parameters() {
    return CBParametersInfo(stopChainParamsInfo);
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      chainref = value;
      break;
    case 1:
      passthrough = value.payload.boolValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return chainref;
    case 1:
      return Var(passthrough);
    default:
      return Var::Empty;
    }
  }

  CBExposedTypesInfo requiredVariables() {
    if (chainref.isVariable()) {
      _requiredChain = CBExposedTypeInfo{chainref.variableName(),
                                         "The chain to run.",
                                         CoreInfo::ChainType,
                                         false,
                                         false,
                                         false};
      return {&_requiredChain, 1, 0};
    } else {
      return {};
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(!chain && chainref.isVariable())) {
      auto vchain = chainref.get();
      if (vchain.valueType == CBType::Chain) {
        chain = CBChain::sharedFromRef(vchain.payload.chainValue);
      } else if (vchain.valueType == String) {
        chain = Globals::GlobalChains[vchain.payload.stringValue];
      } else {
        chain = nullptr;
      }
    }

    if (unlikely(!chain)) {
      LOG(WARNING) << "StopChain's chain is void.";
      return input;
    } else {
      chainblocks::stop(chain.get());
      if (passthrough) {
        return input;
      } else {
        _output = chain->finishedOutput;
        return _output;
      }
    }
  }
};

struct Resume : public ChainBase {
  void setup() { passthrough = true; }

  static inline ParamsInfo params = ParamsInfo(ParamsInfo::Param(
      "Chain", "The name of the chain to switch to.", ChainTypes));

  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  CBTypeInfo compose(const CBInstanceData &data) {
    passthrough = true;
    ChainBase::compose(data);
    return data.inputType;
  }

  void setParam(int index, CBVar value) { chainref = value; }

  CBVar getParam(int index) { return chainref; }

  // NO cleanup, other chains reference this chain but should not stop it
  // An arbitrary chain should be able to resume it!
  // Cleanup mechanics still to figure, for now ref count of the actual chain
  // symbol, TODO maybe use CBVar refcount!

  CBVar activate(CBContext *context, const CBVar &input) {
    if (!chain) {
      throw ActivationError("Resume, chain not found.");
    }

    // assign the new chain as current chain on the flow
    context->flow->chain = chain.get();

    // Allow to re run chains
    if (chainblocks::hasEnded(chain.get())) {
      chainblocks::stop(chain.get());
    }

    // Prepare if no callc was called
    if (!chain->coro) {
      chain->node = context->main->node;
      chainblocks::prepare(chain.get(), context->flow);
    }

    // Start it if not started
    if (!chainblocks::isRunning(chain.get())) {
      chainblocks::start(chain.get(), input);
    }

    // And normally we just delegate the CBNode + CBFlow
    // the following will suspend this current chain
    // and in node tick when re-evaluated tick will
    // resume with the chain we just set above!
    chainblocks::suspend(context, 0);

    return input;
  }
};

struct Start : public Resume {
  void setup() { passthrough = true; }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (!chain) {
      throw ActivationError("Start, chain not found.");
    }

    // assign the new chain as current chain on the flow
    context->flow->chain = chain.get();

    // ensure chain is not running, we start from top
    chainblocks::stop(chain.get());

    // Prepare
    chain->node = context->main->node;
    chainblocks::prepare(chain.get(), context->flow);

    // Start
    chainblocks::start(chain.get(), input);

    // And normally we just delegate the CBNode + CBFlow
    // the following will suspend this current chain
    // and in node tick when re-evaluated tick will
    // resume with the chain we just set above!
    chainblocks::suspend(context, 0);

    return input;
  }
};

struct Recur : public ChainBase {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  CBTypeInfo compose(const CBInstanceData &data) {
    // set current chain as `chain`
    _wchain = data.chain->shared_from_this();

    // find all variables to store in current chain
    // use vector in the end.. cos slightly faster
    IterableExposedInfo shares(data.shared);
    for (CBExposedTypeInfo &shared : shares) {
      if (shared.scope == data.chain && shared.isMutable) {
        CBVar ctxVar{};
        ctxVar.valueType = ContextVar;
        ctxVar.payload.stringValue = shared.name;
        auto &p = _vars.emplace_back();
        p = ctxVar;
      }
    }
    _len = _vars.size();
    return ChainBase::compose(data);
  }

  void warmup(CBContext *ctx) {
    _storage.resize(_len);
    for (auto &v : _vars) {
      v.warmup(ctx);
    }
    auto schain = _wchain.lock();
    assert(schain);
    _chain = schain.get();
  }

  void cleanup() {
    for (auto &v : _vars) {
      v.cleanup();
    }
    // force releasing resources
    for (size_t i = 0; i < _len; i++) {
      IterableSeq s(_storage[i]);
      for (auto &v : s) {
        destroyVar(v);
      }
      arrayFree(_storage[i]);
    }
    _storage.resize(0);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    // store _vars
    for (size_t i = 0; i < _len; i++) {
      const auto len = _storage[i].len;
      arrayResize(_storage[i], len + 1);
      cloneVar(_storage[i].elements[len], _vars[i].get());
    }

    // (Do self)
    // Run within the root flow
    auto runRes = runSubChain(_chain, context, input);
    if (unlikely(runRes.state == Failed)) {
      // meaning there was an exception while
      // running the sub chain, stop the parent too
      context->stopFlow(runRes.output);
    }

    // restore _vars
    for (size_t i = 0; i < _len; i++) {
      auto pops = arrayPop<CBSeq, CBVar>(_storage[i]);
      cloneVar(_vars[i].get(), pops);
    }

    return runRes.output;
  }

  std::weak_ptr<CBChain> _wchain;
  CBChain *_chain;
  std::deque<ParamVar> _vars;
  size_t _len; // cache it to have nothing on stack from us
  std::vector<CBSeq> _storage;
};

struct BaseRunner : public ChainBase {
  // Only chain runners should expose varaibles to the context
  CBExposedTypesInfo exposedVariables() {
    // Only inline mode ensures that variables will be really avail
    // step and detach will run at different timing
    CBExposedTypesInfo empty{};
    return mode == RunChainMode::Inline ? chainValidation.exposedInfo : empty;
  }

  void cleanup() {
    if (chain) {
      if (mode == RunChainMode::Inline && chain->chainUsers.count(this) != 0) {
        chain->chainUsers.erase(this);
        chain->cleanup();
      } else {
        chainblocks::stop(chain.get());
      }
    }
    doneOnce = false;
    ChainBase::cleanup();
  }

  void doWarmup(CBContext *context) {
    if (mode == RunChainMode::Inline && chain &&
        chain->chainUsers.count(this) == 0) {
      chain->chainUsers.emplace(this);
      chain->warmup(context);
    }
  }

  void activateDetached(CBContext *context, const CBVar &input) {
    if (!chainblocks::isRunning(chain.get())) {
      // validated during infer not here! (false)
      auto node = context->main->node.lock();
      if (node)
        node->schedule(chain, input, false);
    }
  }

  void activateStepMode(CBContext *context, const CBVar &input) {
    // Allow to re run chains
    if (chainblocks::hasEnded(chain.get())) {
      // stop the root
      if (!chainblocks::stop(chain.get())) {
        throw ActivationError("Stepped sub-chain did not end normally.");
      }
    }

    // Prepare if no callc was called
    if (!chain->coro) {
      chain->node = context->main->node;
      // Notice we don't share our flow!
      // let the chain create one by passing null
      chainblocks::prepare(chain.get(), nullptr);
    }

    // Starting
    if (!chainblocks::isRunning(chain.get())) {
      chainblocks::start(chain.get(), input);
    }

    // Tick the chain on the flow that this Step chain created
    chainblocks::tick(chain->context->flow->chain, input);
  }
};

struct RunChain : public BaseRunner {
  static CBParametersInfo parameters() {
    return CBParametersInfo(runChainParamsInfo);
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      chainref = value;
      break;
    case 1:
      once = value.payload.boolValue;
      break;
    case 2:
      passthrough = value.payload.boolValue;
      break;
    case 3:
      mode = RunChainMode(value.payload.enumValue);
      break;
    default:
      break;
    }
    // make sure to reset!
    cleanup();
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return chainref;
    case 1:
      return Var(once);
    case 2:
      return Var(passthrough);
    case 3:
      return Var::Enum(mode, 'frag', 'runC');
    default:
      break;
    }
    return Var();
  }

  void warmup(CBContext *context) {
    ChainBase::warmup(context);
    doWarmup(context);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(!chain))
      return input;

    if (!doneOnce) {
      if (once)
        doneOnce = true;

      if (mode == RunChainMode::Detached) {
        activateDetached(context, input);
        return input;
      } else if (mode == RunChainMode::Stepped) {
        activateStepMode(context, input);
        return passthrough ? input : chain->previousOutput;
      } else {
        // Run within the root flow
        auto runRes = runSubChain(chain.get(), context, input);
        if (unlikely(runRes.state == Failed)) {
          // meaning there was an exception while
          // running the sub chain, stop the parent too
          context->stopFlow(runRes.output);
          return runRes.output;
        } else if (passthrough) {
          return input;
        } else {
          return runRes.output;
        }
      }
    } else {
      return input;
    }
  }
};

template <class T> struct BaseLoader : public BaseRunner {
  CBTypeInfo _inputTypeCopy{};
  IterableExposedInfo _sharedCopy;

  CBTypeInfo compose(const CBInstanceData &data) {
    _inputTypeCopy = data.inputType;
    const IterableExposedInfo sharedStb(data.shared);
    // copy shared
    _sharedCopy = sharedStb;
    return data.inputType;
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 1:
      once = value.payload.boolValue;
      break;
    case 2:
      mode = RunChainMode(value.payload.enumValue);
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 1:
      return Var(once);
    case 2:
      return Var::Enum(mode, 'frag', 'runC');
    default:
      break;
    }
    return Var();
  }

  void cleanup() { BaseRunner::cleanup(); }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(!chain))
      return input;

    if (!doneOnce) {
      if (once)
        doneOnce = true;

      if (mode == RunChainMode::Detached) {
        activateDetached(context, input);
        return input;
      } else if (mode == RunChainMode::Stepped) {
        activateStepMode(context, input);
        return input;
      } else {
        // Run within the root flow
        auto runRes = runSubChain(chain.get(), context, input);
        if (unlikely(runRes.state == Failed)) {
          context->stopFlow(input);
        }
        return input;
      }
    } else {
      return input;
    }
  }
};

struct ChainLoader : public BaseLoader<ChainLoader> {
  static inline ParamsInfo paramsInfo = ParamsInfo(
      ParamsInfo::Param("Provider", "The chainblocks chain provider.",
                        ChainProvider::ProviderOrNone),
      ParamsInfo::Param("Once",
                        "Runs this sub-chain only once within the parent chain "
                        "execution cycle.",
                        CoreInfo::BoolType),
      ParamsInfo::Param(
          "Mode",
          "The way to run the chain. Inline: will run the sub chain inline "
          "within the root chain, a pause in the child chain will pause the "
          "root "
          "too; Detached: will run the chain separately in the same node, a "
          "pause in this chain will not pause the root; Stepped: the chain "
          "will "
          "run as a child, the root will tick the chain every activation of "
          "this "
          "block and so a child pause won't pause the root.",
          ModeType));

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  CBChainProvider *_provider;

  void setParam(int index, CBVar value) {
    if (index == 0) {
      cleanup(); // stop current
      if (value.valueType == Object) {
        _provider = (CBChainProvider *)value.payload.objectValue;
      } else {
        _provider = nullptr;
      }
    } else {
      BaseLoader<ChainLoader>::setParam(index, value);
    }
  }

  CBVar getParam(int index) {
    if (index == 0) {
      if (_provider) {
        return Var::Object(_provider, 'frag', 'chnp');
      } else {
        return Var();
      }
    } else {
      return BaseLoader<ChainLoader>::getParam(index);
    }
  }

  void cleanup() {
    BaseLoader<ChainLoader>::cleanup();
    if (_provider)
      _provider->reset(_provider);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(!_provider))
      return input;

    if (unlikely(!_provider->ready(_provider))) {
      CBInstanceData data{};
      data.inputType = _inputTypeCopy;
      data.shared = _sharedCopy;
      data.chain = context->chainStack.back();
      assert(data.chain->node.lock());
      _provider->setup(_provider, Globals::RootPath.c_str(), data);
    }

    if (unlikely(_provider->updated(_provider))) {
      auto update = _provider->acquire(_provider);
      if (unlikely(update.error != nullptr)) {
        LOG(ERROR) << "Failed to reload a chain via ChainLoader, reason: "
                   << update.error;
      } else {
        if (chain) {
          // stop and release previous version
          chainblocks::stop(chain.get());
        }

        // but let the provider release the pointer!
        chain.reset(update.chain,
                    [&](auto &x) { _provider->release(_provider, x); });
        doWarmup(context);
      }
    }

    return BaseLoader<ChainLoader>::activate(context, input);
  }
};

struct ChainRunner : public BaseLoader<ChainRunner> {
  static inline ParamsInfo paramsInfo = ParamsInfo(
      ParamsInfo::Param("Chain", "The chain variable to compose and run.",
                        CoreInfo::ChainVarType),
      ParamsInfo::Param("Once",
                        "Runs this sub-chain only once within the parent chain "
                        "execution cycle.",
                        CoreInfo::BoolType),
      ParamsInfo::Param(
          "Mode",
          "The way to run the chain. Inline: will run the sub chain inline "
          "within the root chain, a pause in the child chain will pause the "
          "root "
          "too; Detached: will run the chain separately in the same node, a "
          "pause in this chain will not pause the root; Stepped: the chain "
          "will "
          "run as a child, the root will tick the chain every activation of "
          "this "
          "block and so a child pause won't pause the root.",
          ModeType));

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  ParamVar _chain{};
  std::size_t _chainHash = 0;
  CBChain *_chainPtr = nullptr;
  CBExposedTypeInfo _requiredChain{};

  void setParam(int index, CBVar value) {
    if (index == 0) {
      _chain = value;
    } else {
      BaseLoader<ChainRunner>::setParam(index, value);
    }
  }

  CBVar getParam(int index) {
    if (index == 0) {
      return _chain;
    } else {
      return BaseLoader<ChainRunner>::getParam(index);
    }
  }

  void cleanup() {
    BaseLoader<ChainRunner>::cleanup();
    _chain.cleanup();
    _chainPtr = nullptr;
  }

  void warmup(CBContext *context) {
    BaseLoader<ChainRunner>::warmup(context);
    _chain.warmup(context);
  }

  CBExposedTypesInfo requiredVariables() {
    if (_chain.isVariable()) {
      _requiredChain = CBExposedTypeInfo{_chain.variableName(),
                                         "The chain to run.",
                                         CoreInfo::ChainType,
                                         false,
                                         false,
                                         false};
      return {&_requiredChain, 1, 0};
    } else {
      return {};
    }
  }

  void doCompose(CBContext *context) {
    CBInstanceData data{};
    data.inputType = _inputTypeCopy;
    data.shared = _sharedCopy;
    data.chain = context->chainStack.back();
    chain->node = context->main->node;

    // avoid stackoverflow
    if (visiting.count(chain.get()))
      return; // we don't know yet...

    visiting.insert(chain.get());

    // We need to validate the sub chain to figure it out!
    auto res = composeChain(
        chain.get(),
        [](const CBlock *errorBlock, const char *errorTxt, bool nonfatalWarning,
           void *userData) {
          if (!nonfatalWarning) {
            LOG(ERROR) << "RunChain: failed inner chain validation, error: "
                       << errorTxt;
            throw CBException("RunChain: failed inner chain validation");
          } else {
            LOG(INFO) << "RunChain: warning during inner chain validation: "
                      << errorTxt;
          }
        },
        this, data);

    visiting.erase(chain.get());
    chainblocks::arrayFree(res.exposedInfo);
    chainblocks::arrayFree(res.requiredInfo);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto chainVar = _chain.get();
    chain = CBChain::sharedFromRef(chainVar.payload.chainValue);
    if (unlikely(!chain))
      return input;

    if (_chainHash == 0 || _chainHash != chain->composedHash ||
        _chainPtr != chain.get()) {
      // Compose and hash in a thread
      auto asyncRes =
          std::async(std::launch::async, [this, context, chainVar]() {
            doCompose(context);
            chain->composedHash = std::hash<CBVar>()(chainVar);
          });

      // Wait suspending!
      while (true) {
        auto state = asyncRes.wait_for(std::chrono::seconds(0));
        if (state == std::future_status::ready)
          break;

        chainblocks::suspend(context, 0);
      }

      // This should throw if we had exceptions
      asyncRes.get();

      _chainHash = chain->composedHash;
      _chainPtr = chain.get();
      doWarmup(context);
    }

    return BaseLoader<ChainRunner>::activate(context, input);
  }
};

void registerChainsBlocks() {
  REGISTER_CBLOCK("Resume", Resume);
  REGISTER_CBLOCK("Start", Start);
  REGISTER_CBLOCK("WaitChain", WaitChain);
  REGISTER_CBLOCK("StopChain", StopChain);
  REGISTER_CBLOCK("RunChain", RunChain);
  REGISTER_CBLOCK("ChainLoader", ChainLoader);
  REGISTER_CBLOCK("ChainRunner", ChainRunner);
  REGISTER_CBLOCK("Recur", Recur);
}
}; // namespace chainblocks
