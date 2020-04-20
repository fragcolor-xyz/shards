/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#include "blockwrapper.hpp"
#include "chainblocks.h"
#include "foundation.hpp"
#include "shared.hpp"
#include <taskflow/taskflow.hpp>

#define HANDLE_FLOW(_ctx_, _output_)                                           \
  if (_ctx_->state != CBChainState::Continue) {                                \
    if (_ctx_->state == CBChainState::Return)                                  \
      _ctx_->state = CBChainState::Continue;                                   \
    else                                                                       \
      return _output_;                                                         \
  }

namespace chainblocks {
static ParamsInfo condParamsInfo = ParamsInfo(
    ParamsInfo::Param(
        "Chains",
        "A sequence of chains, interleaving condition test predicate and "
        "action to execute if the condition matches.",
        CoreInfo::BlockSeqOrNone),
    ParamsInfo::Param(
        "Passthrough",
        "The input of this block will be the output. (default: true)",
        CoreInfo::BoolType),
    ParamsInfo::Param("Threading",
                      "Will not short circuit after the first true test "
                      "expression. The threaded value gets used in only the "
                      "action and not the test part of the clause.",
                      CoreInfo::BoolType));

struct Cond {
  // WORKS but TODO refactor using newer abstracted types
  CBVar _chains{};
  std::vector<std::vector<CBlock *>> _conditions;
  std::vector<std::vector<CBlock *>> _actions;
  bool _passthrough = true;
  bool _threading = false;
  CBValidationResult _chainValidation{};

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static CBParametersInfo parameters() {
    return CBParametersInfo(condParamsInfo);
  }

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
  }

  void setParam(int index, CBVar value) {
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

    // Validate condition chains, altho they do not influence anything we need
    // to report errors
    for (const auto &action : _conditions) {
      auto validation = validateConnections(
          action,
          [](const CBlock *errorBlock, const char *errorTxt,
             bool nonfatalWarning, void *userData) {
            if (!nonfatalWarning) {
              LOG(ERROR) << "Cond: failed inner chain validation, error: "
                         << errorTxt;
              throw CBException("Cond: failed inner chain validation.");
            } else {
              LOG(INFO) << "Cond: warning during inner chain validation: "
                        << errorTxt;
            }
          },
          this, data, false);
      chainblocks::arrayFree(validation.exposedInfo);
    }

    // Evaluate all actions, all must return the same type in order to be safe
    CBTypeInfo previousType{};
    auto idx = 0;
    auto first = true;
    auto exposing = true;
    for (const auto &action : _actions) {
      auto validation = validateConnections(
          action,
          [](const CBlock *errorBlock, const char *errorTxt,
             bool nonfatalWarning, void *userData) {
            if (!nonfatalWarning) {
              LOG(ERROR) << "Cond: failed inner chain validation, error: "
                         << errorTxt;
              throw CBException("Cond: failed inner chain validation.");
            } else {
              LOG(INFO) << "Cond: warning during inner chain validation: "
                        << errorTxt;
            }
          },
          this, data, false);

      if (first) {
        // A first valid exposedInfo array is our gold
        _chainValidation = validation;
        first = false;
      } else {
        if (exposing) {
          auto curlen = _chainValidation.exposedInfo.len;
          auto newlen = validation.exposedInfo.len;
          if (curlen != newlen) {
            LOG(INFO) << "Cond: number of exposed variables between actions "
                         "mismatch, "
                         "variables won't be exposed, flow is unpredictable!";
            exposing = false;
          }

          if (exposing) {
            for (uint32_t i = 0; i < curlen; i++) {
              if (_chainValidation.exposedInfo.elements[i] !=
                  validation.exposedInfo.elements[i]) {
                LOG(INFO)
                    << "Cond: types of exposed variables between actions "
                       "mismatch, "
                       "variables won't be exposed, flow is unpredictable!";
                exposing = false;
                break;
              }
            }
          }
        }

        // free the exposed info part
        chainblocks::arrayFree(validation.exposedInfo);

        if (!exposing) {
          // make sure we expose nothing in this case!
          chainblocks::arrayFree(_chainValidation.exposedInfo);
        }
      }

      if (idx > 0 && !_passthrough && !validation.flowStopper &&
          validation.outputType != previousType)
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
      activateBlocks(blocks, context, input, output);
      HANDLE_FLOW(context, input);
      if (output == Var::True) {
        // Do the action if true!
        // And stop here
        output = {};
        CBlocks action{&_actions[idx][0], (uint32_t)_actions[idx].size(), 0};
        auto state = activateBlocks(action, context, actionInput, output);
        CHECK_STATE(state, output);
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

  CBTypesInfo outputTypes() {
    if (_blocks) {
      auto blks = _blocks.blocks();
      return blks.elements[0]->outputTypes(blks.elements[0]);
    } else {
      return CoreInfo::AnyType;
    }
  }

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, CBVar value) { _blocks = value; }

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
  CBValidationResult _composition{};
  static inline Parameters _params{
      {"Blocks", "The blocks to activate.", {CoreInfo::BlocksOrNone}}};
};

struct MaybeRestart : public BaseSubFlow {
  CBVar activate(CBContext *context, const CBVar &input) {
    CBVar output{};
    if (likely(_blocks)) {
      try {
        CHECK_STATE(_blocks.activate(context, input, output), output);
      } catch (const ActivationError &ex) {
        if (ex.triggerFailure()) {
          LOG(WARNING) << "Maybe block Ignored a failure: " << ex.what();
        }
        context->state = CBChainState::Restart;
      }
    }
    return output;
  }
};

struct Maybe : public BaseSubFlow {
  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, CBVar value) {
    if (index == 0)
      _blocks = value;
    else
      _elseBlks = value;
  }

  CBVar getParam(int index) {
    if (index == 0)
      return _blocks;
    else
      return _elseBlks;
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    // exposed stuff should be balanced
    // and output should the same
    CBValidationResult elseComp{};
    if (_elseBlks)
      elseComp = _elseBlks.compose(data);

    if (_blocks)
      _composition = _blocks.compose(data);
    else
      _composition = {};

    if (!elseComp.flowStopper &&
        _composition.outputType != elseComp.outputType) {
      throw ComposeError(
          "Maybe: output types mismatch between the two possible flows!");
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
        CHECK_STATE(_blocks.activate(context, input, output), output);
      } catch (const ActivationError &ex) {
        if (ex.triggerFailure()) {
          LOG(WARNING) << "Maybe block Ignored a failure: " << ex.what();
        }
        _elseBlks.activate(context, input, output);
      }
    }
    return output;
  }

private:
  BlocksVar _elseBlks{};
  static inline Parameters _params{BaseSubFlow::_params,
                                   {{"Else",
                                     "The blocks to activate on failure.",
                                     {CoreInfo::BlocksOrNone}}}};
};

