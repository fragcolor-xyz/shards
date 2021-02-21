/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2021 Giovanni Petrantoni */

#include "foundation.hpp"
#include "shared.hpp"
#include <chrono>
#include <memory>
#include <set>
#if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
#include <taskflow/taskflow.hpp>
#endif

namespace chainblocks {
enum RunChainMode { Inline, Detached, Stepped };

struct ChainBase {
  typedef EnumInfo<RunChainMode> RunChainModeInfo;
  static inline RunChainModeInfo runChainModeInfo{"RunChainMode", CoreCC,
                                                  'runC'};
  static inline Type ModeType{
      {CBType::Enum, {.enumeration = {.vendorId = CoreCC, .typeId = 'runC'}}}};

  static inline Types ChainTypes{
      {CoreInfo::ChainType, CoreInfo::StringType, CoreInfo::NoneType}};

  static inline Types ChainVarTypes{ChainTypes, {CoreInfo::ChainVarType}};

  static inline Parameters WaitParamsInfo{
      {"Chain", CBCCSTR("The chain to wait."), {ChainVarTypes}},
      {"Passthrough",
       CBCCSTR("The input of this block will be the output."),
       {CoreInfo::BoolType}}};

  static inline Parameters stopChainParamsInfo{
      {"Chain", CBCCSTR("The chain to wait."), {ChainVarTypes}},
      {"Passthrough",
       CBCCSTR("The input of this block will be the output."),
       {CoreInfo::BoolType}}};

  static inline Parameters runChainParamsInfo{
      {"Chain", CBCCSTR("The chain to run."), {ChainTypes}}};

  ParamVar chainref{};
  std::shared_ptr<CBChain> chain;
  bool passthrough{false};
  RunChainMode mode{RunChainMode::Inline};
  CBComposeResult chainValidation{};
  IterableExposedInfo exposedInfo{};

  void destroy() {
    chainblocks::arrayFree(chainValidation.requiredInfo);
    chainblocks::arrayFree(chainValidation.exposedInfo);
  }

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static inline thread_local std::set<CBChain *> visiting;

  CBTypeInfo compose(const CBInstanceData &data) {
    // Free any previous result!
    chainblocks::arrayFree(chainValidation.requiredInfo);
    chainblocks::arrayFree(chainValidation.exposedInfo);

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
    if (chain->isRoot || visiting.count(chain.get())) {
      LOG(DEBUG)
          << "ChainBase::compose early return, chain is being visited, name: "
          << chain->name;
      return data.inputType; // we don't know yet...
    }

    LOG(TRACE) << "ChainBase::compose, source: " << data.chain->name
               << " composing: " << chain->name
               << " input type: " << data.inputType;

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
    IterableExposedInfo shared(data.shared);
    IterableExposedInfo sharedCopy;
    if (mode == RunChainMode::Detached) {
      // keep only globals
      auto end =
          std::remove_if(shared.begin(), shared.end(),
                         [](const CBExposedTypeInfo &x) { return !x.global; });
      sharedCopy = IterableExposedInfo(shared.begin(), end);
    } else {
      // we allow Detached but they need to be referenced during warmup
      sharedCopy = shared;
    }

    dataCopy.shared = sharedCopy;

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
      IterableExposedInfo exposing(chainValidation.exposedInfo);
      // keep only globals
      exposedInfo = IterableExposedInfo(
          exposing.begin(),
          std::remove_if(exposing.begin(), exposing.end(),
                         [](CBExposedTypeInfo &x) { return !x.global; }));
      LOG(TRACE) << "Chain " << chain->name << " composed.";
    } else {
      LOG(TRACE) << "Skipping " << chain->name << " compose.";
      // verify input type
      if (!passthrough && mode != Stepped &&
          data.inputType != chain->inputType) {
        LOG(ERROR) << "Previous chain composed type " << *chain->inputType
                   << " requested call type " << data.inputType;
        throw ComposeError("Attempted to call an already composed chain with a "
                           "different input type! chain: " +
                           chain->name);
      }

      // write output type
      chainOutput = chain->outputType;

      // ensure requirements match our input data
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
                   << *chain->inputType << " outputType " << chain->outputType;
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

struct Wait : public ChainBase {
  // we don't need OwnedVar here
  // we keep the chain referenced!
  CBVar _output{};
  CBExposedTypeInfo _requiredChain{};

  void cleanup() {
    if (chainref.isVariable())
      chain = nullptr;
    ChainBase::cleanup();
  }

  static CBParametersInfo parameters() { return WaitParamsInfo; }

  void setParam(int index, const CBVar &value) {
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
      _requiredChain =
          CBExposedTypeInfo{chainref.variableName(),
                            CBCCSTR("The chain to run."), CoreInfo::ChainType};
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
      LOG(WARNING) << "Wait's chain is void.";
      return input;
    } else {
      while (isRunning(chain.get())) {
        CB_SUSPEND(context, 0);
      }

      if (chain->finishedError.size() > 0) {
        // if the chain has errors we need to propagate them
        // we can avoid interruption using Maybe blocks
        throw ActivationError(chain->finishedError);
      }

      if (passthrough) {
        return input;
      } else {
        // no clone
        _output = chain->finishedOutput;
        return _output;
      }
    }
  }
};

struct StopChain : public ChainBase {
  void setup() { passthrough = true; }

