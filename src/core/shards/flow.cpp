/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "shardwrapper.hpp"
#include "shards.h"
#include "foundation.hpp"
#include "shared.hpp"

namespace shards {
static Type condShardSeqs = Type::SeqOf(CoreInfo::ShardsOrNone);
static ParamsInfo condParamsInfo =
    ParamsInfo(ParamsInfo::Param("Wires",
                                 SHCCSTR("A sequence of shards, interleaving condition test predicate "
                                         "and action to execute if the condition matches."),
                                 condShardSeqs),
               ParamsInfo::Param("Passthrough", SHCCSTR("The output of this shard will be its input."), CoreInfo::BoolType),
               ParamsInfo::Param("Threading",
                                 SHCCSTR("Will not short circuit after the first true test expression. "
                                         "The threaded value gets used in only the action and not the "
                                         "test part of the clause."),
                                 CoreInfo::BoolType));

struct Cond {
  // WORKS but TODO refactor using newer abstracted types
  SHVar _wires{};
  std::vector<std::vector<Shard *>> _conditions;
  std::vector<std::vector<Shard *>> _actions;
  bool _passthrough = true;
  bool _threading = false;
  SHComposeResult _wireValidation{};

  static SHOptionalString help() {
    return SHCCSTR("Takes a sequence of conditions and predicates. "
                   "Evaluates each condition one by one and if one matches, "
                   "executes the associated action.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("The value that will be passed to each predicate and action "
                   "to execute.");
  }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("The input of the shard if `Passthrough` is `true`; otherwise, the "
                   "output of the action of the first matching condition.");
  }

  static SHParametersInfo parameters() { return SHParametersInfo(condParamsInfo); }

  void warmup(SHContext *ctx) {
    for (auto &blks : _conditions) {
      for (auto &blk : blks) {
        if (blk->warmup)
          blk->warmup(blk, ctx);
      }
    }
    for (auto &blks : _actions) {
      for (auto &blk : blks) {
        if (blk->warmup)
          blk->warmup(blk, ctx);
      }
    }
  }

  void cleanup() {
    for (auto it = _conditions.rbegin(); it != _conditions.rend(); ++it) {
      auto shards = *it;
      for (auto jt = shards.rbegin(); jt != shards.rend(); ++jt) {
        auto shard = *jt;
        shard->cleanup(shard);
      }
    }
    for (auto it = _actions.rbegin(); it != _actions.rend(); ++it) {
      auto shards = *it;
      for (auto jt = shards.rbegin(); jt != shards.rend(); ++jt) {
        auto shard = *jt;
        shard->cleanup(shard);
      }
    }
  }

