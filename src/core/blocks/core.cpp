#include "../runtime.hpp"

namespace chainblocks {
struct JointOp {
  std::vector<CBVar *> _multiSortColumns;
  CBVar _columns{};

  static CBTypesInfo inputTypes() { return CBTypesInfo(CoreInfo::anySeqInfo); }
  static CBTypesInfo outputTypes() { return CBTypesInfo(CoreInfo::anySeqInfo); }

  static inline ParamsInfo joinOpParams = ParamsInfo(ParamsInfo::Param(
      "Join",
      "Other columns to join sort/filter using the input (they must be "
      "of the same length).",
      CBTypesInfo(CoreInfo::varSeqInfo)));

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      cloneVar(_columns, value);
      // resets vars fetch in activate
      _multiSortColumns.clear();
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _columns;
    default:
      break;
    }
    throw CBException("Parameter out of range.");
  }

  void cleanup() { _multiSortColumns.clear(); }

  void ensureJoinSetup(CBContext *context, const CBVar &input) {
    if (_columns.valueType != None) {
      auto len = stbds_arrlen(input.payload.seqValue);
      if (_multiSortColumns.size() == 0) {
        if (_columns.valueType == Seq) {
          auto seq = IterableSeq(_columns.payload.seqValue);
          for (const auto &col : seq) {
            auto target = contextVariable(context, col.payload.stringValue);
            if (target && target->valueType == Seq) {
              auto mseqLen = stbds_arrlen(target->payload.seqValue);
              if (len != mseqLen) {
                throw CBException(
                    "JointOp: All the sequences to be processed must have "
                    "the same length as the input sequence.");
              }
              _multiSortColumns.push_back(target);
            }
          }
        } else if (_columns.valueType ==
                   ContextVar) { // normal single context var
          auto target = contextVariable(context, _columns.payload.stringValue);
          if (target && target->valueType == Seq) {
            auto mseqLen = stbds_arrlen(target->payload.seqValue);
            if (len != mseqLen) {
              throw CBException(
                  "JointOp: All the sequences to be processed must have "
                  "the same length as the input sequence.");
            }
            _multiSortColumns.push_back(target);
          }
        }
      } else {
        for (const auto &seqVar : _multiSortColumns) {
          const auto &seq = seqVar->payload.seqValue;
          auto mseqLen = stbds_arrlen(seq);
          if (len != mseqLen) {
            throw CBException(
                "JointOp: All the sequences to be processed must have "
                "the same length as the input sequence.");
          }
        }
      }
    }
  }
};

struct Sort : public JointOp {
  std::vector<CBVar> _multiSortKeys;

  bool _desc = false;

  static inline ParamsInfo paramsInfo = ParamsInfo(
      joinOpParams,
      ParamsInfo::Param(
          "Desc",
          "If sorting should be in descending order, defaults ascending.",
          CBTypesInfo(CoreInfo::boolInfo)));

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      return JointOp::setParam(index, value);
    case 1:
      _desc = value.payload.boolValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return JointOp::getParam(index);
    case 1:
      return Var(_desc);
    default:
      break;
    }
    throw CBException("Parameter out of range.");
  }

  struct {
    bool operator()(CBVar &a, CBVar &b) const { return a > b; }
  } sortAsc;

  struct {
    bool operator()(CBVar &a, CBVar &b) const { return a < b; }
  } sortDesc;

  template <class Compare> void insertSort(CBVar seq[], int n, Compare comp) {
    int i, j;
    CBVar key;
    for (i = 1; i < n; i++) {
      key = seq[i];
      _multiSortKeys.clear();
      for (const auto &seqVar : _multiSortColumns) {
        const auto &col = seqVar->payload.seqValue;
        _multiSortKeys.push_back(col[i]);
      }
      j = i - 1;
      while (j >= 0 && comp(seq[j], key)) {
        seq[j + 1] = seq[j];
        for (const auto &seqVar : _multiSortColumns) {
          const auto &col = seqVar->payload.seqValue;
          col[j + 1] = col[j];
        }
        j = j - 1;
      }
      seq[j + 1] = key;
      auto z = 0;
      for (const auto &seqVar : _multiSortColumns) {
        const auto &col = seqVar->payload.seqValue;
        col[j + 1] = _multiSortKeys[z++];
      }
    }
  }

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    JointOp::ensureJoinSetup(context, input);
    // Sort in place
    auto len = stbds_arrlen(input.payload.seqValue);
    if (!_desc) {
      insertSort(input.payload.seqValue, len, sortAsc);
    } else {
      insertSort(input.payload.seqValue, len, sortDesc);
    }
    return input;
  }
};

