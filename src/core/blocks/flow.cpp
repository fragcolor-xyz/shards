/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "blockwrapper.hpp"
#include "chainblocks.h"
#include "foundation.hpp"
#include "shared.hpp"

namespace chainblocks {
static Type condBlockSeqs = Type::SeqOf(CoreInfo::BlocksOrNone);
static ParamsInfo condParamsInfo =
    ParamsInfo(ParamsInfo::Param("Chains",
                                 CBCCSTR("A sequence of blocks, interleaving condition test predicate "
                                         "and action to execute if the condition matches."),
                                 condBlockSeqs),
               ParamsInfo::Param("Passthrough", CBCCSTR("The output of this block will be its input."), CoreInfo::BoolType),
               ParamsInfo::Param("Threading",
                                 CBCCSTR("Will not short circuit after the first true test expression. "
                                         "The threaded value gets used in only the action and not the "
                                         "test part of the clause."),
                                 CoreInfo::BoolType));

struct Cond {
  // WORKS but TODO refactor using newer abstracted types
  CBVar _chains{};
  std::vector<std::vector<CBlock *>> _conditions;
  std::vector<std::vector<CBlock *>> _actions;
  bool _passthrough = true;
  bool _threading = false;
  CBComposeResult _chainValidation{};

  static CBOptionalString help() {
    return CBCCSTR("Takes a sequence of conditions and predicates. "
                   "Evaluates each condition one by one and if one matches, "
                   "executes the associated action.");
  }

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBOptionalString inputHelp() {
    return CBCCSTR("The value that will be passed to each predicate and action "
                   "to execute.");
  }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static CBOptionalString outputHelp() {
    return CBCCSTR("The input of the block if `Passthrough` is `true`; otherwise, the "
                   "output of the action of the first matching condition.");
  }

  static CBParametersInfo parameters() { return CBParametersInfo(condParamsInfo); }

  void warmup(CBContext *ctx) {
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
      auto blocks = *it;
      for (auto jt = blocks.rbegin(); jt != blocks.rend(); ++jt) {
        auto block = *jt;
        block->cleanup(block);
      }
    }
    for (auto it = _actions.rbegin(); it != _actions.rend(); ++it) {
      auto blocks = *it;
      for (auto jt = blocks.rbegin(); jt != blocks.rend(); ++jt) {
        auto block = *jt;
        block->cleanup(block);
      }
    }
  }