  OwnedVar _output{};
  CBExposedTypeInfo _requiredChain{};

  CBTypeInfo _inputType{};

  CBTypeInfo compose(const CBInstanceData &data) {
    _inputType = data.inputType;
    ChainBase::compose(data);
    return data.inputType;
  }

  void composed(const CBChain *chain, const CBComposeResult *result) {
    if (!chain && chainref->valueType == None &&
        _inputType != result->outputType) {
      LOG(ERROR)
          << "Stop input and chain output type mismatch, Stop input must be "
             "the same type of the chain's output (regular flow), chain: "
          << chain->name << " expected: " << chain->outputType;
      throw ComposeError("Stop input and chain output type mismatch");
    }
  }

  void cleanup() {
    if (chainref.isVariable())
      chain = nullptr;
    ChainBase::cleanup();
  }

  static CBParametersInfo parameters() { return stopChainParamsInfo; }

  void setParam(int index, const CBVar &value) {
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
      _requiredChain =
          CBExposedTypeInfo{chainref.variableName(),
                            CBCCSTR("The chain to run."), CoreInfo::ChainType};
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
      // in this case we stop the current chain
      context->stopFlow(input);
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
  void setup() {
    // we use those during ChainBase::compose
    passthrough = true;
    mode = Detached;
  }

  static inline Parameters params{
      {"Chain", CBCCSTR("The name of the chain to switch to."), {ChainTypes}}};

  static CBParametersInfo parameters() { return params; }

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  CBTypeInfo compose(const CBInstanceData &data) {
    ChainBase::compose(data);
    return data.inputType;
  }

  void setParam(int index, const CBVar &value) { chainref = value; }

  CBVar getParam(int index) { return chainref; }

  // NO cleanup, other chains reference this chain but should not stop it
  // An arbitrary chain should be able to resume it!
  // Cleanup mechanics still to figure, for now ref count of the actual chain
  // symbol, TODO maybe use CBVar refcount!

  CBVar activate(CBContext *context, const CBVar &input) {
    auto current = context->chainStack.back();

    auto pchain = [&] {
      if (!chain) {
        if (current->resumer) {
          return current->resumer;
        } else {
          throw ActivationError("Resume, chain not found.");
        }
      } else {
        return chain.get();
      }
    }();

    // assign the new chain as current chain on the flow
    context->flow->chain = pchain;

    // Allow to re run chains
    if (chainblocks::hasEnded(pchain)) {
      chainblocks::stop(pchain);
    }

    // Prepare if no callc was called
    if (!pchain->coro) {
      pchain->node = context->main->node;
      chainblocks::prepare(pchain, context->flow);
    }

    // we should be valid as this block should be dependent on current
    // do this here as stop/prepare might overwrite
    pchain->resumer = current;

    // Start it if not started
    if (!chainblocks::isRunning(pchain)) {
      chainblocks::start(pchain, input);
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
  CBVar activate(CBContext *context, const CBVar &input) {
    auto current = context->chainStack.back();

    auto pchain = [&] {
      if (!chain) {
        if (current->resumer) {
          return current->resumer;
        } else {
          throw ActivationError("Resume, chain not found.");
        }
      } else {
        return chain.get();
      }
    }();

    // assign the new chain as current chain on the flow
    context->flow->chain = pchain;

    // ensure chain is not running, we start from top
    chainblocks::stop(pchain);

    // Prepare
    pchain->node = context->main->node;
    chainblocks::prepare(pchain, context->flow);

    // we should be valid as this block should be dependent on current
    // do this here as stop/prepare might overwrite
    pchain->resumer = current;

    // Start
    chainblocks::start(pchain, input);

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
    for (auto &shared : data.shared) {
      if (shared.scope == data.chain) {
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
      // must release on capacity
      for (uint32_t j = 0; j < _storage[i].cap; j++) {
        destroyVar(_storage[i].elements[j]);
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
    return mode == RunChainMode::Inline ? CBExposedTypesInfo(exposedInfo)
                                        : empty;
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
      // pre-set chain context with our context
      // this is used to copy chainStack over to the new one
      chain->context = context;
      // Notice we don't share our flow!
      // let the chain create one by passing null
      chainblocks::prepare(chain.get(), nullptr);
    }

    // Starting
    if (!chainblocks::isRunning(chain.get())) {
      chainblocks::start(chain.get(), input);
    }

    // Tick the chain on the flow that this Step chain created
    CBDuration now = CBClock::now().time_since_epoch();
    chainblocks::tick(chain->context->flow->chain, now, input);
  }
};

template <bool INPUT_PASSTHROUGH, RunChainMode CHAIN_MODE>
struct RunChain : public BaseRunner {
  void setup() {
    passthrough = INPUT_PASSTHROUGH;
    mode = CHAIN_MODE;
  }

  static CBParametersInfo parameters() { return runChainParamsInfo; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      chainref = value;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return chainref;
    default:
      break;
    }
    return Var::Empty;
  }

  void warmup(CBContext *context) {
    ChainBase::warmup(context);
    doWarmup(context);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(!chain))
      return input;

    if constexpr (CHAIN_MODE == RunChainMode::Detached) {
      activateDetached(context, input);
      return input;
    } else if constexpr (CHAIN_MODE == RunChainMode::Stepped) {
      activateStepMode(context, input);
      if constexpr (INPUT_PASSTHROUGH) {
        return input;
      } else {
        return chain->previousOutput;
      }
    } else {
      // Run within the root flow
      auto runRes = runSubChain(chain.get(), context, input);
      if (unlikely(runRes.state == Failed)) {
        // meaning there was an exception while
        // running the sub chain, stop the parent too
        context->stopFlow(runRes.output);
        return runRes.output;
      } else {
        if constexpr (INPUT_PASSTHROUGH) {
          return input;
        } else {
          return runRes.output;
        }
      }
    }
  }
};

struct ChainNotFound : public ActivationError {
  ChainNotFound() : ActivationError("Could not find a chain to run") {}
};

template <class T> struct BaseLoader : public BaseRunner {
  CBTypeInfo _inputTypeCopy{};
  IterableExposedInfo _sharedCopy;

  CBTypeInfo compose(const CBInstanceData &data) {
    _inputTypeCopy = data.inputType;
    const IterableExposedInfo sharedStb(data.shared);
    // copy shared
    _sharedCopy = sharedStb;

    if (mode == RunChainMode::Inline || mode == RunChainMode::Stepped) {
      // If inline allow chains to receive a result
      return CoreInfo::AnyType;
    } else {
      return data.inputType;
    }
  }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 1:
      mode = RunChainMode(value.payload.enumValue);
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 1:
      return Var::Enum(mode, CoreCC, 'runC');
    default:
      break;
    }
    return Var::Empty;
  }

  void cleanup() { BaseRunner::cleanup(); }

  CBVar activateChain(CBContext *context, const CBVar &input) {
    if (unlikely(!chain))
      throw ChainNotFound();

    if (mode == RunChainMode::Detached) {
      activateDetached(context, input);
    } else if (mode == RunChainMode::Stepped) {
      activateStepMode(context, input);
      return chain->previousOutput;
    } else {
      // Run within the root flow
      const auto runRes = runSubChain(chain.get(), context, input);
      if (likely(runRes.state != Failed)) {
        return runRes.output;
      }
    }

    return input;
  }
};

struct ChainLoader : public BaseLoader<ChainLoader> {
  BlocksVar _onReloadBlocks{};