  void destroy() {
    for (auto it = _conditions.rbegin(); it != _conditions.rend(); ++it) {
      auto shards = *it;
      for (auto jt = shards.rbegin(); jt != shards.rend(); ++jt) {
        auto shard = *jt;
        shard->cleanup(shard);
        shard->destroy(shard);
      }
    }
    for (auto it = _actions.rbegin(); it != _actions.rend(); ++it) {
      auto shards = *it;
      for (auto jt = shards.rbegin(); jt != shards.rend(); ++jt) {
        auto shard = *jt;
        shard->cleanup(shard);
        shard->destroy(shard);
      }
    }
    destroyVar(_wires);
    shards::arrayFree(_wireValidation.exposedInfo);
    shards::arrayFree(_wireValidation.requiredInfo);
  }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0: {
      destroy();
      _conditions.clear();
      _actions.clear();
      cloneVar(_wires, value);
      if (value.valueType == Seq) {
        auto counter = value.payload.seqValue.len;
        if (counter % 2)
          throw SHException("Cond: first parameter must contain a sequence of "
                            "pairs [condition to check & action to perform if "
                            "check passed (true)].");
        _conditions.resize(counter / 2);
        _actions.resize(counter / 2);
        auto idx = 0;
        for (uint32_t i = 0; i < counter; i++) {
          auto val = value.payload.seqValue.elements[i];
          if (i % 2) { // action
            if (val.valueType == ShardRef) {
              assert(!val.payload.shardValue->owned);
              val.payload.shardValue->owned = true;
              _actions[idx].push_back(val.payload.shardValue);
            } else { // seq
              for (uint32_t y = 0; y < val.payload.seqValue.len; y++) {
                assert(val.payload.seqValue.elements[y].valueType == ShardRef);
                auto blk = val.payload.seqValue.elements[y].payload.shardValue;
                assert(!blk->owned);
                blk->owned = true;
                _actions[idx].push_back(blk);
              }
            }

            idx++;
          } else { // condition
            if (val.valueType == ShardRef) {
              assert(!val.payload.shardValue->owned);
              val.payload.shardValue->owned = true;
              _conditions[idx].push_back(val.payload.shardValue);
            } else { // seq
              for (uint32_t y = 0; y < val.payload.seqValue.len; y++) {
                assert(val.payload.seqValue.elements[y].valueType == ShardRef);
                auto blk = val.payload.seqValue.elements[y].payload.shardValue;
                assert(!blk->owned);
                blk->owned = true;
                _conditions[idx].push_back(blk);
              }
            }
          }
        }
      }
      break;
    }
    case 1:
      _passthrough = value.payload.boolValue;
      break;
    case 2:
      _threading = value.payload.boolValue;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _wires;
    case 1:
      return Var(_passthrough);
    case 2:
      return Var(_threading);
    default:
      break;
    }
    return Var();
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    // Free any previous result!
    shards::arrayFree(_wireValidation.exposedInfo);
    shards::arrayFree(_wireValidation.requiredInfo);

    // Validate condition wires, altho they do not influence anything we need
    // to report errors
    for (const auto &action : _conditions) {
      auto validation = composeWire(
          action,
          [](const Shard *errorShard, const char *errorTxt, bool nonfatalWarning, void *userData) {
            if (!nonfatalWarning) {
              SHLOG_ERROR("Cond: failed inner wire validation, error: {}", errorTxt);
              throw SHException("Cond: failed inner wire validation.");
            } else {
              SHLOG_INFO("Cond: warning during inner wire validation: {}", errorTxt);
            }
          },
          this, data);
      if (validation.outputType.basicType != SHType::Bool) {
        throw ComposeError("Cond - expected Bool output from predicate shards");
      }
      shards::arrayFree(validation.exposedInfo);
      shards::arrayFree(validation.requiredInfo);
    }

    // Evaluate all actions, all must return the same type in order to be safe
    SHTypeInfo previousType{};
    auto idx = 0;
    auto first = true;
    auto exposing = true;
    for (const auto &action : _actions) {
      auto validation = composeWire(
          action,
          [](const Shard *errorShard, const char *errorTxt, bool nonfatalWarning, void *userData) {
            if (!nonfatalWarning) {
              SHLOG_ERROR("Cond: failed inner wire validation, error: {}", errorTxt);
              throw SHException("Cond: failed inner wire validation.");
            } else {
              SHLOG_INFO("Cond: warning during inner wire validation: {}", errorTxt);
            }
          },
          this, data);

      if (first) {
        // A first valid exposedInfo array is our gold
        _wireValidation = validation;
        first = false;
      } else {
        if (exposing) {
          auto curlen = _wireValidation.exposedInfo.len;
          auto newlen = validation.exposedInfo.len;
          if (curlen != newlen) {
            SHLOG_INFO("Cond: number of exposed variables between actions mismatch, "
                       "variables won't be exposed, flow is unpredictable!");
            exposing = false;
          }

          if (exposing) {
            for (uint32_t i = 0; i < curlen; i++) {
              if (_wireValidation.exposedInfo.elements[i] != validation.exposedInfo.elements[i]) {
                SHLOG_INFO("Cond: types of exposed variables between actions "
                           "mismatch, variables won't be exposed, flow is "
                           "unpredictable!");
                exposing = false;
                break;
              }
            }
          }
        }

        // free the exposed info part
        shards::arrayFree(validation.exposedInfo);
        shards::arrayFree(validation.requiredInfo);

        if (!exposing) {
          // make sure we expose nothing in this case!
          shards::arrayFree(_wireValidation.exposedInfo);
          shards::arrayFree(_wireValidation.requiredInfo);
        }
      }

      if (idx > 0 && !_passthrough && !validation.flowStopper && validation.outputType != previousType)
        throw SHException("Cond: output types between actions mismatch.");

      idx++;
      previousType = validation.outputType;
    }

    return _passthrough ? data.inputType : previousType;
  }

  SHExposedTypesInfo exposedVariables() { return _wireValidation.exposedInfo; }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (unlikely(_actions.size() == 0)) {
      if (_passthrough)
        return input;
      else
        return {}; // None
    }

    auto idx = 0;
    SHVar actionInput = input;
    SHVar finalOutput{};
    for (auto &cond : _conditions) {
      SHVar output{};
      Shards shards{&cond[0], (uint32_t)cond.size(), 0};
      auto state = activateShards2(shards, context, input, output);
      // conditional flow so we might have "returns" form (And) (Or)
      if (unlikely(state > SHWireState::Return))
        return output;

      if (output.payload.boolValue) {
        // Do the action if true!
        // And stop here
        output = {};
        Shards action{&_actions[idx][0], (uint32_t)_actions[idx].size(), 0};
        state = activateShards(action, context, actionInput, output);
        if (state != SHWireState::Continue)
          return output;
        if (_threading) {
          // set the output as the next action input (not cond tho!)
          finalOutput = output;
          actionInput = finalOutput;
        } else {
          // not _threading so short circuit here
          return _passthrough ? input : output;
        }
      }
      idx++;
    }

    return _passthrough ? input : finalOutput;
  }
};