struct Remove : public JointOp, public BlocksUser {
  static inline ParamsInfo paramsInfo = ParamsInfo(
      joinOpParams,
      ParamsInfo::Param("Predicate",
                        "The blocks to use as predicate, if true the item will "
                        "be popped from the sequence.",
                        CBTypesInfo(CoreInfo::blocksInfo)));

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      return JointOp::setParam(index, value);
    case 1:
      cloneVar(_blocks, value);
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return JointOp::getParam(index);
    case 1:
      return _blocks;
    default:
      break;
    }
    throw CBException("Parameter out of range.");
  }

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    JointOp::ensureJoinSetup(context, input);
    // Remove in place, will possibly remove any sorting!
    auto len = stbds_arrlen(input.payload.seqValue);
    for (auto i = len - 1; i >= 0; i--) {
      auto &var = input.payload.seqValue[i];
      CBVar output{};
      if (unlikely(!activateBlocks(_blocks.payload.seqValue, context, var,
                                   output))) {
        return StopChain;
      } else if (output == True) {
        // remove from input
        if (var.valueType >= EndOfBlittableTypes) {
          destroyVar(var);
        }
        stbds_arrdelswap(input.payload.seqValue, i);
        // remove from joined
        for (const auto &seqVar : _multiSortColumns) {
          const auto &seq = seqVar->payload.seqValue;
          auto &jvar = seq[i];
          if (var.valueType >= EndOfBlittableTypes) {
            destroyVar(jvar);
          }
          stbds_arrdelswap(seq, i);
        }
      }
    }
    return input;
  }
};

// Register Const
RUNTIME_CORE_BLOCK_FACTORY(Const);
RUNTIME_BLOCK_destroy(Const);
RUNTIME_BLOCK_inputTypes(Const);
RUNTIME_BLOCK_outputTypes(Const);
RUNTIME_BLOCK_parameters(Const);
RUNTIME_BLOCK_inferTypes(Const);
RUNTIME_BLOCK_setParam(Const);
RUNTIME_BLOCK_getParam(Const);
RUNTIME_BLOCK_activate(Const);
RUNTIME_BLOCK_END(Const);

// Register Input
RUNTIME_CORE_BLOCK_FACTORY(Input);
RUNTIME_BLOCK_inputTypes(Input);
RUNTIME_BLOCK_outputTypes(Input);
RUNTIME_BLOCK_activate(Input);
RUNTIME_BLOCK_END(Input);

// Register Sleep
RUNTIME_CORE_BLOCK_FACTORY(Sleep);
RUNTIME_BLOCK_inputTypes(Sleep);
RUNTIME_BLOCK_outputTypes(Sleep);
RUNTIME_BLOCK_parameters(Sleep);
RUNTIME_BLOCK_setParam(Sleep);
RUNTIME_BLOCK_getParam(Sleep);
RUNTIME_BLOCK_activate(Sleep);
RUNTIME_BLOCK_END(Sleep);

// Register And
RUNTIME_CORE_BLOCK_FACTORY(And);
RUNTIME_BLOCK_inputTypes(And);
RUNTIME_BLOCK_outputTypes(And);
RUNTIME_BLOCK_activate(And);
RUNTIME_BLOCK_END(And);

// Register Or
RUNTIME_CORE_BLOCK_FACTORY(Or);
RUNTIME_BLOCK_inputTypes(Or);
RUNTIME_BLOCK_outputTypes(Or);
RUNTIME_BLOCK_activate(Or);
RUNTIME_BLOCK_END(Or);

// Register Stop
RUNTIME_CORE_BLOCK_FACTORY(Stop);
RUNTIME_BLOCK_inputTypes(Stop);
RUNTIME_BLOCK_outputTypes(Stop);
RUNTIME_BLOCK_activate(Stop);
RUNTIME_BLOCK_END(Stop);

