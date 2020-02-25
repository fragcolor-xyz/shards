/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#include "../runtime.hpp"
#include "utility.hpp"

namespace chainblocks {
struct JointOp {
  std::vector<CBVar *> _multiSortColumns;

  CBVar _inputVar{};
  CBVar *_input = nullptr;
  CBVar _columns{};

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnySeqType; }

  static inline ParamsInfo joinOpParams = ParamsInfo(
      ParamsInfo::Param("From",
                        "The name of the sequence variable to edit in place.",
                        CoreInfo::AnyVarSeqType),
      ParamsInfo::Param(

          "Join",
          "Other columns to join sort/filter using the input (they must be "
          "of the same length).",
          CoreInfo::AnyVarSeqType));

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      cloneVar(_inputVar, value);
      break;
    case 1:
      cloneVar(_columns, value);
      cleanup();
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _inputVar;
    case 1:
      return _columns;
    default:
      break;
    }
    throw CBException("Parameter out of range.");
  }

  void cleanup() {
    for (auto ref : _multiSortColumns) {
      releaseVariable(ref);
    }
    _multiSortColumns.clear();

    if (_input) {
      releaseVariable(_input);
      _input = nullptr;
    }
  }

  void ensureJoinSetup(CBContext *context) {
    if (!_input) {
      if (_inputVar.valueType != ContextVar)
        throw CBException("From sequence variable invalid.");

      _input = referenceVariable(context, _inputVar.payload.stringValue);

      if (_input->valueType != Seq)
        throw CBException("From sequence variable is not a Seq.");
    }

    if (_columns.valueType != None) {
      auto len = _input->payload.seqValue.len;
      if (_multiSortColumns.size() == 0) {
        if (_columns.valueType == Seq) {
          auto seq = IterableSeq(_columns.payload.seqValue);
          for (const auto &col : seq) {
            auto target = referenceVariable(context, col.payload.stringValue);
            if (target && target->valueType == Seq) {
              auto mseqLen = target->payload.seqValue.len;
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
          auto target =
              referenceVariable(context, _columns.payload.stringValue);
          if (target && target->valueType == Seq) {
            auto mseqLen = target->payload.seqValue.len;
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
          auto mseqLen = seq.len;
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
  BlocksVar _blks{};
  std::vector<CBVar> _multiSortKeys;
  bool _desc = false;

  void setup() { blocksKeyFn._bu = this; }

  void cleanup() {
    _blks.reset();
    JointOp::cleanup();
  }

  static inline ParamsInfo paramsInfo = ParamsInfo(
      joinOpParams,
      ParamsInfo::Param(
          "Desc",
          "If sorting should be in descending order, defaults ascending.",
          CoreInfo::BoolType),
      ParamsInfo::Param("Key",
                        "The blocks to use to transform the collection's items "
                        "before they are compared. Can be None.",
                        CoreInfo::BlocksOrNone));

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
    case 1:
      return JointOp::setParam(index, value);
    case 2:
      _desc = value.payload.boolValue;
      break;
    case 3:
      _blks = value;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
    case 1:
      return JointOp::getParam(index);
    case 2:
      return Var(_desc);
    case 3:
      return _blks;
    default:
      break;
    }
    throw CBException("Parameter out of range.");
  }

  CBTypeInfo compose(CBInstanceData &data) {
    if (_inputVar.valueType != ContextVar)
      throw CBException("From variable was empty!");

    IterableExposedInfo shared(data.shared);
    CBExposedTypeInfo info{};
    for (auto &reference : shared) {
      if (strcmp(reference.name, _inputVar.payload.stringValue) == 0) {
        info = reference;
        goto found;
      }
    }

    throw CBException("From variable not found!");

  found:
    // need to replace input type of inner chain with inner of seq
    if (info.exposedType.seqTypes.len != 1)
      throw CBException("From variable is not a single type Seq.");

    auto inputType = info.exposedType;
    data.inputType = info.exposedType.seqTypes.elements[0];
    _blks.validate(data);
    return inputType;
  }

  struct {
    bool operator()(CBVar &a, CBVar &b) const { return a > b; }
  } sortAsc;

  struct {
    bool operator()(CBVar &a, CBVar &b) const { return a < b; }
  } sortDesc;

  struct {
    CBVar &operator()(CBVar &a) { return a; }
  } noopKeyFn;

  struct {
    Sort *_bu;
    CBContext *_ctx;
    CBVar _o;

    CBVar &operator()(CBVar &a) {
      _o = _bu->_blks.activate(_ctx, a);
      return _o;
    }
  } blocksKeyFn;

  template <class Compare, class KeyFn>
  void insertSort(CBVar seq[], int n, Compare comp, KeyFn keyfn) {
    int i, j;
    CBVar key{};
    for (i = 1; i < n; i++) {
      key = seq[i];
      _multiSortKeys.clear();
      for (const auto &seqVar : _multiSortColumns) {
        const auto &col = seqVar->payload.seqValue;
        _multiSortKeys.push_back(col.elements[i]);
      }
      j = i - 1;
      // notice no &, we WANT to copy
      auto b = keyfn(key);
      while (j >= 0 && comp(keyfn(seq[j]), b)) {
        seq[j + 1] = seq[j];
        for (const auto &seqVar : _multiSortColumns) {
          const auto &col = seqVar->payload.seqValue;
          col.elements[j + 1] = col.elements[j];
        }
        j = j - 1;
      }
      seq[j + 1] = key;
      auto z = 0;
      for (const auto &seqVar : _multiSortColumns) {
        const auto &col = seqVar->payload.seqValue;
        col.elements[j + 1] = _multiSortKeys[z++];
      }
    }
  }

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    JointOp::ensureJoinSetup(context);
    // Sort in plac
    auto len = _input->payload.seqValue.len;
    if (_blks) {
      blocksKeyFn._ctx = context;
      if (!_desc) {
        insertSort(_input->payload.seqValue.elements, len, sortAsc,
                   blocksKeyFn);
      } else {
        insertSort(_input->payload.seqValue.elements, len, sortDesc,
                   blocksKeyFn);
      }
    } else {
      if (!_desc) {
        insertSort(_input->payload.seqValue.elements, len, sortAsc, noopKeyFn);
      } else {
        insertSort(_input->payload.seqValue.elements, len, sortDesc, noopKeyFn);
      }
    }
    return *_input;
  }
};

struct Remove : public JointOp {
  BlocksVar _blks{};
  bool _fast = false;

  void cleanup() {
    _blks.reset();
    JointOp::cleanup();
  }

  static inline ParamsInfo paramsInfo = ParamsInfo(
      joinOpParams,
      ParamsInfo::Param("Predicate",
                        "The blocks to use as predicate, if true the item will "
                        "be popped from the sequence.",
                        CoreInfo::Blocks),
      ParamsInfo::Param("Unordered",
                        "Turn on to remove items very quickly but will not "
                        "preserve the sequence items order.",
                        CoreInfo::BoolType));

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
    case 1:
      return JointOp::setParam(index, value);
    case 2:
      _blks = value;
      break;
    case 3:
      _fast = value.payload.boolValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
    case 1:
      return JointOp::getParam(index);
    case 2:
      return _blks;
    case 3:
      return Var(_fast);
    default:
      break;
    }
    throw CBException("Parameter out of range.");
  }

  CBTypeInfo compose(CBInstanceData &data) {
    if (_inputVar.valueType != ContextVar)
      throw CBException("From variable was empty!");

    IterableExposedInfo shared(data.shared);
    CBExposedTypeInfo info{};
    for (auto &reference : shared) {
      if (strcmp(reference.name, _inputVar.payload.stringValue) == 0) {
        info = reference;
        goto found;
      }
    }

    throw CBException("From variable not found!");

  found:
    // need to replace input type of inner chain with inner of seq
    if (info.exposedType.seqTypes.len != 1)
      throw CBException("From variable is not a single type Seq.");

    auto inputType = info.exposedType;
    data.inputType = info.exposedType.seqTypes.elements[0];
    _blks.validate(data);
    return inputType;
  }

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    JointOp::ensureJoinSetup(context);
    // Remove in place, will possibly remove any sorting!
    auto len = _input->payload.seqValue.len;
    for (auto i = len; i > 0; i--) {
      auto &var = _input->payload.seqValue.elements[i - 1];
      if (_blks.activate(context, var) == True) {
        // remove from input
        if (var.valueType >= EndOfBlittableTypes) {
          destroyVar(var);
        }
        // this is acceptable cos del ops don't call free! or grow
        if (_fast)
          chainblocks::arrayDelFast(_input->payload.seqValue, i - 1);
        else
          chainblocks::arrayDel(_input->payload.seqValue, i - 1);
        // remove from joined
        for (const auto &seqVar : _multiSortColumns) {
          auto &seq = seqVar->payload.seqValue;
          if (seq.elements ==
              _input->payload.seqValue
                  .elements) // avoid removing from same seq as input!
            continue;

          auto &jvar = seq.elements[i - 1];
          if (var.valueType >= EndOfBlittableTypes) {
            destroyVar(jvar);
          }
          if (_fast)
            chainblocks::arrayDelFast(seq, i - 1);
          else
            chainblocks::arrayDel(seq, i - 1);
        }
      }
    }
    return *_input;
  }
};