  static inline Parameters params{
      {"Provider",
       CBCCSTR("The chainblocks chain provider."),
       {ChainProvider::ProviderOrNone}},
      {"Mode",
       CBCCSTR("The way to run the chain. Inline: will run the sub chain "
               "inline within the root chain, a pause in the child chain will "
               "pause the root too; Detached: will run the chain separately in "
               "the same node, a pause in this chain will not pause the root; "
               "Stepped: the chain will run as a child, the root will tick the "
               "chain every activation of this block and so a child pause "
               "won't pause the root."),
       {ModeType}},
      {"OnReload",
       CBCCSTR("Blocks to execute when the chain is reloaded, the input of "
               "this flow will be the reloaded chain."),
       {CoreInfo::BlocksOrNone}}};

  static CBParametersInfo parameters() { return params; }

  CBChainProvider *_provider;

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0: {
      cleanup(); // stop current
      if (value.valueType == Object) {
        _provider = (CBChainProvider *)value.payload.objectValue;
      } else {
        _provider = nullptr;
      }
    } break;
    case 1: {
      BaseLoader<ChainLoader>::setParam(index, value);
    } break;
    case 2: {
      _onReloadBlocks = value;
    } break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      if (_provider) {
        return Var::Object(_provider, CoreCC, 'chnp');
      } else {
        return Var();
      }
    case 1:
      return BaseLoader<ChainLoader>::getParam(index);
    case 2:
      return _onReloadBlocks;
    default: {
      return Var::Empty;
    }
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    _onReloadBlocks.compose(data);
    return BaseLoader<ChainLoader>::compose(data);
  }