// Register Restart
RUNTIME_CORE_BLOCK_FACTORY(Restart);
RUNTIME_BLOCK_inputTypes(Restart);
RUNTIME_BLOCK_outputTypes(Restart);
RUNTIME_BLOCK_activate(Restart);
RUNTIME_BLOCK_END(Restart);

// Register Return
RUNTIME_CORE_BLOCK_FACTORY(Return);
RUNTIME_BLOCK_inputTypes(Return);
RUNTIME_BLOCK_outputTypes(Return);
RUNTIME_BLOCK_activate(Return);
RUNTIME_BLOCK_END(Return);

// Register Set
RUNTIME_CORE_BLOCK_FACTORY(Set);
RUNTIME_BLOCK_cleanup(Set);
RUNTIME_BLOCK_inputTypes(Set);
RUNTIME_BLOCK_outputTypes(Set);
RUNTIME_BLOCK_parameters(Set);
RUNTIME_BLOCK_inferTypes(Set);
RUNTIME_BLOCK_exposedVariables(Set);
RUNTIME_BLOCK_setParam(Set);
RUNTIME_BLOCK_getParam(Set);
RUNTIME_BLOCK_activate(Set);
RUNTIME_BLOCK_END(Set);

// Register Update
RUNTIME_CORE_BLOCK_FACTORY(Update);
RUNTIME_BLOCK_cleanup(Update);
RUNTIME_BLOCK_inputTypes(Update);
RUNTIME_BLOCK_outputTypes(Update);
RUNTIME_BLOCK_parameters(Update);
RUNTIME_BLOCK_inferTypes(Update);
RUNTIME_BLOCK_consumedVariables(Update);
RUNTIME_BLOCK_setParam(Update);
RUNTIME_BLOCK_getParam(Update);
RUNTIME_BLOCK_activate(Update);
RUNTIME_BLOCK_END(Update);

// Register Push
RUNTIME_CORE_BLOCK_FACTORY(Push);
RUNTIME_BLOCK_destroy(Push);
RUNTIME_BLOCK_cleanup(Push);
RUNTIME_BLOCK_inputTypes(Push);
RUNTIME_BLOCK_outputTypes(Push);
RUNTIME_BLOCK_parameters(Push);
RUNTIME_BLOCK_inferTypes(Push);
RUNTIME_BLOCK_exposedVariables(Push);
RUNTIME_BLOCK_setParam(Push);
RUNTIME_BLOCK_getParam(Push);
RUNTIME_BLOCK_activate(Push);
RUNTIME_BLOCK_END(Push);

// Register Pop
RUNTIME_CORE_BLOCK_FACTORY(Pop);
RUNTIME_BLOCK_cleanup(Pop);
RUNTIME_BLOCK_destroy(Pop);
RUNTIME_BLOCK_inputTypes(Pop);
RUNTIME_BLOCK_outputTypes(Pop);
RUNTIME_BLOCK_parameters(Pop);
RUNTIME_BLOCK_inferTypes(Pop);
RUNTIME_BLOCK_consumedVariables(Pop);
RUNTIME_BLOCK_setParam(Pop);
RUNTIME_BLOCK_getParam(Pop);
RUNTIME_BLOCK_activate(Pop);
RUNTIME_BLOCK_END(Pop);

// Register Count
RUNTIME_CORE_BLOCK_FACTORY(Count);
RUNTIME_BLOCK_cleanup(Count);
RUNTIME_BLOCK_inputTypes(Count);
RUNTIME_BLOCK_outputTypes(Count);
RUNTIME_BLOCK_parameters(Count);
RUNTIME_BLOCK_setParam(Count);
RUNTIME_BLOCK_getParam(Count);
RUNTIME_BLOCK_activate(Count);
RUNTIME_BLOCK_END(Count);

// Register Clear
RUNTIME_CORE_BLOCK_FACTORY(Clear);
RUNTIME_BLOCK_cleanup(Clear);
RUNTIME_BLOCK_inputTypes(Clear);
RUNTIME_BLOCK_outputTypes(Clear);
RUNTIME_BLOCK_parameters(Clear);
RUNTIME_BLOCK_setParam(Clear);
RUNTIME_BLOCK_getParam(Clear);
RUNTIME_BLOCK_activate(Clear);
RUNTIME_BLOCK_END(Clear);

