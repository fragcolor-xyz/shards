#include "shared.hpp"

namespace chainblocks {
static TypesInfo blockSeqsOrNoneInfo =
    TypesInfo::FromManyTypes(CBTypeInfo(blockSeqInfo), CBTypeInfo(noneInfo));

static ParamsInfo condParamsInfo = ParamsInfo(
    ParamsInfo::Param("Chains",
                      "A sequence of chains, interleaving condition test and "
                      "action to execute if the condition matches.",
                      CBTypesInfo(blockSeqsOrNoneInfo)),
    ParamsInfo::Param("Passthrough",
                      "The input of this block will be the output.",
                      CBTypesInfo(boolInfo)),
    ParamsInfo::Param("Threading",
                      "Will not short circuit after the first true test "
                      "expression. The threaded value gets used in only the "
                      "action and not the test part of the clause.",
                      CBTypesInfo(boolInfo)));

struct Cond {
  CBVar _chains{};
  std::vector<std::vector<CBlock *>> _conditions;
  std::vector<std::vector<CBlock *>> _actions;
  bool _passthrough = false;
  bool _threading = false;

  static CBTypesInfo inputTypes() { return CBTypesInfo(anyInfo); }
  static CBTypesInfo outputTypes() { return CBTypesInfo(anyInfo); }
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
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0: {
      destroy();
      _conditions.clear();
      _actions.clear();
      cloneVar(_chains, value);
      if (value.valueType == Seq) {
        auto counter = stbds_arrlen(value.payload.seqValue);
        if (counter % 2)
          throw CBException("Cond: first parameter must contain a sequence of "
                            "pairs [condition to check & action to perform if "
                            "check passed (true)].");
        _conditions.resize(counter / 2);
        _actions.resize(counter / 2);
        auto idx = 0;
        for (auto i = 0; i < counter; i++) {
          auto val = value.payload.seqValue[i];
          if (i % 2) { // action
            if (val.valueType == Block) {
              _actions[idx].push_back(val.payload.blockValue);
            } else { // seq
              for (auto y = 0; y < stbds_arrlen(val.payload.seqValue); y++) {
                assert(val.payload.seqValue[y].valueType == Block);
                _actions[idx].push_back(
                    val.payload.seqValue[y].payload.blockValue);
              }
            }

            idx++;
          } else { // condition
            if (val.valueType == Block) {
              _conditions[idx].push_back(val.payload.blockValue);
            } else { // seq
              for (auto y = 0; y < stbds_arrlen(val.payload.seqValue); y++) {
                assert(val.payload.seqValue[y].valueType == Block);
                _conditions[idx].push_back(
                    val.payload.seqValue[y].payload.blockValue);
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

  CBTypeInfo inferTypes(CBTypeInfo inputType, CBExposedTypesInfo consumables) {
    if (_passthrough)
      return inputType;

    // Evaluate all actions, all must return the same type in order to be safe
    CBTypeInfo previousType{};
    auto idx = 0;
    for (const auto &action : _actions) {
      CBTypeInfo outputType = validateConnections(
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
          this, inputType);
      if (idx > 0 && outputType != previousType)
        throw CBException("Cond: output types between actions mismatch.");
      idx++;
      previousType = outputType;
    }

    return previousType;
  }

  CBVar activate(CBContext *context, CBVar input) {
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
        if (unlikely(!activateBlocks(&_actions[idx][0], _actions.size(),
                                     context, actionInput, output))) {
          return StopChain;
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

    return finalOutput;
  }
};

// Register
RUNTIME_CORE_BLOCK(Cond);
RUNTIME_BLOCK_inputTypes(Cond);
RUNTIME_BLOCK_outputTypes(Cond);
RUNTIME_BLOCK_parameters(Cond);
RUNTIME_BLOCK_inferTypes(Cond);
RUNTIME_BLOCK_setParam(Cond);
RUNTIME_BLOCK_getParam(Cond);
RUNTIME_BLOCK_activate(Cond);
RUNTIME_BLOCK_cleanup(Cond);
RUNTIME_BLOCK_destroy(Cond);
RUNTIME_BLOCK_END(Cond);

void registerFlowBlocks() { REGISTER_CORE_BLOCK(Cond); }
}; // namespace chainblocks