struct Profile {
  BlocksVar _blocks{};
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static inline ParamsInfo paramsInfo = ParamsInfo(
      ParamsInfo::Param("Action", "The action to profile.", CoreInfo::Blocks));

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  void cleanup() { _blocks.reset(); }

  CBTypeInfo compose(const CBInstanceData &data) {
    _blocks.validate(data);
    return data.inputType;
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _blocks = value;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _blocks;
    default:
      break;
    }
    throw CBException("Parameter out of range.");
  }

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    TIMED_FUNC(timerObj);
    CBVar output{};
    if (unlikely(!activateBlocks(CBVar(_blocks).payload.seqValue, context,
                                 input, output))) {
      return StopChain;
    }
    return input;
  }
};

struct XPendBase {
  static inline Types xpendTypes{
      {CoreInfo::AnyVarSeqType, CoreInfo::StringVarType}};
};

struct XpendTo : public XPendBase {
  ThreadShared<std::string> _scratchStr;

  ParamVar _collection{};

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  // TODO use xpendTypes...
  static inline ParamsInfo paramsInfo = ParamsInfo(ParamsInfo::Param(
      "Collection", "The collection to add the input to.", xpendTypes));

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  CBTypeInfo compose(const CBInstanceData &data) {
    auto conss = IterableExposedInfo(data.shared);
    for (auto &cons : conss) {
      if (strcmp(cons.name, _collection.variableName()) == 0) {
        if (cons.exposedType.basicType != CBType::Seq &&
            cons.exposedType.basicType != CBType::Bytes &&
            cons.exposedType.basicType != CBType::String) {
          throw CBException("AppendTo/PrependTo expects either a Seq, String "
                            "or Bytes variable as collection.");
        }
        if (!cons.isMutable) {
          throw CBException(
              "AppendTo/PrependTo expects a mutable variable (Set/Push).");
        }
        if (cons.exposedType.basicType == Seq &&
            (cons.exposedType.seqTypes.len != 1 ||
             cons.exposedType.seqTypes.elements[0] != data.inputType)) {
          throw CBException("AppendTo/PrependTo input type is not compatible "
                            "with the backing Seq.");
        }
        // Validation Ok if here..
        return data.inputType;
      }
    }
    throw CBException("AppendTo/PrependTo: Failed to find variable: " +
                      std::string(_collection.variableName()));
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _collection = value;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _collection;
    default:
      break;
    }
    throw CBException("Parameter out of range.");
  }

  void cleanup() { _collection.cleanup(); }
  void warmup(CBContext *context) { _collection.warmup(context); }
};

