/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#include "shared.hpp"

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
            exposing = true;
          }

          if (exposing) {
            for (uint32_t i = 0; i < curlen; i++) {
              if (_chainValidation.exposedInfo.elements[i] !=
                  validation.exposedInfo.elements[i]) {
                LOG(INFO)
                    << "Cond: types of exposed variables between actions "
                       "mismatch, "
                       "variables won't be exposed, flow is unpredictable!";
                exposing = true;
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

      if (idx > 0 && !_passthrough && validation.outputType != previousType)
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
    CBVar finalOutput = RestartChain;
    for (auto &cond : _conditions) {
      CBVar output{};
      CBlocks blocks{&cond[0], (uint32_t)cond.size(), 0};
      activateBlocks(blocks, context, input, output);
      if (output == True) {
        // Do the action if true!
        // And stop here
        output = {};
        CBlocks action{&_actions[idx][0], (uint32_t)_actions[idx].size(), 0};
        if (!activateBlocks(action, context, actionInput, output))
          return Var::Return(); // we need to propagate Return!

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

void registerFlowBlocks() { REGISTER_CBLOCK("Cond", Cond); }
}; // namespace chainblocks