struct BaseSubFlow {
  SHTypesInfo inputTypes() {
    if (_shards) {
      auto blks = _shards.shards();
      return blks.elements[0]->inputTypes(blks.elements[0]);
    } else {
      return CoreInfo::AnyType;
    }
  }
  static SHOptionalString inputHelp() { return SHCCSTR("Must match the input types of the first shard in the sequence."); }

  SHTypesInfo outputTypes() {
    if (_shards) {
      auto blks = _shards.shards();
      return blks.elements[0]->outputTypes(blks.elements[0]);
    } else {
      return CoreInfo::AnyType;
    }
  }
  static SHOptionalString outputHelp() { return SHCCSTR("Will match the output types of the first shard of the sequence."); }

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) { _shards = value; }

  SHVar getParam(int index) { return _shards; }

  void cleanup() {
    if (_shards)
      _shards.cleanup();
  }

  void warmup(SHContext *ctx) {
    if (_shards)
      _shards.warmup(ctx);
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    if (_shards)
      _composition = _shards.compose(data);
    else
      _composition = {};

    return _composition.outputType;
  }

  SHExposedTypesInfo exposedVariables() { return _composition.exposedInfo; }

protected:
  ShardsVar _shards{};
  SHComposeResult _composition{};
  static inline Parameters _params{{"Shards", SHCCSTR("The shards to activate."), {CoreInfo::ShardsOrNone}}};
};

struct Maybe : public BaseSubFlow {
  static SHOptionalString help() {
    return SHCCSTR("Attempts to activate a shard or a sequence of shards. Upon "
                   "failure, activate another shard or sequence of shards.");
  }

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _shards = value;
      break;
    case 1:
      _elseBlks = value;
      break;
    case 2:
      _silent = value.payload.boolValue;
      break;
    default:
      throw InvalidParameterIndex();
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _shards;
    case 1:
      return _elseBlks;
    case 2:
      return Var(_silent);
    default:
      throw InvalidParameterIndex();
    }
  }

  SHExposedTypesInfo _mergedReqs{};
  SHExposedTypesInfo requiredVariables() { return _mergedReqs; }
  void destroyReqs() { arrayFree(_mergedReqs); }

  SHTypeInfo compose(const SHInstanceData &data) {
    destroyReqs();

    // exposed stuff should be balanced
    // and output should the same
    SHComposeResult elseComp{};
    if (_elseBlks) {
      elseComp = _elseBlks.compose(data);
    }

    // add to merged requirements
    for (auto &req : elseComp.requiredInfo) {
      arrayPush(_mergedReqs, req);
    }

    if (_shards)
      _composition = _shards.compose(data);
    else
      _composition = {};

    // add to merged requirements
    for (auto &req : _composition.requiredInfo) {
      arrayPush(_mergedReqs, req);
    }

    const auto nextIsNone =
        data.outputTypes.len == 0 || (data.outputTypes.len == 1 && data.outputTypes.elements[0].basicType == None);

    if (!nextIsNone && !elseComp.flowStopper && _composition.outputType != elseComp.outputType) {
      SHLOG_ERROR("{} != {}", _composition.outputType, elseComp.outputType);
      throw ComposeError("Maybe: output types mismatch between the two possible flows!");
    }

    // Maybe won't expose
    _composition.exposedInfo = {};

    return _composition.outputType;
  }

  void cleanup() {
    if (_elseBlks)
      _elseBlks.cleanup();

    BaseSubFlow::cleanup();
  }

  void warmup(SHContext *ctx) {
    if (_elseBlks)
      _elseBlks.warmup(ctx);

    BaseSubFlow::warmup(ctx);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    SHVar output = input;
    if (likely(_shards)) {
      if (_silent) {
        spdlog::set_level(spdlog::level::off);
      }
      DEFER({
        if (_silent) {
#ifdef NDEBUG
#if (SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_DEBUG)
          spdlog::set_level(spdlog::level::debug);
#else
          spdlog::set_level(spdlog::level::info);
#endif
#else
          spdlog::set_level(spdlog::level::trace);
#endif
        }
      });
      auto state = _shards.activate(context, input, output);
      if (state == SHWireState::Error) {
        if (likely(!context->onLastResume)) {
          if (!_silent) {
            SHLOG_WARNING("Maybe shard Ignored an error: {}", context->getErrorMessage());
          }
          context->continueFlow();
          if (_elseBlks)
            _elseBlks.activate(context, input, output);
        } else {
          // Just continue as the wire is done
          return Var::Empty;
        }
      }
    }
    return output;
  }