  void cleanup() {
    BaseLoader<ChainLoader>::cleanup();
    _onReloadBlocks.cleanup();
    if (_provider)
      _provider->reset(_provider);
  }

  void warmup(CBContext *context) {
    BaseLoader<ChainLoader>::warmup(context);
    _onReloadBlocks.warmup(context);
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
        LOG(INFO) << "Chain " << update.chain->name << " has been reloaded.";
        CBVar output{};
        _onReloadBlocks.activate(context, Var(chain), output);
      }
    }

    try {
      return BaseLoader<ChainLoader>::activateChain(context, input);
    } catch (const ChainNotFound &ex) {
      // let's ignore chain not found in this case
      return input;
    }
  }
};

struct ChainRunner : public BaseLoader<ChainRunner> {
  static inline Parameters params{
      {"Chain",
       CBCCSTR("The chain variable to compose and run."),
       {CoreInfo::ChainVarType}},
      {"Mode",
       CBCCSTR("The way to run the chain. Inline: will run the sub chain "
               "inline within the root chain, a pause in the child chain will "
               "pause the root too; Detached: will run the chain separately in "
               "the same node, a pause in this chain will not pause the root; "
               "Stepped: the chain will run as a child, the root will tick the "
               "chain every activation of this block and so a child pause "
               "won't pause the root."),
       {ModeType}}};

  static CBParametersInfo parameters() { return params; }

  ParamVar _chain{};
  std::size_t _chainHash = 0;
  CBChain *_chainPtr = nullptr;
  CBExposedTypeInfo _requiredChain{};

  void setParam(int index, const CBVar &value) {
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
      _requiredChain =
          CBExposedTypeInfo{_chain.variableName(), CBCCSTR("The chain to run."),
                            CoreInfo::ChainType};
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
      await(context, [this, context, chainVar]() {
        doCompose(context);
        chain->composedHash = chainblocks::hash(chainVar);
      });

      _chainHash = chain->composedHash;
      _chainPtr = chain.get();
      doWarmup(context);
    }

    return BaseLoader<ChainRunner>::activateChain(context, input);
  }
};

enum class WaitUntil {
  FirstSuccess, // will wait until the first success and stop any other
                // pending operation
  AllSuccess,   // will wait untill all complete, will stop and fail on any
                // failure
  SomeSuccess   // will wait until all complete but won't fail if some of the
                // chains failed
};

struct ManyChain : public std::enable_shared_from_this<ManyChain> {
  uint32_t index;
  std::shared_ptr<CBChain> chain;
};

struct ParallelBase : public ChainBase {
  typedef EnumInfo<WaitUntil> WaitUntilInfo;
  static inline WaitUntilInfo waitUntilInfo{"WaitUntil", CoreCC, 'tryM'};
  static inline Type WaitUntilType{
      {CBType::Enum, {.enumeration = {.vendorId = CoreCC, .typeId = 'tryM'}}}};