// Register Get
RUNTIME_CORE_BLOCK_FACTORY(Get);
RUNTIME_BLOCK_cleanup(Get);
RUNTIME_BLOCK_destroy(Get);
RUNTIME_BLOCK_inputTypes(Get);
RUNTIME_BLOCK_outputTypes(Get);
RUNTIME_BLOCK_parameters(Get);
RUNTIME_BLOCK_inferTypes(Get);
RUNTIME_BLOCK_consumedVariables(Get);
RUNTIME_BLOCK_setParam(Get);
RUNTIME_BLOCK_getParam(Get);
RUNTIME_BLOCK_activate(Get);
RUNTIME_BLOCK_END(Get);

// Register Swap
RUNTIME_CORE_BLOCK_FACTORY(Swap);
RUNTIME_BLOCK_cleanup(Swap);
RUNTIME_BLOCK_inputTypes(Swap);
RUNTIME_BLOCK_outputTypes(Swap);
RUNTIME_BLOCK_parameters(Swap);
RUNTIME_BLOCK_consumedVariables(Swap);
RUNTIME_BLOCK_setParam(Swap);
RUNTIME_BLOCK_getParam(Swap);
RUNTIME_BLOCK_activate(Swap);
RUNTIME_BLOCK_END(Swap);

// Register Take
RUNTIME_CORE_BLOCK_FACTORY(Take);
RUNTIME_BLOCK_destroy(Take);
RUNTIME_BLOCK_cleanup(Take);
RUNTIME_BLOCK_consumedVariables(Take);
RUNTIME_BLOCK_inputTypes(Take);
RUNTIME_BLOCK_outputTypes(Take);
RUNTIME_BLOCK_parameters(Take);
RUNTIME_BLOCK_inferTypes(Take);
RUNTIME_BLOCK_setParam(Take);
RUNTIME_BLOCK_getParam(Take);
RUNTIME_BLOCK_activate(Take);
RUNTIME_BLOCK_END(Take);

// Register Limit
RUNTIME_CORE_BLOCK_FACTORY(Limit);
RUNTIME_BLOCK_destroy(Limit);
RUNTIME_BLOCK_inputTypes(Limit);
RUNTIME_BLOCK_outputTypes(Limit);
RUNTIME_BLOCK_parameters(Limit);
RUNTIME_BLOCK_inferTypes(Limit);
RUNTIME_BLOCK_setParam(Limit);
RUNTIME_BLOCK_getParam(Limit);
RUNTIME_BLOCK_activate(Limit);
RUNTIME_BLOCK_END(Limit);

// Register Repeat
RUNTIME_CORE_BLOCK_FACTORY(Repeat);
RUNTIME_BLOCK_inputTypes(Repeat);
RUNTIME_BLOCK_outputTypes(Repeat);
RUNTIME_BLOCK_parameters(Repeat);
RUNTIME_BLOCK_setParam(Repeat);
RUNTIME_BLOCK_getParam(Repeat);
RUNTIME_BLOCK_activate(Repeat);
RUNTIME_BLOCK_destroy(Repeat);
RUNTIME_BLOCK_cleanup(Repeat);
RUNTIME_BLOCK_exposedVariables(Repeat);
RUNTIME_BLOCK_consumedVariables(Repeat);
RUNTIME_BLOCK_inferTypes(Repeat);
RUNTIME_BLOCK_END(Repeat);

// Register Sort
RUNTIME_CORE_BLOCK(Sort);
RUNTIME_BLOCK_inputTypes(Sort);
RUNTIME_BLOCK_outputTypes(Sort);
RUNTIME_BLOCK_activate(Sort);
RUNTIME_BLOCK_parameters(Sort);
RUNTIME_BLOCK_setParam(Sort);
RUNTIME_BLOCK_getParam(Sort);
RUNTIME_BLOCK_cleanup(Sort);
RUNTIME_BLOCK_END(Sort);