private:
  ShardsVar _elseBlks{};
  bool _silent{false};
  static inline Parameters _params{BaseSubFlow::_params,
                                   {{"Else", SHCCSTR("The shards to activate on failure."), {CoreInfo::ShardsOrNone}},
                                    {"Silent",
                                     SHCCSTR("If logging should be disabled while running the shards (this "
                                             "will also disable (Log) and (Msg) shards) and no warning "
                                             "message should be printed on failure."),
                                     {CoreInfo::BoolType}}}};
};

struct Await : public BaseSubFlow {
  static SHOptionalString help() {
    return SHCCSTR("Executes a shard or a sequence of shards asynchronously "
                   "and awaits their completion.");
  }

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) { _shards = value; }

  SHVar getParam(int index) { return _shards; }

  SHTypeInfo compose(const SHInstanceData &data) {
    auto dataCopy = data;
    // flag that we might use a worker
    dataCopy.onWorkerThread = true;
    return BaseSubFlow::compose(dataCopy);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    return awaitne(
        context,
        [&] {
          SHVar output{};
          _shards.activate(context, input, output);
          return output;
        },
        [] {});
  }
};

// Not used actually
// I decided to not expose from When and If
// Exposing from flow stuff is dangerous
struct ExposerFlow {
  template <typename... Validations> void merge(Validations... vals) {
    constexpr int size = sizeof...(vals);
    std::array<SHComposeResult, size> v{vals...};
    IterableExposedInfo master(v[0].exposedInfo);
    for (size_t i = 1; i < v.size(); i++) {
      IterableExposedInfo sub(v[i].exposedInfo);
      if (master.size() != sub.size()) {
        throw ComposeError("Flow is unbalanced, exposing a different amount of variables.");
        _composition = {};
        return;
      }
      if (!std::equal(master.begin(), master.end(), sub.begin())) {
        throw ComposeError("Flow is unbalanced, type mismatch.");
        _composition = {};
        return;
      }
    }
    _composition = v[0];
  }

  SHExposedTypesInfo exposedVariables() { return _composition.exposedInfo; }
  SHComposeResult _composition{};
};