  static inline Parameters _params{
      {"Chain",
       CBCCSTR("The chain to spawn and try to run many times concurrently."),
       ChainBase::ChainVarTypes},
      {"Policy",
       CBCCSTR("The execution policy in terms of chains success."),
       {WaitUntilType}},
      {"Threads",
       CBCCSTR("The number of cpu threads to use."),
       {CoreInfo::IntType}},
      {"Coroutines",
       CBCCSTR("The number of coroutines to run on each thread."),
       {CoreInfo::IntType}}};

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      chainref = value;
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

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return chainref;
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

  CBTypeInfo compose(const CBInstanceData &data) {
    ChainBase::compose(data); // discard the result, we do our thing here

    _pool.reset(new ChainDoppelgangerPool<ManyChain>(CBChain::weakRef(chain)));

    const IterableExposedInfo shared(data.shared);
    // copy shared
    _sharedCopy = shared;

    return CoreInfo::NoneType; // not complete
  }

  struct Composer {
    ParallelBase &server;
    CBContext *context;

    void compose(CBChain *chain) {
      CBInstanceData data{};
      data.inputType = server._inputType;
      data.shared = server._sharedCopy;
      data.chain = context->chainStack.back();
      chain->node = context->main->node;
      auto res = composeChain(
          chain,
          [](const struct CBlock *errorBlock, const char *errorTxt,
             CBBool nonfatalWarning, void *userData) {
            if (!nonfatalWarning) {
              LOG(ERROR) << errorTxt;
              throw ActivationError("Http.Server handler chain compose failed");
            } else {
              LOG(WARNING) << errorTxt;
            }
          },
          nullptr, data);
      arrayFree(res.exposedInfo);
      arrayFree(res.requiredInfo);
    }
  } _composer{*this};

  void warmup(CBContext *context) {
#if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
    if (_threads > 1) {
      const auto threads =
          std::min(_threads, int64_t(std::thread::hardware_concurrency()));
      if (!_exec || _exec->num_workers() != (size_t(threads))) {
        _exec.reset(new tf::Executor(size_t(threads)));
      }
    }
#endif
    _composer.context = context;
  }

  void cleanup() {
    _outputs.resize(_maxSize);
    for (auto &v : _outputs) {
      destroyVar(v);
    }
  }

protected:
  WaitUntil _policy{WaitUntil::AllSuccess};
  std::unique_ptr<ChainDoppelgangerPool<ManyChain>> _pool;
  IterableExposedInfo _sharedCopy;
  Type _outputSeqType;
  Types _outputTypes;
  CBTypeInfo _inputType{};
  std::vector<CBVar> _outputs;
  std::vector<std::shared_ptr<ManyChain>> _chains;
  size_t _maxSize{0};
  int64_t _threads{1};
  int64_t _coros{1};
  std::unique_ptr<tf::Executor> _exec;
};

struct TryMany : public ParallelBase {
  static CBTypesInfo inputTypes() { return CoreInfo::AnySeqType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnySeqType; }