// Register Repeat
RUNTIME_CORE_BLOCK(Remove);
RUNTIME_BLOCK_inputTypes(Remove);
RUNTIME_BLOCK_outputTypes(Remove);
RUNTIME_BLOCK_parameters(Remove);
RUNTIME_BLOCK_setParam(Remove);
RUNTIME_BLOCK_getParam(Remove);
RUNTIME_BLOCK_activate(Remove);
RUNTIME_BLOCK_destroy(Remove);
RUNTIME_BLOCK_cleanup(Repeat);
RUNTIME_BLOCK_inferTypes(Remove);
RUNTIME_BLOCK_END(Remove);

LOGIC_OP_DESC(Is);
LOGIC_OP_DESC(IsNot);
LOGIC_OP_DESC(IsMore);
LOGIC_OP_DESC(IsLess);
LOGIC_OP_DESC(IsMoreEqual);
LOGIC_OP_DESC(IsLessEqual);

LOGIC_OP_DESC(Any);
LOGIC_OP_DESC(All);
LOGIC_OP_DESC(AnyNot);
LOGIC_OP_DESC(AllNot);
LOGIC_OP_DESC(AnyMore);
LOGIC_OP_DESC(AllMore);
LOGIC_OP_DESC(AnyLess);
LOGIC_OP_DESC(AllLess);
LOGIC_OP_DESC(AnyMoreEqual);
LOGIC_OP_DESC(AllMoreEqual);
LOGIC_OP_DESC(AnyLessEqual);
LOGIC_OP_DESC(AllLessEqual);

namespace Math {
MATH_BINARY_BLOCK(Add);
MATH_BINARY_BLOCK(Subtract);
MATH_BINARY_BLOCK(Multiply);
MATH_BINARY_BLOCK(Divide);
MATH_BINARY_BLOCK(Xor);
MATH_BINARY_BLOCK(And);
MATH_BINARY_BLOCK(Or);
MATH_BINARY_BLOCK(Mod);
MATH_BINARY_BLOCK(LShift);
MATH_BINARY_BLOCK(RShift);

MATH_UNARY_BLOCK(Abs);
MATH_UNARY_BLOCK(Exp);
MATH_UNARY_BLOCK(Exp2);
MATH_UNARY_BLOCK(Expm1);
MATH_UNARY_BLOCK(Log);
MATH_UNARY_BLOCK(Log10);
MATH_UNARY_BLOCK(Log2);
MATH_UNARY_BLOCK(Log1p);
MATH_UNARY_BLOCK(Sqrt);
MATH_UNARY_BLOCK(Cbrt);
MATH_UNARY_BLOCK(Sin);
MATH_UNARY_BLOCK(Cos);
MATH_UNARY_BLOCK(Tan);
MATH_UNARY_BLOCK(Asin);
MATH_UNARY_BLOCK(Acos);
MATH_UNARY_BLOCK(Atan);
MATH_UNARY_BLOCK(Sinh);
MATH_UNARY_BLOCK(Cosh);
MATH_UNARY_BLOCK(Tanh);
MATH_UNARY_BLOCK(Asinh);
MATH_UNARY_BLOCK(Acosh);
MATH_UNARY_BLOCK(Atanh);
MATH_UNARY_BLOCK(Erf);
MATH_UNARY_BLOCK(Erfc);
MATH_UNARY_BLOCK(TGamma);
MATH_UNARY_BLOCK(LGamma);
MATH_UNARY_BLOCK(Ceil);
MATH_UNARY_BLOCK(Floor);
MATH_UNARY_BLOCK(Trunc);
MATH_UNARY_BLOCK(Round);

RUNTIME_BLOCK(Math, Mean);
RUNTIME_BLOCK_inputTypes(Mean);
RUNTIME_BLOCK_outputTypes(Mean);
RUNTIME_BLOCK_activate(Mean);
RUNTIME_BLOCK_END(Mean);
}; // namespace Math