template <bool COND> struct When {
  static SHOptionalString help() {
    if constexpr (COND == true) {
      return SHCCSTR("Conditonal shard that only executes the action if the "
                     "predicate is true.");
    } else {
      return SHCCSTR("Conditonal shard that only executes the action if the "
                     "predicate is false.");
    }
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The value that will be passed to the predicate."); }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString outputHelp() {
    if constexpr (COND == true) {
      return SHCCSTR("The input of the shard if `Passthrough` is `true`, or the "
                     "`Predicate` is `false`; otherwise, the output of the `Action`.");
    } else {
      return SHCCSTR("The input of the shard if `Passthrough` is `true`, or the "
                     "`Predicate` is `true`; otherwise, the output of the `Action`.");
    }
  }

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    if (index == 0)
      _cond = value;
    else if (index == 1)
      _action = value;
    else
      _passth = value.payload.boolValue;
  }

  SHVar getParam(int index) {
    if (index == 0)
      return _cond;
    else if (index == 1)
      return _action;
    else
      return Var(_passth);
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    // both not exposing!
    const auto cres = _cond.compose(data);
    if (cres.outputType.basicType != SHType::Bool) {
      throw ComposeError("When predicate should output a boolean value!");
    }

    auto ares = _action.compose(data);

    const auto nextIsNone =
        data.outputTypes.len == 0 || (data.outputTypes.len == 1 && data.outputTypes.elements[0].basicType == None);

    if (!nextIsNone && !ares.flowStopper && !_passth) {
      if (cres.outputType != data.inputType) {
        throw ComposeError("When Passthrough is false but action output type "
                           "does not match input type.");
      }
    }
    return data.inputType;
  }

  void cleanup() {
    _cond.cleanup();
    _action.cleanup();
  }

  void warmup(SHContext *ctx) {
    _cond.warmup(ctx);
    _action.warmup(ctx);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    SHVar output{};
    auto state = _cond.activate<true>(context, input, output);
    if (unlikely(state > SHWireState::Return))
      return input;

    // type check in compose!
    if (output.payload.boolValue == COND) {
      _action.activate(context, input, output);
      if (!_passth)
        return output;
    }
    return input;
  }

private:
  static inline Parameters _params{
      {"Predicate", SHCCSTR("The predicate to evaluate in order to trigger Action."), {CoreInfo::ShardsOrNone}},
      {"Action",
       SHCCSTR("The shards to activate on when Predicate is true for When and "
               "false for WhenNot."),
       {CoreInfo::ShardsOrNone}},
      {"Passthrough", SHCCSTR("The output of this shard will be its input."), {CoreInfo::BoolType}}};
  ShardsVar _cond{};
  ShardsVar _action{};
  bool _passth = true;
};

struct IfBlock {
  static SHOptionalString help() { return SHCCSTR("Evaluates a predicate and executes an action."); }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The value that will be passed to the predicate."); }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("The input of the shard if `Passthrough` is `true`; otherwise, the "
                   "output of the action that was performed (i.e. `Then` or `Else`).");
  }

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    if (index == 0)
      _cond = value;
    else if (index == 1)
      _then = value;
    else if (index == 2)
      _else = value;
    else
      _passth = value.payload.boolValue;
  }

  SHVar getParam(int index) {
    if (index == 0)
      return _cond;
    else if (index == 1)
      return _then;
    else if (index == 2)
      return _else;
    else
      return Var(_passth);
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    // both not exposing!
    const auto cres = _cond.compose(data);
    if (cres.outputType.basicType != SHType::Bool) {
      throw ComposeError("If - predicate should output a boolean value!");
    }

    const auto tres = _then.compose(data);
    const auto eres = _else.compose(data);

    const auto nextIsNone =
        data.outputTypes.len == 0 || (data.outputTypes.len == 1 && data.outputTypes.elements[0].basicType == None);

    if (!nextIsNone && !tres.flowStopper && !eres.flowStopper && !_passth) {
      if (tres.outputType != eres.outputType) {
        throw ComposeError("If - Passthrough is false but action output types "
                           "do not match.");
      }
    }
    return _passth ? data.inputType : tres.outputType;
  }

  void cleanup() {
    _cond.cleanup();
    _then.cleanup();
    _else.cleanup();
  }

  void warmup(SHContext *ctx) {
    _cond.warmup(ctx);
    _then.warmup(ctx);
    _else.warmup(ctx);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    SHVar output{};
    auto state = _cond.activate<true>(context, input, output);
    if (unlikely(state > SHWireState::Return))
      return input;

    // type check in compose!
    if (output.payload.boolValue) {
      _then.activate(context, input, output);
    } else {
      _else.activate(context, input, output);
    }

    if (!_passth)
      return output;
    else
      return input;
  }