  CBTypeInfo compose(const CBInstanceData &data) {
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
      return chain->outputType;
    } else {
      // seq result
      _outputTypes = Types({chain->outputType});
      _outputSeqType = Type::SeqOf(_outputTypes);
      return _outputSeqType;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    // schedule many
    auto node = context->main->node.lock();

    _chains.resize(input.payload.seqValue.len);
    // stop chains in any case if we exit this call
    DEFER({
      for (auto &cref : _chains) {
        stop(cref->chain.get());
        _pool->release(cref);
      }
    });

    for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {
      _chains[i] = _pool->acquire(_composer);
      _chains[i]->index = i;
    }

    auto nchains = _chains.size();

    // cleanup outputs, store max size tho for cleanup
    _maxSize = std::max(_maxSize, _outputs.size());
    _outputs.clear();

    // wait according to policy
    while (true) {
      const auto _suspend_state = chainblocks::suspend(context, 0);
      if (unlikely(_suspend_state != CBChainState::Continue)) {
        return Var::Empty;
      } else {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
        {
#else
        if (_threads == 1) {
#endif
          // advance our chains and check
          for (auto it = _chains.begin(); it != _chains.end();) {
            auto &cref = *it;

            // Prepare and start if no callc was called
            if (!cref->chain->coro) {
              cref->chain->node = context->main->node;
              // pre-set chain context with our context
              // this is used to copy chainStack over to the new one
              cref->chain->context = context;
              // Notice we don't share our flow!
              // let the chain create one by passing null
              chainblocks::prepare(cref->chain.get(), nullptr);
              chainblocks::start(cref->chain.get(),
                                 input.payload.seqValue.elements[cref->index]);
            }

            // Tick the chain on the flow that this chain created
            CBDuration now = CBClock::now().time_since_epoch();
            chainblocks::tick(cref->chain->context->flow->chain, now,
                              input.payload.seqValue.elements[cref->index]);

            if (!isRunning(cref->chain.get())) {
              if (cref->chain->state == CBChain::State::Ended) {
                if (_policy == WaitUntil::FirstSuccess) {
                  // success, next call clones, make sure to destroy
                  _outputs.resize(1);
                  stop(cref->chain.get(), &_outputs[0]);
                  // short-circuit, defer will take care of cleaning up
                  return _outputs[0];
                } else {
                  CBVar output{};
                  stop(cref->chain.get(), &output);
                  _outputs.emplace_back(output);
                  it = _chains.erase(it);
                }
              } else {
                // remove failed chains
                it = _chains.erase(it);
              }
            } else {
              ++it;
            }
          }
        }
#if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
        else {
          // multithreaded
          tf::Taskflow flow;

          flow.for_each_dynamic(
              _chains.begin(), _chains.end(),
              [context, input](auto &cref) {
                // skip if failed or ended
                if (cref->chain->state >= CBChain::State::Failed)
                  return;
                // Prepare and start if no callc was called
                if (!cref->chain->coro) {
                  cref->chain->node = context->main->node;
                  // pre-set chain context with our context
                  // this is used to copy chainStack over to the new one
                  cref->chain->context = context;
                  // Notice we don't share our flow!
                  // let the chain create one by passing null
                  chainblocks::prepare(cref->chain.get(), nullptr);
                  chainblocks::start(
                      cref->chain.get(),
                      input.payload.seqValue.elements[cref->index]);
                }

                // Tick the chain on the flow that this chain created
                CBDuration now = CBClock::now().time_since_epoch();
                chainblocks::tick(cref->chain->context->flow->chain, now,
                                  input.payload.seqValue.elements[cref->index]);
              },
              _coros);

          _exec->run(flow).get();

          for (auto it = _chains.begin(); it != _chains.end();) {
            auto &cref = *it;
            if (cref->chain->state == CBChain::State::Ended) {
              if (_policy == WaitUntil::FirstSuccess) {
                // success, next call clones, make sure to destroy
                _outputs.resize(1);
                stop(cref->chain.get(), &_outputs[0]);
                // short-circuit, defer will take care of cleaning up
                return _outputs[0];
              } else {
                CBVar output{};
                stop(cref->chain.get(), &output);
                _outputs.emplace_back(output);
                it = _chains.erase(it);
              }
            } else {
              // remove failed chains
              it = _chains.erase(it);
            }
          }
        }
#endif

        if (_chains.size() == 0) {
          auto osize = _outputs.size();
          if (unlikely(osize == 0)) {
            throw ActivationError("TryMany, failed all chains!");
          } else {
            // all ended let's apply policy here
            if (_policy == WaitUntil::SomeSuccess) {
              return Var(_outputs);
            } else {
              assert(_policy == WaitUntil::AllSuccess);
              if (nchains == osize) {
                return Var(_outputs);
              } else {
                throw ActivationError("TryMany, failed some chains!");
              }
            }
          }
        }
      }
    }
  }
};

struct DoWide : public ParallelBase {
  int64_t _width{10};

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnySeqType; }