struct Await : public BaseSubFlow {
  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, CBVar value) { _blocks = value; }

  CBVar getParam(int index) { return _blocks; }

  CBVar activate(CBContext *context, const CBVar &input) {
    CBVar output{};
    if (likely(_blocks)) {
      // avoid nesting, if we are alrady inside a worker
      // run normally
      if (Tasks.this_worker_id() == -1) {
        AsyncOp<InternalCore> op(context);
        op.sidechain(Tasks,
                     [&]() { _blocks.activate(context, input, output); });
      } else {
        _blocks.activate(context, input, output);
      }
    }
    return output;
  }

private:
  tf::Executor &Tasks{Singleton<tf::Executor>::value};
};

// Not used actually
// I decided to not expose from When and If
// Exposing from flow stuff is dangerous
struct ExposerFlow {
  template <typename... Validations> void merge(Validations... vals) {
    constexpr int size = sizeof...(vals);
    std::array<CBValidationResult, size> v{vals...};
    IterableExposedInfo master(v[0].exposedInfo);
    for (size_t i = 1; i < v.size(); i++) {
      IterableExposedInfo sub(v[i].exposedInfo);
      if (master.size() != sub.size()) {
        throw ComposeError(
            "Flow is unbalanced, exposing a different amount of variables.");
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
  CBValidationResult _composition{};
};

template <bool COND> struct When {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, CBVar value) {
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

    if (!ares.flowStopper && !_passth) {
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
    _cond.activate(context, input, output);
    HANDLE_FLOW(context, input);
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
      {"Predicate",
       "The predicate to evaluate in order to trigger Action.",
       {CoreInfo::BlocksOrNone}},
      {"Action",
       "The blocks to activate on when Predicate is true for When and false "
       "for WhenNot.",
       {CoreInfo::BlocksOrNone}},
      {"Passthrough",
       "The input of this block will be the output. (default: true)",
       {CoreInfo::BoolType}}};
  BlocksVar _cond{};
  BlocksVar _action{};
  bool _passth = true;
};

struct IfBlock {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, CBVar value) {
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

    if (!tres.flowStopper && !eres.flowStopper && !_passth) {
      if (tres.outputType != eres.outputType) {
        throw ComposeError("If - Passthrough is false but action output types "
                           "do not match.");
      }
    }
    return data.inputType;
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
    _cond.activate(context, input, output);
    HANDLE_FLOW(context, input);
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
       "The predicate to evaluate in order to trigger Action.",
       {CoreInfo::BlocksOrNone}},
      {"Then", "The blocks to activate when Predicate is true.",
       CoreInfo::BlocksOrNone},
      {"Else", "The blocks to activate when Predicate is false.",
       CoreInfo::BlocksOrNone},
      {"Passthrough",
       "The input of this block will be the output. (default: true)",
       {CoreInfo::BoolType}}};
  BlocksVar _cond{};
  BlocksVar _then{};
  BlocksVar _else{};
  bool _passth = false;
};

void registerFlowBlocks() {
  REGISTER_CBLOCK("Cond", Cond);
  REGISTER_CBLOCK("MaybeRestart", MaybeRestart);
  REGISTER_CBLOCK("Maybe", Maybe);
  REGISTER_CBLOCK("Await", Await);
  REGISTER_CBLOCK("When", When<true>);
  REGISTER_CBLOCK("WhenNot", When<false>);
  REGISTER_CBLOCK("If", IfBlock);
}
}; // namespace chainblocks
