/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#include "shared.hpp"

namespace chainblocks {
static ParamsInfo condParamsInfo = ParamsInfo(
    ParamsInfo::Param(
        "Chains",
        "A sequence of chains, interleaving condition test predicate and "
        "action to execute if the condition matches.",
        CBTypesInfo(SharedTypes::blockSeqsOrNoneInfo)),
    ParamsInfo::Param(
        "Passthrough",
        "The input of this block will be the output. (default: true)",
        CBTypesInfo(SharedTypes::boolInfo)),
    ParamsInfo::Param("Threading",
                      "Will not short circuit after the first true test "
                      "expression. The threaded value gets used in only the "
                      "action and not the test part of the clause.",
                      CBTypesInfo(SharedTypes::boolInfo)));

struct Cond {
  CBVar _chains{};
  std::vector<std::vector<CBlock *>> _conditions;
  std::vector<std::vector<CBlock *>> _actions;
  bool _passthrough = true;
  bool _threading = false;
  CBValidationResult _chainValidation{};

  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }
  static CBTypesInfo outputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }
  static CBParametersInfo parameters() {
    return CBParametersInfo(condParamsInfo);
  }

  void cleanup() {
    for (const auto &blocks : _conditions) {
      for (auto block : blocks) {
        block->cleanup(block);
      }
    }
    for (const auto &blocks : _actions) {
      for (auto block : blocks) {
        block->cleanup(block);
      }
    }
  }

  void destroy() {
    for (const auto &blocks : _conditions) {
      for (auto block : blocks) {
        block->cleanup(block);
        block->destroy(block);
      }
    }
    for (const auto &blocks : _actions) {
      for (auto block : blocks) {
        block->cleanup(block);
        block->destroy(block);
      }
    }
    destroyVar(_chains);
    stbds_arrfree(_chainValidation.exposedInfo);
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
        for (auto i = 0; i < counter; i++) {
          auto val = value.payload.seqValue.elements[i];
          if (i % 2) { // action
            if (val.valueType == Block) {
              _actions[idx].push_back(val.payload.blockValue);
            } else { // seq
              for (auto y = 0; y < val.payload.seqValue.len; y++) {
                assert(val.payload.seqValue.elements[y].valueType == Block);
                _actions[idx].push_back(
                    val.payload.seqValue.elements[y].payload.blockValue);
              }
            }

            idx++;
          } else { // condition
            if (val.valueType == Block) {
              _conditions[idx].push_back(val.payload.blockValue);
            } else { // seq
              for (auto y = 0; y < val.payload.seqValue.len; y++) {
                assert(val.payload.seqValue.elements[y].valueType == Block);
                _conditions[idx].push_back(
                    val.payload.seqValue.elements[y].payload.blockValue);
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
    stbds_arrfree(_chainValidation.exposedInfo);
    _chainValidation.exposedInfo = nullptr;

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
          this, data);
      stbds_arrfree(validation.exposedInfo);
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
          this, data);

      if (first) {
        // A first valid exposedInfo array is our gold
        _chainValidation = validation;
        first = false;
      } else {
        if (exposing) {
          auto curlen = stbds_arrlen(_chainValidation.exposedInfo);
          auto newlen = stbds_arrlen(validation.exposedInfo);
          if (curlen != newlen) {
            LOG(INFO) << "Cond: number of exposed variables between actions "
                         "mismatch, "
                         "variables won't be exposed, flow is unpredictable!";
            exposing = true;
          }

          if (exposing) {
            for (auto i = 0; i < curlen; i++) {
              if (_chainValidation.exposedInfo[i] !=
                  validation.exposedInfo[i]) {
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
        stbds_arrfree(validation.exposedInfo);

        if (!exposing) {
          // make sure we expose nothing in this case!
          stbds_arrfree(_chainValidation.exposedInfo);
          _chainValidation.exposedInfo = nullptr;
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
        return CBVar(); // None
    }

    auto idx = 0;
    CBVar actionInput = input;
    CBVar finalOutput = RestartChain;
    for (const auto &cond : _conditions) {
      CBVar output{};
      if (unlikely(!activateBlocks(const_cast<CBlocks>(&cond[0]), cond.size(),
                                   context, input, output))) {
        return StopChain;
      } else if (output == True) {
        // Do the action if true!
        // And stop here
        output = Empty;
        auto state = activateBlocks(&_actions[idx][0], _actions[idx].size(),
                                    context, actionInput, output);
        if (unlikely(state == FlowState::Stopping)) {
          return StopChain;
        } else if (unlikely(state == FlowState::Returning)) {
          return Var::Return();
        } else if (_threading) {
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

// Register
RUNTIME_CORE_BLOCK(Cond);
RUNTIME_BLOCK_inputTypes(Cond);
RUNTIME_BLOCK_outputTypes(Cond);
RUNTIME_BLOCK_parameters(Cond);
RUNTIME_BLOCK_compose(Cond);
RUNTIME_BLOCK_exposedVariables(Cond);
RUNTIME_BLOCK_setParam(Cond);
RUNTIME_BLOCK_getParam(Cond);
RUNTIME_BLOCK_activate(Cond);
RUNTIME_BLOCK_cleanup(Cond);
RUNTIME_BLOCK_destroy(Cond);
RUNTIME_BLOCK_END(Cond);

void registerFlowBlocks() { REGISTER_CORE_BLOCK(Cond); }
}; // namespace chainblocks