private:
  static inline Parameters _params{
      {"Predicate",
       SHCCSTR("The predicate to evaluate in order to trigger `Then` (when "
               "`true`) or `Else` (when `false`)."),
       {CoreInfo::ShardsOrNone}},
      {"Then", SHCCSTR("The shards to activate when `Predicate` is `true`."), CoreInfo::ShardsOrNone},
      {"Else", SHCCSTR("The shards to activate when `Predicate` is `false`."), CoreInfo::ShardsOrNone},
      {"Passthrough", SHCCSTR("The output of this shard will be its input."), {CoreInfo::BoolType}}};
  ShardsVar _cond{};
  ShardsVar _then{};
  ShardsVar _else{};
  bool _passth = false;
};

struct Match {
  static SHOptionalString help() {
    return SHCCSTR("Compares the input with the declared cases (in order of the declaration) and activates the shard of the "
                   "first matched case.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The value that's compared with the declared cases."); }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("Same value as input if `:Passthrough` is `true` else the output of the matched case's shard if "
                   "`:Passthrough` is `false`.");
  }

  static inline Parameters params{
      {"Cases", SHCCSTR("Values to match against the input. A `nil` case will match anything."), {CoreInfo::AnySeqType}},
      {"Passthrough",
       SHCCSTR("Parameter to control the shard's output. `true` allows the `Match` shard's input itself to appear as its output; "
               "`false` allows the matched shard's output to appear as `Match` shard's output."),
       {CoreInfo::BoolType}}};
  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0: {
      auto counter = value.payload.seqValue.len;
      if (counter % 2)
        throw SHException("Match: first parameter must contain a sequence of pairs [variable "
                          "to compare & action to perform if matches].");
      _ncases = int(counter / 2);
      _pcases.resize(_ncases);
      _cases.resize(_ncases);
      _actions.resize(_ncases);
      _full.resize(counter);
      auto idx = 0;
      for (uint32_t i = 0; i < counter; i += 2) {
        _cases[idx] = value.payload.seqValue.elements[i];
        _pcases[idx] = value.payload.seqValue.elements[i];
        _actions[idx] = value.payload.seqValue.elements[i + 1];
        _full[i] = _pcases[idx];
        _full[i + 1] = _actions[idx];
        idx++;
      }
    } break;
    case 1:
      _pass = value.payload.boolValue;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_full);
    case 1:
      return Var(_pass);
    default:
      return Var::Empty;
    }
  }

  SHExposedTypesInfo _mergedReqs{};
  SHExposedTypesInfo requiredVariables() { return _mergedReqs; }
  void destroyReqs() { arrayFree(_mergedReqs); }

  SHTypeInfo compose(const SHInstanceData &data) {
    destroyReqs();

    for (auto &case_ : _pcases) {
      if (case_.valueType != None) {
        // must compare deeply
        // also pass vector to ensure we get context var info (even if we don't use it)
        std::vector<SHExposedTypeInfo> types;
        TypeInfo cinfo(case_, data, &types);
        if (cinfo != data.inputType) {
          SHLOG_ERROR("Match - case type does not match input type: {} != {}", SHTypeInfo(cinfo), data.inputType);
          throw ComposeError("Match: each case must match the input type!, found a mismatch.");
        }
      }
    }

    SHTypeInfo firstOutput{};
    bool first = true;
    for (auto &action : _actions) {
      const auto cres = action.compose(data);
      if (!_pass) {
        // must evaluate output types and enforce they match between each case,
        // unless flow stopper
        if (first) {
          firstOutput = cres.outputType;
          first = false;
        } else {
          if (!cres.flowStopper) {
            if (cres.outputType != firstOutput) {
              SHLOG_ERROR("Match - case output types do not match: {} != {}", cres.outputType, firstOutput);
              throw ComposeError("Match: when not Passthrough output types "
                                 "must match between cases.");
            }
          }
        }
      }

      // add to merged requirements
      for (auto &req : cres.requiredInfo) {
        arrayPush(_mergedReqs, req);
      }
    }

    return _pass ? data.inputType : firstOutput;
  }

  void warmup(SHContext *context) {
    const auto ncases = _cases.size();
    for (size_t i = 0; i < ncases; ++i) {
      resolver.warmup(_pcases[i], _cases[i], context);
    }

    for (auto &action : _actions) {
      action.warmup(context);
    }
  }

  void cleanup() {
    for (auto &action : _actions) {
      action.cleanup();
    }

    resolver.cleanup();
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    SHVar finalOutput{};
    bool matched = false;
    resolver.reassign();
    for (auto i = 0; i < _ncases; i++) {
      auto &case_ = _cases[i];
      auto &action = _actions[i];
      if (case_ == input || case_.valueType == None) {
        action.activate(context, input, finalOutput);
        matched = true;
        break;
      }
    }
    if (!matched) {
      throw ActivationError("Failed to match input, no matching case is present.");
    }
    return _pass ? input : finalOutput;
  }

  VariableResolver resolver;
  std::vector<OwnedVar> _cases;
  std::vector<OwnedVar> _pcases;
  std::vector<ShardsVar> _actions;
  std::vector<SHVar> _full;
  int _ncases = 0;
  bool _pass = true;
};