  void destroy() {
    for (auto it = _conditions.rbegin(); it != _conditions.rend(); ++it) {
      auto blocks = *it;
      for (auto jt = blocks.rbegin(); jt != blocks.rend(); ++jt) {
        auto block = *jt;
        block->cleanup(block);
        block->destroy(block);
      }
    }
    for (auto it = _actions.rbegin(); it != _actions.rend(); ++it) {
      auto blocks = *it;
      for (auto jt = blocks.rbegin(); jt != blocks.rend(); ++jt) {
        auto block = *jt;
        block->cleanup(block);
        block->destroy(block);
      }
    }
    destroyVar(_chains);
    chainblocks::arrayFree(_chainValidation.exposedInfo);
    chainblocks::arrayFree(_chainValidation.requiredInfo);
  }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0: {
      destroy();
      _conditions.clear();
      _actions.clear();
      cloneVar(_chains, value);
      if (value.valueType == Seq) {
        auto counter = value.payload.seqValue.len;
        if (counter % 2)
          throw CBException("Cond: first parameter must contain a sequence of "
                            "pairs [condition to check & action to perform if "
                            "check passed (true)].");
        _conditions.resize(counter / 2);
        _actions.resize(counter / 2);
        auto idx = 0;
        for (uint32_t i = 0; i < counter; i++) {
          auto val = value.payload.seqValue.elements[i];
          if (i % 2) { // action
            if (val.valueType == Block) {
              assert(!val.payload.blockValue->owned);
              val.payload.blockValue->owned = true;
              _actions[idx].push_back(val.payload.blockValue);
            } else { // seq
              for (uint32_t y = 0; y < val.payload.seqValue.len; y++) {
                assert(val.payload.seqValue.elements[y].valueType == Block);
                auto blk = val.payload.seqValue.elements[y].payload.blockValue;
                assert(!blk->owned);
                blk->owned = true;
                _actions[idx].push_back(blk);
              }
            }

            idx++;
          } else { // condition
            if (val.valueType == Block) {
              assert(!val.payload.blockValue->owned);
              val.payload.blockValue->owned = true;
              _conditions[idx].push_back(val.payload.blockValue);
            } else { // seq
              for (uint32_t y = 0; y < val.payload.seqValue.len; y++) {
                assert(val.payload.seqValue.elements[y].valueType == Block);
                auto blk = val.payload.seqValue.elements[y].payload.blockValue;
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

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _chains;
    case 1:
      return Var(_passthrough);
    case 2:
      return Var(_threading);
    default:
      break;
    }
    return Var();
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    // Free any previous result!
    chainblocks::arrayFree(_chainValidation.exposedInfo);
    chainblocks::arrayFree(_chainValidation.requiredInfo);

    // Validate condition chains, altho they do not influence anything we need
    // to report errors
    for (const auto &action : _conditions) {
      auto validation = composeChain(
          action,
          [](const CBlock *errorBlock, const char *errorTxt, bool nonfatalWarning, void *userData) {
            if (!nonfatalWarning) {
              CBLOG_ERROR("Cond: failed inner chain validation, error: {}", errorTxt);
              throw CBException("Cond: failed inner chain validation.");
            } else {
              CBLOG_INFO("Cond: warning during inner chain validation: {}", errorTxt);
            }
          },
          this, data);
      if (validation.outputType.basicType != CBType::Bool) {
        throw ComposeError("Cond - expected Bool output from predicate blocks");
      }
      chainblocks::arrayFree(validation.exposedInfo);
      chainblocks::arrayFree(validation.requiredInfo);
    }

    // Evaluate all actions, all must return the same type in order to be safe
    CBTypeInfo previousType{};
    auto idx = 0;
    auto first = true;
    auto exposing = true;
    for (const auto &action : _actions) {
      auto validation = composeChain(
          action,
          [](const CBlock *errorBlock, const char *errorTxt, bool nonfatalWarning, void *userData) {
            if (!nonfatalWarning) {
              CBLOG_ERROR("Cond: failed inner chain validation, error: {}", errorTxt);
              throw CBException("Cond: failed inner chain validation.");
            } else {
              CBLOG_INFO("Cond: warning during inner chain validation: {}", errorTxt);
            }
          },
          this, data);

      if (first) {
        // A first valid exposedInfo array is our gold
        _chainValidation = validation;
        first = false;
      } else {
        if (exposing) {
          auto curlen = _chainValidation.exposedInfo.len;
          auto newlen = validation.exposedInfo.len;
          if (curlen != newlen) {
            CBLOG_INFO("Cond: number of exposed variables between actions mismatch, "
                       "variables won't be exposed, flow is unpredictable!");
            exposing = false;
          }

          if (exposing) {
            for (uint32_t i = 0; i < curlen; i++) {
              if (_chainValidation.exposedInfo.elements[i] != validation.exposedInfo.elements[i]) {
                CBLOG_INFO("Cond: types of exposed variables between actions "
                           "mismatch, variables won't be exposed, flow is "
                           "unpredictable!");
                exposing = false;
                break;
              }
            }
          }
        }

        // free the exposed info part
        chainblocks::arrayFree(validation.exposedInfo);
        chainblocks::arrayFree(validation.requiredInfo);

        if (!exposing) {
          // make sure we expose nothing in this case!
          chainblocks::arrayFree(_chainValidation.exposedInfo);
          chainblocks::arrayFree(_chainValidation.requiredInfo);
        }
      }

      if (idx > 0 && !_passthrough && !validation.flowStopper && validation.outputType != previousType)
        throw CBException("Cond: output types between actions mismatch.");

      idx++;
      previousType = validation.outputType;
    }

    return _passthrough ? data.inputType : previousType;
  }

  CBExposedTypesInfo exposedVariables() { return _chainValidation.exposedInfo; }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(_actions.size() == 0)) {
      if (_passthrough)
        return input;
      else
        return {}; // None
    }

    auto idx = 0;
    CBVar actionInput = input;
    CBVar finalOutput{};
    for (auto &cond : _conditions) {
      CBVar output{};
      CBlocks blocks{&cond[0], (uint32_t)cond.size(), 0};
      auto state = activateBlocks2(blocks, context, input, output);
      // conditional flow so we might have "returns" form (And) (Or)
      if (unlikely(state > CBChainState::Return))
        return output;

      if (output.payload.boolValue) {
        // Do the action if true!
        // And stop here
        output = {};
        CBlocks action{&_actions[idx][0], (uint32_t)_actions[idx].size(), 0};
        state = activateBlocks(action, context, actionInput, output);
        if (state != CBChainState::Continue)
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
  CBTypesInfo inputTypes() {
    if (_blocks) {
      auto blks = _blocks.blocks();
      return blks.elements[0]->inputTypes(blks.elements[0]);
    } else {
      return CoreInfo::AnyType;
    }
  }
  static CBOptionalString inputHelp() { return CBCCSTR("Must match the input types of the first block in the sequence."); }

  CBTypesInfo outputTypes() {
    if (_blocks) {
      auto blks = _blocks.blocks();
      return blks.elements[0]->outputTypes(blks.elements[0]);
    } else {
      return CoreInfo::AnyType;
    }
  }
  static CBOptionalString outputHelp() { return CBCCSTR("Will match the output types of the first block of the sequence."); }

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) { _blocks = value; }

  CBVar getParam(int index) { return _blocks; }

  void cleanup() {
    if (_blocks)
      _blocks.cleanup();
  }

  void warmup(CBContext *ctx) {
    if (_blocks)
      _blocks.warmup(ctx);
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    if (_blocks)
      _composition = _blocks.compose(data);
    else
      _composition = {};

    return _composition.outputType;
  }

  CBExposedTypesInfo exposedVariables() { return _composition.exposedInfo; }

protected:
  BlocksVar _blocks{};
  CBComposeResult _composition{};
  static inline Parameters _params{{"Blocks", CBCCSTR("The blocks to activate."), {CoreInfo::BlocksOrNone}}};
};

struct Maybe : public BaseSubFlow {
  static CBOptionalString help() {
    return CBCCSTR("Attempts to activate a block or a sequence of blocks. Upon "
                   "failure, activate another block or sequence of blocks.");
  }

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _blocks = value;
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

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _blocks;
    case 1:
      return _elseBlks;
    case 2:
      return Var(_silent);
    default:
      throw InvalidParameterIndex();
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    // exposed stuff should be balanced
    // and output should the same
    CBComposeResult elseComp{};
    if (_elseBlks) {
      elseComp = _elseBlks.compose(data);
    }

    if (_blocks)
      _composition = _blocks.compose(data);
    else
      _composition = {};

    const auto nextIsNone =
        data.outputTypes.len == 0 || (data.outputTypes.len == 1 && data.outputTypes.elements[0].basicType == None);

    if (!nextIsNone && !elseComp.flowStopper && _composition.outputType != elseComp.outputType) {
      CBLOG_ERROR("{} != {}", _composition.outputType, elseComp.outputType);
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

  void warmup(CBContext *ctx) {
    if (_elseBlks)
      _elseBlks.warmup(ctx);

    BaseSubFlow::warmup(ctx);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    CBVar output{};
    if (likely(_blocks)) {
      try {
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
        _blocks.activate(context, input, output);
      } catch (const ActivationError &ex) {
        if (likely(!context->onLastResume)) {
          if (!_silent) {
            CBLOG_WARNING("Maybe block Ignored an error: {}", ex.what());
          }
          context->resetCancelFlow();
          if (_elseBlks)
            _elseBlks.activate(context, input, output);
        } else {
          throw ex;
        }
      }
    }
    return output;
  }

private:
  BlocksVar _elseBlks{};
  bool _silent{false};
  static inline Parameters _params{BaseSubFlow::_params,
                                   {{"Else", CBCCSTR("The blocks to activate on failure."), {CoreInfo::BlocksOrNone}},
                                    {"Silent",
                                     CBCCSTR("If logging should be disabled while running the blocks (this "
                                             "will also disable (Log) and (Msg) blocks) and no warning "
                                             "message should be printed on failure."),
                                     {CoreInfo::BoolType}}}};
};

struct Await : public BaseSubFlow {
  static CBOptionalString help() {
    return CBCCSTR("Executes a block or a sequence of blocks asynchronously "
                   "and awaits their completion.");
  }

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) { _blocks = value; }

  CBVar getParam(int index) { return _blocks; }

  CBTypeInfo compose(const CBInstanceData &data) {
    auto dataCopy = data;
    // flag that we might use a worker
    dataCopy.onWorkerThread = true;
    return BaseSubFlow::compose(dataCopy);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    return awaitne(
        context,
        [&] {
          CBVar output{};
          _blocks.activate(context, input, output);
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
    std::array<CBComposeResult, size> v{vals...};
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

  CBExposedTypesInfo exposedVariables() { return _composition.exposedInfo; }
  CBComposeResult _composition{};
};

template <bool COND> struct When {
  static CBOptionalString help() {
    if constexpr (COND == true) {
      return CBCCSTR("Conditonal block that only executes the action if the "
                     "predicate is true.");
    } else {
      return CBCCSTR("Conditonal block that only executes the action if the "
                     "predicate is false.");
    }
  }

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBOptionalString inputHelp() { return CBCCSTR("The value that will be passed to the predicate."); }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static CBOptionalString outputHelp() {
    if constexpr (COND == true) {
      return CBCCSTR("The input of the block if `Passthrough` is `true`, or the "
                     "`Predicate` is `false`; otherwise, the output of the `Action`.");
    } else {
      return CBCCSTR("The input of the block if `Passthrough` is `true`, or the "
                     "`Predicate` is `true`; otherwise, the output of the `Action`.");
    }
  }

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
    if (index == 0)
      _cond = value;
    else if (index == 1)
      _action = value;
    else
      _passth = value.payload.boolValue;
  }

  CBVar getParam(int index) {
    if (index == 0)
      return _cond;
    else if (index == 1)
      return _action;
    else
      return Var(_passth);
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    // both not exposing!
    const auto cres = _cond.compose(data);
    if (cres.outputType.basicType != CBType::Bool) {
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

  void warmup(CBContext *ctx) {
    _cond.warmup(ctx);
    _action.warmup(ctx);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    CBVar output{};
    auto state = _cond.activate<true>(context, input, output);
    if (unlikely(state > CBChainState::Return))
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
      {"Predicate", CBCCSTR("The predicate to evaluate in order to trigger Action."), {CoreInfo::BlocksOrNone}},
      {"Action",
       CBCCSTR("The blocks to activate on when Predicate is true for When and "
               "false for WhenNot."),
       {CoreInfo::BlocksOrNone}},
      {"Passthrough", CBCCSTR("The output of this block will be its input."), {CoreInfo::BoolType}}};
  BlocksVar _cond{};
  BlocksVar _action{};
  bool _passth = true;
};

struct IfBlock {
  static CBOptionalString help() { return CBCCSTR("Evaluates a predicate and executes an action."); }

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBOptionalString inputHelp() { return CBCCSTR("The value that will be passed to the predicate."); }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static CBOptionalString outputHelp() {
    return CBCCSTR("The input of the block if `Passthrough` is `true`; otherwise, the "
                   "output of the action that was performed (i.e. `Then` or `Else`).");
  }

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
    if (index == 0)
      _cond = value;
    else if (index == 1)
      _then = value;
    else if (index == 2)
      _else = value;
    else
      _passth = value.payload.boolValue;
  }

  CBVar getParam(int index) {
    if (index == 0)
      return _cond;
    else if (index == 1)
      return _then;
    else if (index == 2)
      return _else;
    else
      return Var(_passth);
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    // both not exposing!
    const auto cres = _cond.compose(data);
    if (cres.outputType.basicType != CBType::Bool) {
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

  void warmup(CBContext *ctx) {
    _cond.warmup(ctx);
    _then.warmup(ctx);
    _else.warmup(ctx);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    CBVar output{};
    auto state = _cond.activate<true>(context, input, output);
    if (unlikely(state > CBChainState::Return))
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
       CBCCSTR("The predicate to evaluate in order to trigger `Then` (when "
               "`true`) or `Else` (when `false`)."),
       {CoreInfo::BlocksOrNone}},
      {"Then", CBCCSTR("The blocks to activate when `Predicate` is `true`."), CoreInfo::BlocksOrNone},
      {"Else", CBCCSTR("The blocks to activate when `Predicate` is `false`."), CoreInfo::BlocksOrNone},
      {"Passthrough", CBCCSTR("The output of this block will be its input."), {CoreInfo::BoolType}}};
  BlocksVar _cond{};
  BlocksVar _then{};
  BlocksVar _else{};
  bool _passth = false;
};

struct Match {
  static CBOptionalString help() {
    return CBCCSTR("Compares the input with the declared cases (in order of the declaration) and activates the block of the "
                   "first matched case.");
  }

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBOptionalString inputHelp() { return CBCCSTR("The value that's compared with the declared cases."); }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static CBOptionalString outputHelp() {
    return CBCCSTR("Same value as input if `:Passthrough` is `true` else the output of the matched case's block if "
                   "`:Passthrough` is `false`.");
  }

  static inline Parameters params{
      {"Cases", CBCCSTR("Values to match against the input. A `nil` case will match anything."), {CoreInfo::AnySeqType}},
      {"Passthrough",
       CBCCSTR("Parameter to control the block's output. `true` allows the `Match` block's input itself to appear as its output; "
               "`false` allows the matched block's output to appear as `Match` block's output."),
       {CoreInfo::BoolType}}};
  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0: {
      auto counter = value.payload.seqValue.len;
      if (counter % 2)
        throw CBException("Match: first parameter must contain a sequence of pairs [variable "
                          "to compare & action to perform if matches].");
      _pcases.resize(counter / 2);
      _cases.resize(counter / 2);
      _actions.resize(counter / 2);
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
      _ncases = int(counter);
    } break;
    case 1:
      _pass = value.payload.boolValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_full);
    case 1:
      return Var(_pass);
    default:
      return Var::Empty;
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    for (auto &case_ : _pcases) {
      if (case_.valueType != None) {
        // must compare deeply
        bool hasVariables = false;
        TypeInfo cinfo(case_, data, &hasVariables);
        if (cinfo != data.inputType) {
          throw ComposeError("Match: each case must match the input type!, found a mismatch.");
        }
      }
    }

    CBTypeInfo firstOutput{};
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
              throw ComposeError("Match: when not Passthrough output types "
                                 "must match between cases.");
            }
          }
        }
      }
    }

    return _pass ? data.inputType : firstOutput;
  }

  void warmup(CBContext *context) {
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

  CBVar activate(CBContext *context, const CBVar &input) {
    CBVar finalOutput{};
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
  std::vector<BlocksVar> _actions;
  std::vector<CBVar> _full;
  int _ncases = 0;
  bool _pass = true;
};

struct Sub {
  BlocksVar _blocks{};
  CBComposeResult _composition{};

  static CBOptionalString help() {
    return CBCCSTR("Activates a block or a sequence of blocks independently, without "
                   "consuming the input. In other words, the ouput of the sub flow will "
                   "be its input regardless of the blocks activated in this sub flow.");
  }

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBOptionalString inputHelp() { return CBCCSTR("The value given to the block or sequence of blocks in this sub flow."); }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static CBOptionalString outputHelp() { return CBCCSTR("The output of this block will be its input."); }

  static CBParametersInfo parameters() {
    static Parameters params{{"Blocks", CBCCSTR("The blocks to execute in the sub flow."), {CoreInfo::BlocksOrNone}}};
    return params;
  }

  void setParam(int index, const CBVar &value) { _blocks = value; }

  CBVar getParam(int index) { return _blocks; }

  CBTypeInfo compose(const CBInstanceData &data) {
    _composition = _blocks.compose(data);
    return data.inputType;
  }

  CBExposedTypesInfo exposedVariables() { return _composition.exposedInfo; }

  void warmup(CBContext *ctx) { _blocks.warmup(ctx); }

  void cleanup() { _blocks.cleanup(); }

  CBVar activate(CBContext *context, const CBVar &input) {
    CBVar output{};
    _blocks.activate(context, input, output);
    return input;
  }
};

struct HashedBlocks {
  BlocksVar _blocks{};
  CBComposeResult _composition{};

  Types _outputTableTypes{{CoreInfo::AnyType, CoreInfo::Int2Type}};
  static inline std::array<CBString, 2> OutputTableKeys{"Result", "Hash"};
  Type _outputTableType = Type::TableOf(_outputTableTypes, OutputTableKeys);

  TableVar _outputTable{};

  static CBOptionalString help() {
    return CBCCSTR("Activates a block or a sequence of blocks independently, without "
                   "consuming the input. In other words, the ouput of the sub flow will "
                   "be its input regardless of the blocks activated in this sub flow.");
  }

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBOptionalString inputHelp() { return CBCCSTR("The value given to the block or sequence of blocks in this sub flow."); }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static CBOptionalString outputHelp() { return CBCCSTR("The output of this block will be its input."); }

  static CBParametersInfo parameters() {
    static Parameters params{{"Blocks", CBCCSTR("The blocks to execute in the sub flow."), {CoreInfo::BlocksOrNone}}};
    return params;
  }

  void setParam(int index, const CBVar &value) { _blocks = value; }

  CBVar getParam(int index) { return _blocks; }

  CBTypeInfo compose(const CBInstanceData &data) {
    _composition = _blocks.compose(data);
    _outputTableTypes._types[0] = _composition.outputType;
    return _outputTableType;
  }

  CBExposedTypesInfo exposedVariables() { return _composition.exposedInfo; }

  void warmup(CBContext *ctx) { _blocks.warmup(ctx); }

  void cleanup() { _blocks.cleanup(); }

  CBVar activate(CBContext *context, const CBVar &input) {
    CBVar output{};
    CBVar hash{};
    _blocks.activateHashed(context, input, output, hash);
    _outputTable["Result"] = output;
    _outputTable["Hash"] = hash;
    return _outputTable;
  }
};

void registerFlowBlocks() {
  REGISTER_CBLOCK("Cond", Cond);
  REGISTER_CBLOCK("Maybe", Maybe);
  REGISTER_CBLOCK("Await", Await);
  REGISTER_CBLOCK("When", When<true>);
  REGISTER_CBLOCK("WhenNot", When<false>);
  REGISTER_CBLOCK("If", IfBlock);
  REGISTER_CBLOCK("Match", Match);
  REGISTER_CBLOCK("Sub", Sub);
  REGISTER_CBLOCK("Hashed", HashedBlocks);
}
}; // namespace chainblocks