  static inline Parameters _params{
      {{"Width",
        CBCCSTR("How wide the execution mesh will be."),
        {CoreInfo::IntType}}},
      ParallelBase::_params};

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
    if (index == 0) {
      _width = std::max(int64_t(1), value.payload.intValue);
    } else {
      ParallelBase::setParam(index - 1, value);
    }
  }

  CBVar getParam(int index) {
    if (index == 0) {
      return Var(_width);
    } else {
      return ParallelBase::getParam(index - 1);
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    ParallelBase::compose(data);

    // input
    _inputType = data.inputType;

    // output
    _outputTypes = Types({chain->outputType});
    _outputSeqType = Type::SeqOf(_outputTypes);
    return _outputSeqType;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    // schedule many
    auto node = context->main->node.lock();

    _chains.resize(_width);
    // stop chains in any case if we exit this call
    DEFER({
      for (auto &cref : _chains) {
        stop(cref->chain.get());
        _pool->release(cref);
      }
    });

    for (int64_t i = 0; i < _width; i++) {
      _chains[i] = _pool->acquire(_composer);
      _chains[i]->index = uint32_t(i);
    }

    auto nchains = _chains.size();

    // cleanup outputs, store max size tho for cleanup
    _maxSize = std::max(_maxSize, _outputs.size());
    _outputs.clear();

    // wait according to policy
    while (true) {
      const auto _suspend_state = chainblocks::suspend(context, 0);
      if (unlikely(_suspend_state != CBChainState::Continue)) {
        return Var::Empty;
      } else {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
        {
#else
        if (_threads == 1) {
#endif
          // advance our chains and check
          for (auto it = _chains.begin(); it != _chains.end();) {
            auto &cref = *it;

            // Prepare and start if no callc was called
            if (!cref->chain->coro) {
              cref->chain->node = context->main->node;
              // pre-set chain context with our context
              // this is used to copy chainStack over to the new one
              cref->chain->context = context;
              // Notice we don't share our flow!
              // let the chain create one by passing null
              chainblocks::prepare(cref->chain.get(), nullptr);
              chainblocks::start(cref->chain.get(), input);
            }

            // Tick the chain on the flow that this chain created
            CBDuration now = CBClock::now().time_since_epoch();
            chainblocks::tick(cref->chain->context->flow->chain, now, input);

            if (!isRunning(cref->chain.get())) {
              if (cref->chain->state == CBChain::State::Ended) {
                if (_policy == WaitUntil::FirstSuccess) {
                  // success, next call clones, make sure to destroy
                  _outputs.resize(1);
                  stop(cref->chain.get(), &_outputs[0]);
                  // short-circuit, defer will take care of cleaning up
                  return _outputs[0];
                } else {
                  CBVar output{};
                  stop(cref->chain.get(), &output);
                  _outputs.emplace_back(output);
                  it = _chains.erase(it);
                }
              } else {
                // remove failed chains
                it = _chains.erase(it);
              }
            } else {
              ++it;
            }
          }
        }
#if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
        else {
          // multithreaded
          tf::Taskflow flow;

          flow.for_each_dynamic(
              _chains.begin(), _chains.end(),
              [context, input](auto &cref) {
                // skip if failed or ended
                if (cref->chain->state >= CBChain::State::Failed)
                  return;
                // Prepare and start if no callc was called
                if (!cref->chain->coro) {
                  cref->chain->node = context->main->node;
                  // pre-set chain context with our context
                  // this is used to copy chainStack over to the new one
                  cref->chain->context = context;
                  // Notice we don't share our flow!
                  // let the chain create one by passing null
                  chainblocks::prepare(cref->chain.get(), nullptr);
                  chainblocks::start(cref->chain.get(), input);
                }

                // Tick the chain on the flow that this chain created
                CBDuration now = CBClock::now().time_since_epoch();
                chainblocks::tick(cref->chain->context->flow->chain, now,
                                  input);
              },
              _coros);

          _exec->run(flow).get();

          for (auto it = _chains.begin(); it != _chains.end();) {
            auto &cref = *it;
            if (cref->chain->state == CBChain::State::Ended) {
              if (_policy == WaitUntil::FirstSuccess) {
                // success, next call clones, make sure to destroy
                _outputs.resize(1);
                stop(cref->chain.get(), &_outputs[0]);
                // short-circuit, defer will take care of cleaning up
                return _outputs[0];
              } else {
                CBVar output{};
                stop(cref->chain.get(), &output);
                _outputs.emplace_back(output);
                it = _chains.erase(it);
              }
            } else {
              // remove failed chains
              it = _chains.erase(it);
            }
          }
        }
#endif

        if (_chains.size() == 0) {
          auto osize = _outputs.size();
          if (unlikely(osize == 0)) {
            throw ActivationError("TryMany, failed all chains!");
          } else {
            // all ended let's apply policy here
            if (_policy == WaitUntil::SomeSuccess) {
              return Var(_outputs);
            } else {
              assert(_policy == WaitUntil::AllSuccess);
              if (nchains == osize) {
                return Var(_outputs);
              } else {
                throw ActivationError("TryMany, failed some chains!");
              }
            }
          }
        }
      }
    }
  }
};

struct Spawn : public ChainBase {
  Spawn() { mode = RunChainMode::Detached; }

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::ChainType; }

  static inline Parameters _params{
      {"Chain",
       CBCCSTR("The chain to spawn and try to run many times concurrently."),
       ChainBase::ChainVarTypes}};

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      chainref = value;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return chainref;
    default:
      return Var::Empty;
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    ChainBase::compose(data); // discard the result, we do our thing here
    // chain should be populated now and such
    _pool.reset(new ChainDoppelgangerPool<ManyChain>(CBChain::weakRef(chain)));

    const IterableExposedInfo shared(data.shared);
    // copy shared
    _sharedCopy = shared;

    _inputType = data.inputType;
    return CoreInfo::ChainType;
  }

  struct Composer {
    Spawn &server;
    CBContext *context;

    void compose(CBChain *chain) {
      CBInstanceData data{};
      data.inputType = server._inputType;
      data.shared = server._sharedCopy;
      data.chain = context->chainStack.back();
      chain->node = context->main->node;
      auto res = composeChain(
          chain,
          [](const struct CBlock *errorBlock, const char *errorTxt,
             CBBool nonfatalWarning, void *userData) {
            if (!nonfatalWarning) {
              LOG(ERROR) << errorTxt;
              throw ActivationError("Http.Server handler chain compose failed");
            } else {
              LOG(WARNING) << errorTxt;
            }
          },
          nullptr, data);
      arrayFree(res.exposedInfo);
      arrayFree(res.requiredInfo);
    }
  } _composer{*this};

  void warmup(CBContext *context) { _composer.context = context; }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto node = context->main->node.lock();
    auto c = _pool->acquire(_composer);
    c->chain->onStop.clear(); // we have a fresh recycled chain here
    std::weak_ptr<ManyChain> wc(c);
    c->chain->onStop.emplace_back([this, wc]() {
      if (auto c = wc.lock())
        _pool->release(c);
    });
    node->schedule(c->chain, input, false);
    return Var(c->chain); // notice this is "weak"
  }

  std::unique_ptr<ChainDoppelgangerPool<ManyChain>> _pool;
  IterableExposedInfo _sharedCopy;
  CBTypeInfo _inputType{};
};