struct AppendTo : public XpendTo {
  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    auto &collection = _collection(context);
    switch (collection.valueType) {
    case Seq: {
      CBVar tmp{};
      cloneVar(tmp, input);
      chainblocks::arrayPush(collection.payload.seqValue, tmp);
      break;
    }
    case String: {
      // variable is mutable, so we are sure we manage the memory
      // specifically in Set, cloneVar is used, which uses `new` to allocate
      // all we have to do use to clone our scratch on top of it
      _scratchStr().clear();
      _scratchStr() += collection.payload.stringValue;
      _scratchStr() += input.payload.stringValue;
      Var tmp(_scratchStr());
      cloneVar(collection, tmp);
      break;
    }
    default:
      break;
    }
    return input;
  }
};

struct PrependTo : public XpendTo {
  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    auto &collection = _collection(context);
    switch (collection.valueType) {
    case Seq: {
      CBVar tmp{};
      cloneVar(tmp, input);
      chainblocks::arrayInsert(collection.payload.seqValue, 0, tmp);
      break;
    }
    case String: {
      // variable is mutable, so we are sure we manage the memory
      // specifically in Set, cloneVar is used, which uses `new` to allocate
      // all we have to do use to clone our scratch on top of it
      _scratchStr().clear();
      _scratchStr() += input.payload.stringValue;
      _scratchStr() += collection.payload.stringValue;
      Var tmp(_scratchStr());
      cloneVar(collection, tmp);
      break;
    }
    default:
      break;
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
RUNTIME_BLOCK_compose(Const);
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

// Register SetInput
RUNTIME_CORE_BLOCK_FACTORY(SetInput);
RUNTIME_BLOCK_inputTypes(SetInput);
RUNTIME_BLOCK_outputTypes(SetInput);
RUNTIME_BLOCK_activate(SetInput);
RUNTIME_BLOCK_END(SetInput);

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

// Register Not
RUNTIME_CORE_BLOCK_FACTORY(Not);
RUNTIME_BLOCK_inputTypes(Not);
RUNTIME_BLOCK_outputTypes(Not);
RUNTIME_BLOCK_activate(Not);
RUNTIME_BLOCK_END(Not);

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

// Register IsNan
RUNTIME_CORE_BLOCK_FACTORY(IsValidNumber);
RUNTIME_BLOCK_inputTypes(IsValidNumber);
RUNTIME_BLOCK_outputTypes(IsValidNumber);
RUNTIME_BLOCK_activate(IsValidNumber);
RUNTIME_BLOCK_END(IsValidNumber);

// Register Set
RUNTIME_CORE_BLOCK_FACTORY(Set);
RUNTIME_BLOCK_cleanup(Set);
RUNTIME_BLOCK_inputTypes(Set);
RUNTIME_BLOCK_outputTypes(Set);
RUNTIME_BLOCK_parameters(Set);
RUNTIME_BLOCK_compose(Set);
RUNTIME_BLOCK_exposedVariables(Set);
RUNTIME_BLOCK_setParam(Set);
RUNTIME_BLOCK_getParam(Set);
RUNTIME_BLOCK_activate(Set);
RUNTIME_BLOCK_END(Set);

// Register Ref
RUNTIME_CORE_BLOCK_FACTORY(Ref);
RUNTIME_BLOCK_cleanup(Ref);
RUNTIME_BLOCK_inputTypes(Ref);
RUNTIME_BLOCK_outputTypes(Ref);
RUNTIME_BLOCK_parameters(Ref);
RUNTIME_BLOCK_compose(Ref);
RUNTIME_BLOCK_exposedVariables(Ref);
RUNTIME_BLOCK_setParam(Ref);
RUNTIME_BLOCK_getParam(Ref);
RUNTIME_BLOCK_activate(Ref);
RUNTIME_BLOCK_END(Ref);

// Register Update
RUNTIME_CORE_BLOCK_FACTORY(Update);
RUNTIME_BLOCK_cleanup(Update);
RUNTIME_BLOCK_inputTypes(Update);
RUNTIME_BLOCK_outputTypes(Update);
RUNTIME_BLOCK_parameters(Update);
RUNTIME_BLOCK_compose(Update);
RUNTIME_BLOCK_requiredVariables(Update);
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
RUNTIME_BLOCK_compose(Push);
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
RUNTIME_BLOCK_compose(Pop);
RUNTIME_BLOCK_requiredVariables(Pop);
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

// Register Drop
RUNTIME_CORE_BLOCK_FACTORY(Drop);
RUNTIME_BLOCK_cleanup(Drop);
RUNTIME_BLOCK_inputTypes(Drop);
RUNTIME_BLOCK_outputTypes(Drop);
RUNTIME_BLOCK_parameters(Drop);
RUNTIME_BLOCK_setParam(Drop);
RUNTIME_BLOCK_getParam(Drop);
RUNTIME_BLOCK_activate(Drop);
RUNTIME_BLOCK_END(Drop);

// Register Get
RUNTIME_CORE_BLOCK_FACTORY(Get);
RUNTIME_BLOCK_cleanup(Get);
RUNTIME_BLOCK_destroy(Get);
RUNTIME_BLOCK_inputTypes(Get);
RUNTIME_BLOCK_outputTypes(Get);
RUNTIME_BLOCK_parameters(Get);
RUNTIME_BLOCK_compose(Get);
RUNTIME_BLOCK_requiredVariables(Get);
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
RUNTIME_BLOCK_requiredVariables(Swap);
RUNTIME_BLOCK_setParam(Swap);
RUNTIME_BLOCK_getParam(Swap);
RUNTIME_BLOCK_activate(Swap);
RUNTIME_BLOCK_END(Swap);

// Register Take
RUNTIME_CORE_BLOCK_FACTORY(Take);
RUNTIME_BLOCK_destroy(Take);
RUNTIME_BLOCK_cleanup(Take);
RUNTIME_BLOCK_requiredVariables(Take);
RUNTIME_BLOCK_inputTypes(Take);
RUNTIME_BLOCK_outputTypes(Take);
RUNTIME_BLOCK_parameters(Take);
RUNTIME_BLOCK_compose(Take);
RUNTIME_BLOCK_setParam(Take);
RUNTIME_BLOCK_getParam(Take);
RUNTIME_BLOCK_activate(Take);
RUNTIME_BLOCK_END(Take);

// Register Slice
RUNTIME_CORE_BLOCK_FACTORY(Slice);
RUNTIME_BLOCK_destroy(Slice);
RUNTIME_BLOCK_cleanup(Slice);
RUNTIME_BLOCK_requiredVariables(Slice);
RUNTIME_BLOCK_inputTypes(Slice);
RUNTIME_BLOCK_outputTypes(Slice);
RUNTIME_BLOCK_parameters(Slice);
RUNTIME_BLOCK_compose(Slice);
RUNTIME_BLOCK_setParam(Slice);
RUNTIME_BLOCK_getParam(Slice);
RUNTIME_BLOCK_activate(Slice);
RUNTIME_BLOCK_END(Slice);

// Register Limit
RUNTIME_CORE_BLOCK_FACTORY(Limit);
RUNTIME_BLOCK_destroy(Limit);
RUNTIME_BLOCK_inputTypes(Limit);
RUNTIME_BLOCK_outputTypes(Limit);
RUNTIME_BLOCK_parameters(Limit);
RUNTIME_BLOCK_compose(Limit);
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
RUNTIME_BLOCK_cleanup(Repeat);
RUNTIME_BLOCK_exposedVariables(Repeat);
RUNTIME_BLOCK_requiredVariables(Repeat);
RUNTIME_BLOCK_compose(Repeat);
RUNTIME_BLOCK_END(Repeat);

// Register Sort
RUNTIME_CORE_BLOCK(Sort);
RUNTIME_BLOCK_setup(Sort);
RUNTIME_BLOCK_inputTypes(Sort);
RUNTIME_BLOCK_outputTypes(Sort);
RUNTIME_BLOCK_compose(Sort);
RUNTIME_BLOCK_activate(Sort);
RUNTIME_BLOCK_parameters(Sort);
RUNTIME_BLOCK_setParam(Sort);
RUNTIME_BLOCK_getParam(Sort);
RUNTIME_BLOCK_cleanup(Sort);
RUNTIME_BLOCK_END(Sort);

// Register
RUNTIME_CORE_BLOCK(Remove);
RUNTIME_BLOCK_inputTypes(Remove);
RUNTIME_BLOCK_outputTypes(Remove);
RUNTIME_BLOCK_parameters(Remove);
RUNTIME_BLOCK_setParam(Remove);
RUNTIME_BLOCK_getParam(Remove);
RUNTIME_BLOCK_activate(Remove);
RUNTIME_BLOCK_cleanup(Remove);
RUNTIME_BLOCK_compose(Remove);
RUNTIME_BLOCK_END(Remove);

// Register
RUNTIME_CORE_BLOCK(Profile);
RUNTIME_BLOCK_inputTypes(Profile);
RUNTIME_BLOCK_outputTypes(Profile);
RUNTIME_BLOCK_parameters(Profile);
RUNTIME_BLOCK_setParam(Profile);
RUNTIME_BLOCK_getParam(Profile);
RUNTIME_BLOCK_activate(Profile);
RUNTIME_BLOCK_cleanup(Profile);
RUNTIME_BLOCK_compose(Profile);
RUNTIME_BLOCK_END(Profile);

// Register PrependTo
RUNTIME_CORE_BLOCK(PrependTo);
RUNTIME_BLOCK_cleanup(PrependTo);
RUNTIME_BLOCK_warmup(PrependTo);
RUNTIME_BLOCK_inputTypes(PrependTo);
RUNTIME_BLOCK_outputTypes(PrependTo);
RUNTIME_BLOCK_parameters(PrependTo);
RUNTIME_BLOCK_compose(PrependTo);
RUNTIME_BLOCK_setParam(PrependTo);
RUNTIME_BLOCK_getParam(PrependTo);
RUNTIME_BLOCK_activate(PrependTo);
RUNTIME_BLOCK_END(PrependTo);

// Register AppendTo
RUNTIME_CORE_BLOCK(AppendTo);
RUNTIME_BLOCK_cleanup(AppendTo);
RUNTIME_BLOCK_warmup(AppendTo);
RUNTIME_BLOCK_inputTypes(AppendTo);
RUNTIME_BLOCK_outputTypes(AppendTo);
RUNTIME_BLOCK_parameters(AppendTo);
RUNTIME_BLOCK_compose(AppendTo);
RUNTIME_BLOCK_setParam(AppendTo);
RUNTIME_BLOCK_getParam(AppendTo);
RUNTIME_BLOCK_activate(AppendTo);
RUNTIME_BLOCK_END(AppendTo);

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

MATH_BINARY_BLOCK(Inc);
MATH_BINARY_BLOCK(Dec);
}; // namespace Math

void registerBlocksCoreBlocks() {
  REGISTER_CORE_BLOCK(Const);
  REGISTER_CORE_BLOCK(Input);
  REGISTER_CORE_BLOCK(SetInput);
  REGISTER_CORE_BLOCK(Set);
  REGISTER_CORE_BLOCK(Ref);
  REGISTER_CORE_BLOCK(Update);
  REGISTER_CORE_BLOCK(Push);
  REGISTER_CORE_BLOCK(Pop);
  REGISTER_CORE_BLOCK(Clear);
  REGISTER_CORE_BLOCK(Drop);
  REGISTER_CORE_BLOCK(Count);
  REGISTER_CORE_BLOCK(Get);
  REGISTER_CORE_BLOCK(Swap);
  REGISTER_CORE_BLOCK(Sleep);
  REGISTER_CORE_BLOCK(Restart);
  REGISTER_CORE_BLOCK(Return);
  REGISTER_CORE_BLOCK(Stop);
  REGISTER_CORE_BLOCK(And);
  REGISTER_CORE_BLOCK(Or);
  REGISTER_CORE_BLOCK(Not);
  REGISTER_CORE_BLOCK(IsValidNumber);
  REGISTER_CORE_BLOCK(Take);
  REGISTER_CORE_BLOCK(Slice);
  REGISTER_CORE_BLOCK(Limit);
  REGISTER_CORE_BLOCK(Repeat);
  REGISTER_CORE_BLOCK(Sort);
  REGISTER_CORE_BLOCK(Remove);
  REGISTER_CORE_BLOCK(Profile);
  REGISTER_CORE_BLOCK(PrependTo);
  REGISTER_CORE_BLOCK(AppendTo);
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

  REGISTER_BLOCK(Math, Inc);
  REGISTER_BLOCK(Math, Dec);
}
}; // namespace chainblocks