void registerBlocksCoreBlocks() {
  REGISTER_CORE_BLOCK(Const);
  REGISTER_CORE_BLOCK(Input);
  REGISTER_CORE_BLOCK(Set);
  REGISTER_CORE_BLOCK(Update);
  REGISTER_CORE_BLOCK(Push);
  REGISTER_CORE_BLOCK(Pop);
  REGISTER_CORE_BLOCK(Clear);
  REGISTER_CORE_BLOCK(Count);
  REGISTER_CORE_BLOCK(Get);
  REGISTER_CORE_BLOCK(Swap);
  REGISTER_CORE_BLOCK(Sleep);
  REGISTER_CORE_BLOCK(Restart);
  REGISTER_CORE_BLOCK(Return);
  REGISTER_CORE_BLOCK(Stop);
  REGISTER_CORE_BLOCK(And);
  REGISTER_CORE_BLOCK(Or);
  REGISTER_CORE_BLOCK(Take);
  REGISTER_CORE_BLOCK(Limit);
  REGISTER_CORE_BLOCK(Repeat);
  REGISTER_CORE_BLOCK(Sort);
  REGISTER_CORE_BLOCK(Remove);
  REGISTER_CORE_BLOCK(Is);
  REGISTER_CORE_BLOCK(IsNot);
  REGISTER_CORE_BLOCK(IsMore);
  REGISTER_CORE_BLOCK(IsLess);
  REGISTER_CORE_BLOCK(IsMoreEqual);
  REGISTER_CORE_BLOCK(IsLessEqual);
  REGISTER_CORE_BLOCK(Any);
  REGISTER_CORE_BLOCK(All);
  REGISTER_CORE_BLOCK(AnyNot);
  REGISTER_CORE_BLOCK(AllNot);
  REGISTER_CORE_BLOCK(AnyMore);
  REGISTER_CORE_BLOCK(AllMore);
  REGISTER_CORE_BLOCK(AnyLess);
  REGISTER_CORE_BLOCK(AllLess);
  REGISTER_CORE_BLOCK(AnyMoreEqual);
  REGISTER_CORE_BLOCK(AllMoreEqual);
  REGISTER_CORE_BLOCK(AnyLessEqual);
  REGISTER_CORE_BLOCK(AllLessEqual);

  REGISTER_BLOCK(Math, Add);
  REGISTER_BLOCK(Math, Subtract);
  REGISTER_BLOCK(Math, Multiply);
  REGISTER_BLOCK(Math, Divide);
  REGISTER_BLOCK(Math, Xor);
  REGISTER_BLOCK(Math, And);
  REGISTER_BLOCK(Math, Or);
  REGISTER_BLOCK(Math, Mod);
  REGISTER_BLOCK(Math, LShift);
  REGISTER_BLOCK(Math, RShift);

  REGISTER_BLOCK(Math, Abs);
  REGISTER_BLOCK(Math, Exp);
  REGISTER_BLOCK(Math, Exp2);
  REGISTER_BLOCK(Math, Expm1);
  REGISTER_BLOCK(Math, Log);
  REGISTER_BLOCK(Math, Log10);
  REGISTER_BLOCK(Math, Log2);
  REGISTER_BLOCK(Math, Log1p);
  REGISTER_BLOCK(Math, Sqrt);
  REGISTER_BLOCK(Math, Cbrt);
  REGISTER_BLOCK(Math, Sin);
  REGISTER_BLOCK(Math, Cos);
  REGISTER_BLOCK(Math, Tan);
  REGISTER_BLOCK(Math, Asin);
  REGISTER_BLOCK(Math, Acos);
  REGISTER_BLOCK(Math, Atan);
  REGISTER_BLOCK(Math, Sinh);
  REGISTER_BLOCK(Math, Cosh);
  REGISTER_BLOCK(Math, Tanh);
  REGISTER_BLOCK(Math, Asinh);
  REGISTER_BLOCK(Math, Acosh);
  REGISTER_BLOCK(Math, Atanh);
  REGISTER_BLOCK(Math, Erf);
  REGISTER_BLOCK(Math, Erfc);
  REGISTER_BLOCK(Math, TGamma);
  REGISTER_BLOCK(Math, LGamma);
  REGISTER_BLOCK(Math, Ceil);
  REGISTER_BLOCK(Math, Floor);
  REGISTER_BLOCK(Math, Trunc);
  REGISTER_BLOCK(Math, Round);

  REGISTER_BLOCK(Math, Mean);
}
}; // namespace chainblocks