struct Sub {
  ShardsVar _shards{};
  SHComposeResult _composition{};

  static SHOptionalString help() {
    return SHCCSTR("Activates a shard or a sequence of shards independently, without consuming the input. I.e. the input of the "
                   "Sub flow will also be its output regardless of the shards activated in this Sub flow.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("The input value passed to this Sub flow (and hence to the shard or sequence of shards in this Sub flow).");
  }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The output of this Sub flow (which is the same as its input)."); }

  static SHParametersInfo parameters() {
    static Parameters params{
        {"Shards", SHCCSTR("The shard or sequence of shards to execute in the Sub flow."), {CoreInfo::ShardsOrNone}}};
    return params;
  }

  void setParam(int index, const SHVar &value) { _shards = value; }

  SHVar getParam(int index) { return _shards; }

  SHExposedTypesInfo requiredVariables() { return _composition.requiredInfo; }

  SHTypeInfo compose(const SHInstanceData &data) {
    _composition = _shards.compose(data);
    return data.inputType;
  }

  SHExposedTypesInfo exposedVariables() { return _composition.exposedInfo; }

  void warmup(SHContext *ctx) { _shards.warmup(ctx); }

  void cleanup() { _shards.cleanup(); }

  SHVar activate(SHContext *context, const SHVar &input) {
    SHVar output{};
    _shards.activate(context, input, output);
    return input;
  }
};

struct HashedShards {
  ShardsVar _shards{};
  SHComposeResult _composition{};

  Types _outputTableTypes{{CoreInfo::AnyType, CoreInfo::Int2Type}};
  static inline std::array<SHString, 2> OutputTableKeys{"Result", "Hash"};
  Type _outputTableType = Type::TableOf(_outputTableTypes, OutputTableKeys);

  TableVar _outputTable{};

  static SHOptionalString help() {
    return SHCCSTR("Activates a shard or a sequence of shards independently, without "
                   "consuming the input. In other words, the ouput of the sub flow will "
                   "be its input regardless of the shards activated in this sub flow.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The value given to the shard or sequence of shards in this sub flow."); }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The output of this shard will be its input."); }

  static SHParametersInfo parameters() {
    static Parameters params{{"Shards", SHCCSTR("The shards to execute in the sub flow."), {CoreInfo::ShardsOrNone}}};
    return params;
  }

  void setParam(int index, const SHVar &value) { _shards = value; }

  SHVar getParam(int index) { return _shards; }

  SHTypeInfo compose(const SHInstanceData &data) {
    _composition = _shards.compose(data);
    _outputTableTypes._types[0] = _composition.outputType;
    return _outputTableType;
  }

  SHExposedTypesInfo exposedVariables() { return _composition.exposedInfo; }

  void warmup(SHContext *ctx) { _shards.warmup(ctx); }

  void cleanup() { _shards.cleanup(); }

  SHVar activate(SHContext *context, const SHVar &input) {
    SHVar output{};
    SHVar hash{};
    _shards.activateHashed(context, input, output, hash);
    _outputTable["Result"] = output;
    _outputTable["Hash"] = hash;
    return _outputTable;
  }
};

void registerFlowShards() {
  REGISTER_SHARD("Cond", Cond);
  REGISTER_SHARD("Maybe", Maybe);
  REGISTER_SHARD("Await", Await);
  REGISTER_SHARD("When", When<true>);
  REGISTER_SHARD("WhenNot", When<false>);
  REGISTER_SHARD("If", IfBlock);
  REGISTER_SHARD("Match", Match);
  REGISTER_SHARD("Sub", Sub);
  REGISTER_SHARD("Hashed", HashedShards);
}
}; // namespace shards