void registerChainsBlocks() {
  using RunChainDo = RunChain<false, RunChainMode::Inline>;
  using RunChainDispatch = RunChain<true, RunChainMode::Inline>;
  using RunChainDetach = RunChain<true, RunChainMode::Detached>;
  using RunChainStep = RunChain<false, RunChainMode::Stepped>;
  REGISTER_CBLOCK("Resume", Resume);
  REGISTER_CBLOCK("Start", Start);
  REGISTER_CBLOCK("Wait", Wait);
  REGISTER_CBLOCK("Stop", StopChain);
  REGISTER_CBLOCK("Do", RunChainDo);
  REGISTER_CBLOCK("Dispatch", RunChainDispatch);
  REGISTER_CBLOCK("Detach", RunChainDetach);
  REGISTER_CBLOCK("Step", RunChainStep);
  REGISTER_CBLOCK("ChainLoader", ChainLoader);
  REGISTER_CBLOCK("ChainRunner", ChainRunner);
  REGISTER_CBLOCK("Recur", Recur);
  REGISTER_CBLOCK("TryMany", TryMany);
  REGISTER_CBLOCK("Spawn", Spawn);
  REGISTER_CBLOCK("DoWide", DoWide);
}
}; // namespace chainblocks

#ifndef __EMSCRIPTEN__
// this is a hack to fix a linker issue with taskflow...
/*
duplicate symbol 'thread-local initialization routine for
tf::Executor::_per_thread' in: libcb_static.a(chains.cpp.o)
    libcb_static.a(genetic.cpp.o)
*/
#include "genetic.hpp"

#endif