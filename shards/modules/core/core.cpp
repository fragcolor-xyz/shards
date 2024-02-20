/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include <shards/core/runtime.hpp>
#include <shards/core/module.hpp>
#include <shards/utility.hpp>
#include "core.hpp"
#include "pdqsort.h"
#include <boost/algorithm/string.hpp>
#include <chrono>

namespace shards {

struct JointOp {
  std::vector<SHVar *> _multiSortColumns;

  SHVar _inputVar{};
  SHVar *_input = nullptr;
  SHVar _columns{};

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnySeqType; }

  static inline Type anyVarSeq = Type::VariableOf(CoreInfo::AnySeqType);
  static inline Type anyVarSeqSeq = Type::SeqOf(anyVarSeq);

  static inline Parameters joinOpParams{{"From", SHCCSTR("The name of the sequence variable to edit in place."), {anyVarSeq}},
                                        {"Join",
                                         SHCCSTR("Other columns to join sort/filter using the input (they "
                                                 "must be of the same length)."),
                                         {anyVarSeq, anyVarSeqSeq}}};

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      cloneVar(_inputVar, value);
      break;
    case 1:
      cloneVar(_columns, value);
      cleanup(nullptr);
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _inputVar;
    case 1:
      return _columns;
    default:
      break;
    }
    throw SHException("Parameter out of range.");
  }

  void cleanup(SHContext *context) {
    for (auto ref : _multiSortColumns) {
      releaseVariable(ref);
    }
    _multiSortColumns.clear();

    if (_input) {
      releaseVariable(_input);
      _input = nullptr;
    }
  }

  void warmup(SHContext *context) {
    if (!_input) {
      _input = referenceVariable(context, SHSTRVIEW(_inputVar));
    }
  }

  void ensureJoinSetup(SHContext *context) {
    if (_columns.valueType != SHType::None) {
      auto len = _input->payload.seqValue.len;
      if (_multiSortColumns.size() == 0) {
        if (_columns.valueType == SHType::Seq) {
          for (const auto &col : _columns) {
            auto target = referenceVariable(context, SHSTRVIEW(col));
            if (target && target->valueType == SHType::Seq) {
              auto mseqLen = target->payload.seqValue.len;
              if (len != mseqLen) {
                throw ActivationError("JointOp: All the sequences to be processed must have "
                                      "the same length as the input sequence.");
              }
              _multiSortColumns.push_back(target);
            }
          }
        } else if (_columns.valueType == SHType::ContextVar) { // normal single context var
          auto target = referenceVariable(context, SHSTRVIEW(_columns));
          if (target && target->valueType == SHType::Seq) {
            auto mseqLen = target->payload.seqValue.len;
            if (len != mseqLen) {
              throw ActivationError("JointOp: All the sequences to be processed must have "
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
            throw ActivationError("JointOp: All the sequences to be processed must have "
                                  "the same length as the input sequence.");
          }
        }
      }
    }
  }
};

struct ActionJointOp : public JointOp {
  ShardsVar _blks{};
  void cleanup(SHContext *context) {
    _blks.cleanup(context);
    JointOp::cleanup(context);
  }

  void warmup(SHContext *ctx) {
    JointOp::warmup(ctx);
    _blks.warmup(ctx);
  }
};

struct Sort : public ActionJointOp {
  std::vector<SHVar> _multiSortKeys;
  bool _desc = false;

  void setup() { shardsKeyFn._bu = this; }

  static SHOptionalString help() {
    return SHCCSTR("Sorts the elements of a sequence. Can also move around the elements of a joined sequence in alignment with "
                   "the sorted sequence.");
  }

  static SHOptionalString inputHelp() { return SHCCSTR("Any input is ignored."); }

  static SHOptionalString outputHelp() { return SHCCSTR("Output is the sorted sequence."); }

  static inline Parameters paramsInfo{
      joinOpParams,
      {{"Desc", SHCCSTR("If sorting should be in descending order, defaults ascending."), {CoreInfo::BoolType}},
       {"Key",
        SHCCSTR("The shards to use to transform the collection's items "
                "before they are compared. Can be None."),
        {CoreInfo::ShardsOrNone}}}};

  static SHParametersInfo parameters() { return paramsInfo; }

  void setParam(int index, const SHVar &value) {
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

  SHVar getParam(int index) {
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
    throw SHException("Parameter out of range.");
  }

  SHTypeInfo compose(SHInstanceData &data) {
    if (_inputVar.valueType != SHType::ContextVar)
      throw SHException("From variable was empty!");

    SHExposedTypeInfo info{};
    for (auto &reference : data.shared) {
      std::string_view name(reference.name);
      if (name == SHSTRVIEW(_inputVar)) {
        info = reference;
        goto found;
      }
    }

    throw SHException("From variable not found!");

  found:
    // need to replace input type of inner wire with inner of seq
    if (info.exposedType.seqTypes.len != 1)
      throw SHException(fmt::format("From variable \"{}\" is not a single type SHType::Seq ({}).", SHSTRVIEW(_inputVar), info.exposedType.seqTypes));

    auto inputType = info.exposedType;
    data.inputType = info.exposedType.seqTypes.elements[0];
    _blks.compose(data);
    return inputType;
  }

  struct {
    bool operator()(const SHVar &a, const SHVar &b) const { return a > b; }
  } sortAsc;

  struct {
    bool operator()(const SHVar &a, const SHVar &b) const { return a < b; }
  } sortDesc;

  struct {
    const SHVar &operator()(const SHVar &a) { return a; }
  } noopKeyFn;

  struct {
    Sort *_bu;
    SHContext *_ctx;
    SHVar _o;

    const SHVar &operator()(const SHVar &a) {
      _bu->_blks.activate(_ctx, a, _o);
      return _o;
    }
  } shardsKeyFn;

  template <class Compare, class KeyFn> void insertSort(SHVar seq[], int64_t n, Compare comp, KeyFn keyfn) {
    int64_t i, j;
    SHVar key{};
    for (i = 1; i < n; i++) {
      // main
      key = seq[i];
      // joined seqs
      _multiSortKeys.clear();
      for (const auto &seqVar : _multiSortColumns) {
        const auto &col = seqVar->payload.seqValue;
        if (col.len != n) {
          throw ActivationError("Sort: All the sequences to be processed must have "
                                "the same length as the input sequence.");
        }
        _multiSortKeys.push_back(col.elements[i]);
      }
      j = i - 1;
      // notice no &, we WANT to copy
      auto b = keyfn(key);
      while (j >= 0 && comp(keyfn(seq[j]), b)) {
        // main
        seq[j + 1] = seq[j];
        // joined seqs
        for (const auto &seqVar : _multiSortColumns) {
          const auto &col = seqVar->payload.seqValue;
          col.elements[j + 1] = col.elements[j];
        }
        // main + join
        j = j - 1;
      }
      // main
      seq[j + 1] = key;
      // joined seq
      auto z = 0;
      for (const auto &seqVar : _multiSortColumns) {
        const auto &col = seqVar->payload.seqValue;
        col.elements[j + 1] = _multiSortKeys[z++];
      }
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    JointOp::ensureJoinSetup(context);
    // Sort in plac
    int64_t len = int64_t(_input->payload.seqValue.len);
    if (_blks) {
      shardsKeyFn._ctx = context;
      if (!_desc) {
        insertSort(_input->payload.seqValue.elements, len, sortAsc, shardsKeyFn);
      } else {
        insertSort(_input->payload.seqValue.elements, len, sortDesc, shardsKeyFn);
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

struct Remove : public ActionJointOp {
  bool _fast = false;

  static SHOptionalString help() {
    return SHCCSTR("Removes all elements from a sequence that match the given condition. Can also take these matched indices and "
                   "remove corresponding elements from a joined sequence.");
  }

  static SHOptionalString inputHelp() { return SHCCSTR("Any input is ignored."); }

  static SHOptionalString outputHelp() { return SHCCSTR("Output is the filtered sequence."); }

  static inline Parameters paramsInfo{joinOpParams,
                                      {{"Predicate",
                                        SHCCSTR("The shards to use as predicate, if true the "
                                                "item will be popped from the sequence."),
                                        {CoreInfo::Shards}},
                                       {"Unordered",
                                        SHCCSTR("Turn on to remove items very quickly but will "
                                                "not preserve the sequence items order."),
                                        {CoreInfo::BoolType}}}};

  static SHParametersInfo parameters() { return paramsInfo; }

  void setParam(int index, const SHVar &value) {
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

  SHVar getParam(int index) {
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
    throw SHException("Parameter out of range.");
  }

  SHTypeInfo compose(SHInstanceData &data) {
    if (_inputVar.valueType != SHType::ContextVar)
      throw SHException("From variable was empty!");

    SHExposedTypeInfo info{};
    for (auto &reference : data.shared) {
      std::string_view name(reference.name);
      if (name == SHSTRVIEW(_inputVar)) {
        info = reference;
        goto found;
      }
    }

    throw SHException("From variable not found!");

  found:
    // need to replace input type of inner wire with inner of seq
    if (info.exposedType.seqTypes.len != 1)
      throw SHException(fmt::format("From variable \"{}\" is not a single type SHType::Seq ({}).", SHSTRVIEW(_inputVar), info.exposedType.seqTypes));

    auto inputType = info.exposedType;
    data.inputType = info.exposedType.seqTypes.elements[0];
    const auto pres = _blks.compose(data);
    if (pres.outputType.basicType != SHType::Bool) {
      throw ComposeError("Remove Predicate should output a boolean value.");
    }
    return inputType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    JointOp::ensureJoinSetup(context);
    // Remove in place, will possibly remove any sorting!
    SHVar output{};
    const uint32_t len = _input->payload.seqValue.len;
    for (uint32_t i = len; i > 0; i--) {
      const auto &var = _input->payload.seqValue.elements[i - 1];
      // conditional flow so we might have "returns" form (And) (Or)
      if (unlikely(_blks.activate<true>(context, var, output) > SHWireState::Return))
        return *_input;

      if (output == Var::True) {
        // this is acceptable cos del ops don't call free or grow
        if (_fast)
          arrayDelFast(_input->payload.seqValue, i - 1);
        else
          arrayDel(_input->payload.seqValue, i - 1);
        // remove from joined
        for (const auto &seqVar : _multiSortColumns) {
          auto &seq = seqVar->payload.seqValue;
          if (seq.elements == _input->payload.seqValue.elements) // avoid removing from same seq as input!
            continue;

          if (seq.len >= i) {
            if (_fast)
              arrayDelFast(seq, i - 1);
            else
              arrayDel(seq, i - 1);
          }
        }
      }
    }
    return *_input;
  }
};

struct Profile {
  ShardsVar _shards{};
  SHExposedTypesInfo _exposed{};
  std::string _label{"<no label>"};

  static inline Parameters _params{{"Action", SHCCSTR("The action shards to profile."), {CoreInfo::Shards}},
                                   {"Label", SHCCSTR("The label to print when outputting time data."), {CoreInfo::StringType}}};

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static SHParametersInfo parameters() { return _params; }

  void cleanup(SHContext *context) { _shards.cleanup(context); }

  void warmup(SHContext *ctx) { _shards.warmup(ctx); }

  SHTypeInfo compose(const SHInstanceData &data) {
    auto res = _shards.compose(data);
    _exposed = res.exposedInfo;
    return res.outputType;
  }

  SHExposedTypesInfo exposedVariables() { return _exposed; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _shards = value;
      break;
    case 1:
      _label = SHSTRVIEW(value);
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _shards;
    case 1:
      return Var(_label);
    default:
      break;
    }
    throw SHException("Parameter out of range.");
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    SHVar output{};
    const auto start = std::chrono::high_resolution_clock::now();
    activateShards(SHVar(_shards).payload.seqValue, context, input, output);
    const auto stop = std::chrono::high_resolution_clock::now();
    const auto dur = std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count();
    SHLOG_INFO("{} took {} microseconds.", dur);
    return output;
  }
};

struct XPendBase {
  static inline Types xpendTypes{{CoreInfo::AnyVarSeqType, CoreInfo::StringVarType, CoreInfo::BytesVarType}};
};

struct XpendTo : public XPendBase {
  static inline thread_local std::string _scratchStr;

  ParamVar _collection{};

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static inline ParamsInfo paramsInfo =
      ParamsInfo(ParamsInfo::Param("Collection", SHCCSTR("The collection to add the input to."), xpendTypes));

  static SHParametersInfo parameters() { return SHParametersInfo(paramsInfo); }

  SHTypeInfo compose(const SHInstanceData &data) {
    for (auto &cons : data.shared) {
      if (strcmp(cons.name, _collection.variableName()) == 0) {
        if (cons.exposedType.basicType != SHType::Seq && cons.exposedType.basicType != SHType::Bytes &&
            cons.exposedType.basicType != SHType::String) {
          throw ComposeError("AppendTo/PrependTo expects either a SHType::Seq, SHType::String "
                             "or SHType::Bytes variable as collection.");
        } else {
          if (cons.exposedType.basicType != SHType::Seq && cons.exposedType != data.inputType) {
            SHLOG_ERROR("AppendTo/PrependTo input is: {} variable is: {}", data.inputType, cons.exposedType);
            throw ComposeError("Input type not matching the variable.");
          }
        }
        if (!cons.isMutable) {
          throw ComposeError("AppendTo/PrependTo expects a mutable variable (Set/Push).");
        }
        if (cons.exposedType.basicType == SHType::Seq &&
            (cons.exposedType.seqTypes.len != 1 || cons.exposedType.seqTypes.elements[0] != data.inputType)) {
          throw ComposeError("AppendTo/PrependTo input type is not compatible "
                             "with the backing SHType::Seq.");
        }
        // Validation Ok if here..
        return data.inputType;
      }
    }
    throw ComposeError("AppendTo/PrependTo: Failed to find variable: " + std::string(_collection.variableName()));
  }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _collection = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _collection;
    default:
      break;
    }
    throw SHException("Parameter out of range.");
  }

  void cleanup(SHContext *context) { _collection.cleanup(); }
  void warmup(SHContext *context) { _collection.warmup(context); }
};

struct AppendTo : public XpendTo {
  static SHOptionalString help() { return SHCCSTR("Appends the input to the context variable passed to `:Collection`."); }
  static SHOptionalString inputHelp() { return SHCCSTR("The value to append to the collection."); }
  static SHOptionalString outputHelp() { return SHCCSTR("The input to this shard is passed through as its output."); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto &collection = _collection.get();
    switch (collection.valueType) {
    case SHType::Seq: {
      auto &arr = collection.payload.seqValue;
      const auto len = arr.len;
      shards::arrayResize(arr, len + 1);
      cloneVar(arr.elements[len], input);
      break;
    }
    case SHType::String: {
      // variable is mutable, so we are sure we manage the memory
      // specifically in Set, cloneVar is used, which uses `new` to allocate
      // all we have to do use to clone our scratch on top of it
      _scratchStr.clear();
      _scratchStr.append(collection.payload.stringValue, SHSTRLEN(collection));
      _scratchStr.append(input.payload.stringValue, SHSTRLEN(input));
      Var tmp(_scratchStr);
      cloneVar(collection, tmp);
      break;
    }
    case SHType::Bytes: {
      // we know it's a mutable variable so must be compatible with our
      // arrayPush and such routines just do like string for now basically
      _scratchStr.clear();
      _scratchStr.append((char *)collection.payload.bytesValue, collection.payload.bytesSize);
      _scratchStr.append((char *)input.payload.bytesValue, input.payload.bytesSize);
      Var tmp((uint8_t *)_scratchStr.data(), _scratchStr.size());
      cloneVar(collection, tmp);
    } break;
    default:
      throw ActivationError(fmt::format("AppendTo, case not implemented for type {}", collection.valueType));
    }
    return input;
  }
};

struct PrependTo : public XpendTo {
  static SHOptionalString help() { return SHCCSTR("Prepends the input to the context variable passed to `Collection`."); }
  static SHOptionalString inputHelp() { return SHCCSTR("The value to prepend to the collection."); }
  static SHOptionalString outputHelp() { return SHCCSTR("The input to this shard is passed through as its output."); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto &collection = _collection.get();
    switch (collection.valueType) {
    case SHType::Seq: {
      auto &arr = collection.payload.seqValue;
      const auto len = arr.len;
      shards::arrayResize(arr, len + 1);
      memmove(&arr.elements[1], &arr.elements[0], sizeof(SHVar) * len);
      arr.elements[0] = Var::Empty; // this shifted so we can safely reset it, and we need to otherwise clone would free [1]
      cloneVar(arr.elements[0], input);
      break;
    }
    case SHType::String: {
      // variable is mutable, so we are sure we manage the memory
      // specifically in Set, cloneVar is used, which uses `new` to allocate
      // all we have to do use to clone our scratch on top of it
      _scratchStr.clear();
      _scratchStr.append(input.payload.stringValue, SHSTRLEN(input));
      _scratchStr.append(collection.payload.stringValue, SHSTRLEN(collection));
      Var tmp(_scratchStr);
      cloneVar(collection, tmp);
      break;
    }
    case SHType::Bytes: {
      // we know it's a mutable variable so must be compatible with our
      // arrayPush and such routines just do like string for now basically
      _scratchStr.clear();
      _scratchStr.append((char *)input.payload.bytesValue, input.payload.bytesSize);
      _scratchStr.append((char *)collection.payload.bytesValue, collection.payload.bytesSize);
      Var tmp((uint8_t *)_scratchStr.data(), _scratchStr.size());
      cloneVar(collection, tmp);
    } break;
    default:
      throw ActivationError("PrependTo, case not implemented");
    }
    return input;
  }
};

struct ForEachShard {
  static inline Types _types{{CoreInfo::AnySeqType, CoreInfo::AnyTableType}};

  static SHOptionalString help() {
    return SHCCSTR("Processes every element or key-value pair of a sequence/table with a given shard or sequence of shards.");
  }

  static SHTypesInfo inputTypes() { return _types; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Sequence/table whose elements or key-value pairs need to be processed.");
  }

  static SHTypesInfo outputTypes() { return _types; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("The output from processing the sequence/table elements or key-value pairs.");
  }

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) { _shards = value; }

  SHVar getParam(int index) { return _shards; }

  SHTypeInfo compose(const SHInstanceData &data) {
    if (data.inputType.basicType != SHType::Seq && data.inputType.basicType != SHType::Table) {
      throw ComposeError("ForEach shard expected a sequence or a table as input.");
    }

    auto dataCopy = data;
    if (data.inputType.basicType == SHType::Seq && data.inputType.seqTypes.len == 1) {
      dataCopy.inputType = data.inputType.seqTypes.elements[0];
    } else if (data.inputType.basicType == SHType::Table) {
      dataCopy.inputType = CoreInfo::AnySeqType;
    } else {
      dataCopy.inputType = CoreInfo::AnyType;
    }

    _shards.compose(dataCopy);

    if (data.inputType.basicType == SHType::Table) {
      OVERRIDE_ACTIVATE(data, activateTable);
    } else {
      OVERRIDE_ACTIVATE(data, activateSeq);
    }

    return data.inputType;
  }

  void warmup(SHContext *ctx) { _shards.warmup(ctx); }

  void cleanup(SHContext *context) { _shards.cleanup(context); }

  SHVar activateSeq(SHContext *context, const SHVar &input) {
    SHVar output{};
    for (auto &item : input) {
      auto state = _shards.activate<true>(context, item, output);
      // handle return short circuit, assume it was for us
      if (state != SHWireState::Continue)
        break;
    }
    return input;
  }

  std::array<SHVar, 2> _tableItem;

  SHVar activateTable(SHContext *context, const SHVar &input) {
    const auto &table = input.payload.tableValue;
    SHVar output{};
    for (auto &[k, v] : table) {
      _tableItem[0] = Var(k);
      _tableItem[1] = v;
      const auto item = Var(_tableItem);
      const auto state = _shards.activate<true>(context, item, output);
      // handle return short circuit, assume it was for us
      if (state != SHWireState::Continue)
        break;
    }
    return input;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    throw ActivationError("Invalid activation path");
    return input;
  }

private:
  static inline Parameters _params{
      {"Apply",
       SHCCSTR("The processing logic (in the form of a shard or sequence of shards) to apply to the input sequence/table."),
       {CoreInfo::Shards}}};

  ShardsVar _shards{};
};

struct Map {
  SHTypesInfo inputTypes() { return CoreInfo::AnySeqType; }

  SHTypesInfo outputTypes() { return CoreInfo::AnySeqType; }

  SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) { _shards = value; }

  SHVar getParam(int index) { return _shards; }

  void destroy() { destroyVar(_output); }

  SHTypeInfo compose(const SHInstanceData &data) {
    if (data.inputType.seqTypes.len != 1) {
      throw SHException("Map: Invalid sequence inner type, must be a single defined type.");
    }
    SHInstanceData dataCopy = data;
    dataCopy.inputType = data.inputType.seqTypes.elements[0];
    auto innerRes = _shards.compose(dataCopy);
    _outputSingleType = innerRes.outputType;
    _outputType = {SHType::Seq, {.seqTypes = {&_outputSingleType, 1, 0}}};
    return _outputType;
  }

  void warmup(SHContext *ctx) {
    _output.valueType = SHType::Seq;
    _shards.warmup(ctx);
  }

  void cleanup(SHContext *context) { _shards.cleanup(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    SHVar output{};
    arrayResize(_output.payload.seqValue, 0);
    for (auto &item : input) {
      // handle return short circuit, assume it was for us
      auto state = _shards.activate<true>(context, item, output);
      if (state != SHWireState::Continue)
        break;
      size_t index = _output.payload.seqValue.len;
      arrayResize(_output.payload.seqValue, index + 1);
      cloneVar(_output.payload.seqValue.elements[index], output);
    }
    return _output;
  }

private:
  static inline Parameters _params{{"Apply", SHCCSTR("The function to apply to each item of the sequence."), {CoreInfo::Shards}}};

  SHVar _output{};
  ShardsVar _shards{};
  SHTypeInfo _outputSingleType{};
  Type _outputType{};
};

struct Reduce {
  SHTypesInfo inputTypes() { return CoreInfo::AnySeqType; }

  SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) { _shards = value; }

  SHVar getParam(int index) { return _shards; }

  void destroy() { destroyVar(_output); }

  SHTypeInfo compose(const SHInstanceData &data) {
    if (data.inputType.seqTypes.len != 1) {
      throw SHException("Reduce: Invalid sequence inner type, must be a single "
                        "defined type.");
    }
    // we need to edit a copy of data
    SHInstanceData dataCopy = data;
    // we need to deep copy it
    dataCopy.shared = {};
    DEFER({ arrayFree(dataCopy.shared); });
    dataCopy.inputType = data.inputType.seqTypes.elements[0];
    // copy killing any existing $0
    for (uint32_t i = data.shared.len; i > 0; i--) {
      auto idx = i - 1;
      auto &item = data.shared.elements[idx];
      if (strcmp(item.name, "$0") != 0) {
        arrayPush(dataCopy.shared, item);
      }
    }
    _tmpInfo.exposedType = dataCopy.inputType;
    arrayPush(dataCopy.shared, _tmpInfo);
    auto innerRes = _shards.compose(dataCopy);
    _outputSingleType = innerRes.outputType;
    return _outputSingleType;
  }

  void warmup(SHContext *ctx) {
    _tmp = referenceVariable(ctx, "$0");
    _shards.warmup(ctx);
  }

  void cleanup(SHContext *context) {
    _shards.cleanup(context);
    releaseVariable(_tmp);
    _tmp = nullptr;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (input.payload.seqValue.len == 0) {
      throw ActivationError("Reduce: Input sequence was empty!");
    }
    cloneVar(*_tmp, input.payload.seqValue.elements[0]);
    SHVar output{};
    for (uint32_t i = 1; i < input.payload.seqValue.len; i++) {
      auto &item = input.payload.seqValue.elements[i];
      // allow short circut with (Return)
      auto state = _shards.activate<true>(context, item, output);
      if (state != SHWireState::Continue)
        break;
      cloneVar(*_tmp, output);
    }
    cloneVar(_output, *_tmp);
    return _output;
  }

private:
  static inline Parameters _params{{"Apply", SHCCSTR("The function to apply to each item of the sequence."), {CoreInfo::Shards}}};

  SHVar *_tmp = nullptr;
  SHVar _output{};
  ShardsVar _shards{};
  SHTypeInfo _outputSingleType{};
  SHExposedTypeInfo _tmpInfo{"$0"};
};

struct Erase : SeqUser {
  static SHOptionalString help() { return SHCCSTR("Deletes an index or indices from a sequence or a key or keys from a table."); }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString inputHelp() { return SHCCSTR("Any input is ignored."); }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The input to this shard is passed through as its output."); }

  static SHParametersInfo parameters() { return _params; }

  void warmup(SHContext *ctx) {
    SeqUser::warmup(ctx);
    _indices.warmup(ctx);
  }

  void cleanup(SHContext *context) {
    _indices.cleanup(context);
    SeqUser::cleanup(context);
  }

  void setParam(int index, const SHVar &value) {
    if (index == 0) {
      _indices = value;
    } else {
      index--;
      SeqUser::setParam(index, value);
    }
  }

  SHVar getParam(int index) {
    if (index == 0) {
      return _indices;
    } else {
      index--;
      return SeqUser::getParam(index);
    }
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    bool valid = false;

    std::optional<SHExposedTypeInfo> info;
    for (uint32_t i = 0; i < data.shared.len; i++) {
      auto &reference = data.shared.elements[i];
      if (strcmp(reference.name, _name.c_str()) == 0) {
        info = reference;
        break;
      }
    }

    if (!info) {
      throw ComposeError("Erase: Could not find reference to sequence or table.");
    }

    if (info->exposedType.basicType != SHType::Seq && info->exposedType.basicType != SHType::Table) {
      throw ComposeError("Erase: Reference to sequence or table was not a sequence or table.");
    }

    auto isTable = info->exposedType.basicType == SHType::Table;

    // Figure if we output a sequence or not
    if (isTable) {
      valid = true;
    } else if (_indices->valueType == SHType::Seq) {
      if (_indices->payload.seqValue.len > 0 && _indices->payload.seqValue.elements[0].valueType == SHType::Int) {
        valid = true;
      }
    } else if (_indices->valueType == SHType::Int) {
      valid = true;
    } else { // SHType::ContextVar && !isTable
      auto info = findExposedVariable(data.shared, SHSTRVIEW((*_indices)));
      if (info) {
        if (info->exposedType.basicType == SHType::Seq && info->exposedType.seqTypes.len == 1 &&
            info->exposedType.seqTypes.elements[0].basicType == SHType::Int) {
          valid = true;
        } else if (info->exposedType.basicType == SHType::Int) {
          valid = true;
        } else {
          auto msg = "Take indices variable " + std::string(info->name) + " expected to be either SHType::Seq, SHType::Int";
          throw SHException(msg);
        }
      }
    }

    if (!valid)
      throw SHException("Erase, invalid indices or malformed input.");
    return data.inputType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    const auto &indices = _indices.get();
    if (unlikely(_target->valueType == SHType::Table)) {
      if (indices.valueType == SHType::String) {
        // single key
        const auto key = indices;
        _target->payload.tableValue.api->tableRemove(_target->payload.tableValue, key);
      } else {
        if (indices.valueType != SHType::Seq)
          throw ActivationError("Erase: Expected a sequence of keys to remove from table.");

        // multiple keys
        const uint32_t nkeys = indices.payload.seqValue.len;
        for (uint32_t i = 0; nkeys > i; i++) {
          const auto key = indices.payload.seqValue.elements[i];
          _target->payload.tableValue.api->tableRemove(_target->payload.tableValue, key);
        }
      }
    } else {
      if (indices.valueType == SHType::Int) {
        const auto index = indices.payload.intValue;
        arrayDel(_target->payload.seqValue, index);
      } else {
        if (indices.valueType != SHType::Seq)
          throw ActivationError("Erase: Expected a sequence of indices to remove from sequence.");

        _sorter.insertSort(indices.payload.seqValue.elements, indices.payload.seqValue.len, _sorter.sortDesc, _sorter.noopKeyFn);
        for (auto &idx : indices) {
          const auto index = idx.payload.intValue;
          arrayDel(_target->payload.seqValue, index);
        }
      }
    }
    return input;
  }

private:
  Sort _sorter{};
  ParamVar _indices{};
  static inline Parameters _params = {
      {"Indices", SHCCSTR("One or multiple indices to filter from a sequence."), CoreInfo::TakeTypes},
      {"Name", SHCCSTR("The name of the variable."), CoreInfo::StringOrAnyVar},
      {"Key",
       SHCCSTR("The key of the value to erase from the table (this "
               "variable will become a table)."),
       {CoreInfo::AnyType}},
      {"Global",
       SHCCSTR("If the variable is or should be available to all of the wires "
               "in the same mesh."),
       {CoreInfo::BoolType}}};
};

struct Assoc : public VariableBase {
  ExposedInfo _requiredInfo{};
  Type _tableTypes{};

  static inline Parameters params{
      {"Name", SHCCSTR("The name of the sequence or table to be updated."), {CoreInfo::StringOrAnyVar}},
      {"Key",
       SHCCSTR("Table key for the value that is to be updated. Parameter "
               "applicable if target is table."),
       {CoreInfo::AnyType}},
      {"Global",
       SHCCSTR("If the variable is or should be available to all the wires in "
               "the same mesh. The default value (false) makes the variable "
               "local to the wire."),
       {CoreInfo::BoolType}}};
  static SHParametersInfo parameters() { return params; }

  static SHOptionalString help() {
    return SHCCSTR("Updates a sequence (array) or a table (associative array/ "
                   "dictionary) on the basis of an input sequence.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnySeqType; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Input sequence that defines which element in the target sequence or table needs to be updated and with what "
                   "value. Should have even number of elements.");
  }

  static SHTypesInfo outputTypes() { return CoreInfo::AnySeqType; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("Modified array or table. Has the same type as the array or "
                   "table on which Assoc was applied.");
  }

  SHExposedTypesInfo requiredVariables() {
    _requiredInfo = ExposedInfo(ExposedInfo::Variable(_name.c_str(), SHCCSTR("The required variable."), CoreInfo::AnyType));
    return SHExposedTypesInfo(_requiredInfo);
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    for (auto &shared : data.shared) {
      std::string_view vName(shared.name);
      if (vName == _name && !shared.isMutable) {
        SHLOG_ERROR("Assoc: Variable {} is not mutable.", _name);
        throw ComposeError("Assoc: Variable is not mutable.");
      }
    }

    return data.inputType;
  }

  void warmup(SHContext *context) {
    if (_global)
      _target = referenceGlobalVariable(context, _name.c_str());
    else
      _target = referenceVariable(context, _name.c_str());
    _key.warmup(context);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (likely(_cell != nullptr)) {
      if ((input.payload.seqValue.len % 2) != 0) {
        throw ActivationError("Expected an even sized sequence as input.");
      }

      auto n = input.payload.seqValue.len / 2;

      if (_cell->valueType == SHType::Seq) {
        auto &s = _cell->payload.seqValue;
        for (uint32_t i = 0; i < n; i++) {
          auto &idx = input.payload.seqValue.elements[(i * 2) + 0];
          if (idx.valueType != SHType::Int) {
            throw ActivationError("Expected an SHType::Int for index.");
          }
          auto index = uint32_t(idx.payload.intValue);
          if (index >= s.len) {
            throw ActivationError("Index out of range, sequence is smaller.");
          }
          auto &v = input.payload.seqValue.elements[(i * 2) + 1];
          cloneVar(s.elements[index], v);
        }
      } else if (_cell->valueType == SHType::Table) {
        auto &t = _cell->payload.tableValue;
        for (uint32_t i = 0; i < n; i++) {
          auto &k = input.payload.seqValue.elements[(i * 2) + 0];
          auto &v = input.payload.seqValue.elements[(i * 2) + 1];
          SHVar *record = t.api->tableAt(t, k);
          cloneVar(*record, v);
        }
      } else {
        throw ActivationError("Expected table or sequence variable.");
      }

      if (_isTable && _key.isVariable())
        _cell = nullptr; // need to cleanup as it's a variable

      return input;
    } else {
      if (_isTable) {
        if (_target->valueType == SHType::Table) {
          auto &kv = _key.get();
          if (_target->payload.tableValue.api->tableContains(_target->payload.tableValue, kv)) {
            // Has it
            SHVar *vptr = _target->payload.tableValue.api->tableAt(_target->payload.tableValue, kv);
            // Pin fast cell
            _cell = vptr;
          } else {
            throw ActivationError("Key not found in table.");
          }
        } else {
          throw ActivationError("Table is empty or does not exist yet.");
        }
      } else {
        if (_target->valueType == SHType::Seq || _target->valueType == SHType::Table) {
          // Pin fast cell
          _cell = _target;
        } else {
          throw ActivationError("Variable is empty or does not exist yet.");
        }
      }
      // recurse in, now that we have cell
      assert(_cell);
      return activate(context, input);
    }
  }
};

struct Replace {
  static inline Types inTypes{{CoreInfo::AnySeqType, CoreInfo::StringType}};
  static inline Parameters params{
      {"Patterns",
       SHCCSTR("The patterns to find."),
       {CoreInfo::NoneType, CoreInfo::StringSeqType, CoreInfo::StringVarSeqType, CoreInfo::AnyVarSeqType, CoreInfo::AnySeqType}},
      {"Replacements",
       SHCCSTR("The replacements to apply to the input, if a single value is "
               "provided every match will be replaced with that single value."),
       {CoreInfo::NoneType, CoreInfo::AnyType, CoreInfo::AnyVarType, CoreInfo::AnySeqType, CoreInfo::AnyVarSeqType}}};

  static SHTypesInfo inputTypes() { return inTypes; }
  static SHTypesInfo outputTypes() { return inTypes; }
  static SHParametersInfo parameters() { return params; }

  ParamVar _patterns{};
  ParamVar _replacements{};
  std::string _stringOutput;
  OwnedVar _vectorOutput;

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _patterns = value;
      break;
    case 1:
      _replacements = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _patterns;
    case 1:
      return _replacements;
    default:
      return Var::Empty;
    }
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    if (_patterns->valueType == SHType::None) {
      data.shard->inlineShardId = InlineShard::NoopShard;
    } else {
      data.shard->inlineShardId = InlineShard::NotInline;
    }

    if (data.inputType.basicType == SHType::String) {
      OVERRIDE_ACTIVATE(data, activateString);
      return CoreInfo::StringType;
    } else {
      // this should be only sequence
      OVERRIDE_ACTIVATE(data, activateSeq);
      return data.inputType;
    }
  }

  void warmup(SHContext *context) {
    _patterns.warmup(context);
    _replacements.warmup(context);
  }

  void cleanup(SHContext *context) {
    _patterns.cleanup();
    _replacements.cleanup();
  }

  SHVar activateSeq(SHContext *context, const SHVar &input) {
    _vectorOutput = input;
    IterableSeq o(_vectorOutput);
    const auto &patterns = _patterns.get();
    const auto &replacements = _replacements.get();
    if (replacements.valueType == SHType::Seq) {
      if (patterns.payload.seqValue.len != replacements.payload.seqValue.len) {
        throw ActivationError("Translate patterns size mismatch, must be equal "
                              "to replacements size.");
      }

      for (uint32_t i = 0; i < patterns.payload.seqValue.len; i++) {
        const auto &p = patterns.payload.seqValue.elements[i];
        const auto &r = replacements.payload.seqValue.elements[i];
        std::replace(o.begin(), o.end(), p, r);
      }
    } else {
      for (uint32_t i = 0; i < patterns.payload.seqValue.len; i++) {
        const auto &p = patterns.payload.seqValue.elements[i];
        std::replace(o.begin(), o.end(), p, replacements);
      }
    }
    return Var(_vectorOutput);
  }

  SHVar activateString(SHContext *context, const SHVar &input) {
    const auto source = input.payload.stringLen > 0 ? std::string_view(input.payload.stringValue, input.payload.stringLen)
                                                    : std::string_view(input.payload.stringValue);
    _stringOutput.assign(source);
    const auto &patterns = _patterns.get();
    const auto &replacements = _replacements.get();
    if (replacements.valueType == SHType::Seq) {
      if (patterns.payload.seqValue.len != replacements.payload.seqValue.len) {
        throw ActivationError("Translate patterns size mismatch, must be equal "
                              "to replacements size.");
      }
      for (uint32_t i = 0; i < patterns.payload.seqValue.len; i++) {
        const auto &p = patterns.payload.seqValue.elements[i];
        const auto &r = replacements.payload.seqValue.elements[i];
        const auto pattern = p.payload.stringLen > 0 ? std::string_view(p.payload.stringValue, p.payload.stringLen)
                                                     : std::string_view(p.payload.stringValue);
        const auto replacement = r.payload.stringLen > 0 ? std::string_view(r.payload.stringValue, r.payload.stringLen)
                                                         : std::string_view(r.payload.stringValue);
        boost::replace_all(_stringOutput, pattern, replacement);
      }
    } else {
      const auto replacement = replacements.payload.stringLen > 0
                                   ? std::string_view(replacements.payload.stringValue, replacements.payload.stringLen)
                                   : std::string_view(replacements.payload.stringValue);
      for (uint32_t i = 0; i < patterns.payload.seqValue.len; i++) {
        const auto &p = patterns.payload.seqValue.elements[i];
        const auto pattern = p.payload.stringLen > 0 ? std::string_view(p.payload.stringValue, p.payload.stringLen)
                                                     : std::string_view(p.payload.stringValue);

        boost::replace_all(_stringOutput, pattern, replacement);
      }
    }
    return Var(_stringOutput);
  }

  SHVar activate(SHContext *context, const SHVar &input) { throw ActivationError("Invalid activation function"); }
};

struct Reverse {
  static inline Types inTypes{{CoreInfo::AnySeqType, CoreInfo::StringType, CoreInfo::BytesType}};

  static SHTypesInfo inputTypes() { return inTypes; }
  static SHTypesInfo outputTypes() { return inTypes; }

  std::string _stringOutput;
  OwnedVar _vectorOutput;
  std::vector<uint8_t> _bytesOutput;

  SHTypeInfo compose(const SHInstanceData &data) {
    if (data.inputType.basicType == SHType::String) {
      OVERRIDE_ACTIVATE(data, activateString);
      return CoreInfo::StringType;
    } else if (data.inputType.basicType == SHType::Bytes) {
      OVERRIDE_ACTIVATE(data, activateBytes);
      return CoreInfo::BytesType;
    } else {
      // this should be only sequence
      OVERRIDE_ACTIVATE(data, activateSeq);
      return data.inputType;
    }
  }

  SHVar activateSeq(SHContext *context, const SHVar &input) {
    _vectorOutput = input;
    IterableSeq o(_vectorOutput);
    std::reverse(o.begin(), o.end());
    return Var(_vectorOutput);
  }

  SHVar activateString(SHContext *context, const SHVar &input) {
    const auto source = input.payload.stringLen > 0 ? std::string_view(input.payload.stringValue, input.payload.stringLen)
                                                    : std::string_view(input.payload.stringValue);
    _stringOutput.assign(source);
    std::reverse(_stringOutput.begin(), _stringOutput.end());
    return Var(_stringOutput);
  }

  SHVar activateBytes(SHContext *context, const SHVar &input) {
    _bytesOutput.assign(input.payload.bytesValue, input.payload.bytesValue + input.payload.bytesSize);
    std::reverse(_bytesOutput.begin(), _bytesOutput.end());
    return Var(_bytesOutput);
  }

  SHVar activate(SHContext *context, const SHVar &input) { throw ActivationError("Invalid activation function"); }
};

struct UnsafeActivate {
  static inline Parameters params{
      {"Activate",
       SHCCSTR("The function address, must be of type std::function<SHVar(SHContext *, const SHVar &)>."),
       {CoreInfo::IntType}},
      {"Cleanup", SHCCSTR("The function address, must be of type std::function<void(SHContext *)>."), {CoreInfo::IntType}}};

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static SHParametersInfo parameters() { return params; }

  using ActivationFunc = std::function<SHVar(SHContext *, const SHVar &)>;
  ActivationFunc *_func{nullptr};

  using CleanupFunc = std::function<void(SHContext *)>;
  CleanupFunc *_cleanup{nullptr};

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _func = reinterpret_cast<ActivationFunc *>(value.payload.intValue);
      break;
    case 1:
      _cleanup = reinterpret_cast<CleanupFunc *>(value.payload.intValue);
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(reinterpret_cast<int64_t>(_func));
    case 1:
      return Var(reinterpret_cast<int64_t>(_cleanup));
    default:
      return Var::Empty;
    }
  }

  void cleanup(SHContext *context) {
    if (_cleanup) {
      (*_cleanup)(context);
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) { return (*_func)(context, input); }
};

struct GetShards {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringSeqType; }

  SeqVar _output{};

  SHVar activate(SHContext *context, const SHVar &input) {
    for (auto [name, _] : shards::GetGlobals().ShardsRegister) {
      _output.emplace_back(Var(name));
    }
    return _output;
  }
};

struct GetShardHelp {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyTableType; }

  TableVar _output{};

  static Var ostr(SHOptionalString &str) { return Var(str.string ? str.string : getString(str.crc)); }

  static SeqVar richTypeInfo(const SHTypesInfo &types, bool ignoreNone = true) {
    SeqVar s{};
    for (uint32_t j = 0; j < types.len; j++) {
      auto &type = types.elements[j];
      if (ignoreNone && type.basicType == SHType::None)
        continue;
      TableVar t{};
      std::stringstream ss;
      ss << type;
      auto name = ss.str();
      t["name"] = Var(name);
      t["type"] = Var(int64_t(type.basicType));
      s.emplace_back(std::move(t));
    }
    return s;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
#ifdef SH_COMPRESSED_STRINGS
    static bool decompressed = false;
    if (!decompressed) {
      decompressStrings();
      decompressed = true;
    }
#endif

    auto blkname = SHSTRVIEW(input);
    auto shard = createShard(blkname);
    if (!shard) {
      throw ActivationError(fmt::format("Shard {} creation failed, likely does not exist", blkname));
    }
    DEFER(shard->destroy(shard));

    shard->setup(shard);

    _output.clear();

    auto help = shard->help(shard);
    _output.insert(Var("help"), ostr(help));

    auto inputTypes = shard->inputTypes(shard);
    _output.insert(Var("inputTypes"), richTypeInfo(inputTypes, false));
    auto inputHelp = shard->inputHelp(shard);
    _output.insert(Var("inputHelp"), ostr(inputHelp));

    auto outputTypes = shard->outputTypes(shard);
    _output.insert(Var("outputTypes"), richTypeInfo(outputTypes, false));
    auto outputHelp = shard->outputHelp(shard);
    _output.insert(Var("outputHelp"), ostr(outputHelp));

    auto params = shard->parameters(shard);
    if (params.len > 0) {
      SeqVar paramsSeq{};
      for (uint32_t i = 0; i < params.len; i++) {
        auto &param = params.elements[i];
        TableVar paramTable{};
        paramTable.insert(Var("name"), Var(param.name));
        paramTable.insert(Var("help"), ostr(param.help));
        paramTable.insert(Var("types"), richTypeInfo(param.valueTypes, false));
        paramTable.insert(Var("default"), shard->getParam(shard, i));
        paramsSeq.emplace_back(std::move(paramTable));
      }
      _output.insert(Var("parameters"), std::move(paramsSeq));
    } else {
      _output.insert(Var("parameters"), SeqVar{});
    }

    auto properties = shard->properties(shard);
    if (properties) {
      TableVar propertiesTable{};
      ForEach(*properties, [&](auto &key, auto &val) { propertiesTable[key] = val; });
      _output.insert(Var("properties"), std::move(propertiesTable));
    }

    return _output;
  }
};

// Register And
RUNTIME_CORE_SHARD_FACTORY(And);
RUNTIME_SHARD_help(And);
RUNTIME_SHARD_inputTypes(And);
RUNTIME_SHARD_inputHelp(And);
RUNTIME_SHARD_outputTypes(And);
RUNTIME_SHARD_outputHelp(And);
RUNTIME_SHARD_activate(And);
RUNTIME_SHARD_END(And);

// Register Or
RUNTIME_CORE_SHARD_FACTORY(Or);
RUNTIME_SHARD_help(Or);
RUNTIME_SHARD_inputTypes(Or);
RUNTIME_SHARD_inputHelp(Or);
RUNTIME_SHARD_outputTypes(Or);
RUNTIME_SHARD_outputHelp(Or);
RUNTIME_SHARD_activate(Or);
RUNTIME_SHARD_END(Or);

// Register Not
RUNTIME_CORE_SHARD_FACTORY(Not);
RUNTIME_SHARD_help(Not);
RUNTIME_SHARD_inputTypes(Not);
RUNTIME_SHARD_inputHelp(Not);
RUNTIME_SHARD_outputTypes(Not);
RUNTIME_SHARD_outputHelp(Not);
RUNTIME_SHARD_activate(Not);
RUNTIME_SHARD_END(Not);

// Register IsNan
RUNTIME_CORE_SHARD_FACTORY(IsValidNumber);
RUNTIME_SHARD_inputTypes(IsValidNumber);
RUNTIME_SHARD_outputTypes(IsValidNumber);
RUNTIME_SHARD_activate(IsValidNumber);
RUNTIME_SHARD_END(IsValidNumber);

// Register IsAlmost
RUNTIME_CORE_SHARD_FACTORY(IsAlmost);
RUNTIME_SHARD_help(IsAlmost);
RUNTIME_SHARD_inputTypes(IsAlmost);
RUNTIME_SHARD_inputHelp(IsAlmost);
RUNTIME_SHARD_outputTypes(IsAlmost);
RUNTIME_SHARD_outputHelp(IsAlmost);
RUNTIME_SHARD_parameters(IsAlmost);
RUNTIME_SHARD_setParam(IsAlmost);
RUNTIME_SHARD_getParam(IsAlmost);
RUNTIME_SHARD_compose(IsAlmost);
RUNTIME_SHARD_activate(IsAlmost);
RUNTIME_SHARD_END(IsAlmost);

// Register Set
RUNTIME_CORE_SHARD_FACTORY(Set);
RUNTIME_SHARD_cleanup(Set);
RUNTIME_SHARD_warmup(Set);
RUNTIME_SHARD_help(Set);
RUNTIME_SHARD_inputTypes(Set);
RUNTIME_SHARD_inputHelp(Set);
RUNTIME_SHARD_outputTypes(Set);
RUNTIME_SHARD_outputHelp(Set);
RUNTIME_SHARD_parameters(Set);
RUNTIME_SHARD_compose(Set);
RUNTIME_SHARD_exposedVariables(Set);
RUNTIME_SHARD_setParam(Set);
RUNTIME_SHARD_getParam(Set);
RUNTIME_SHARD_activate(Set);
RUNTIME_SHARD_END(Set);

// Register Ref
RUNTIME_CORE_SHARD_FACTORY(Ref);
RUNTIME_SHARD_cleanup(Ref);
RUNTIME_SHARD_warmup(Ref);
RUNTIME_SHARD_help(Ref);
RUNTIME_SHARD_inputTypes(Ref);
RUNTIME_SHARD_inputHelp(Ref);
RUNTIME_SHARD_outputTypes(Ref);
RUNTIME_SHARD_outputHelp(Ref);
RUNTIME_SHARD_parameters(Ref);
RUNTIME_SHARD_compose(Ref);
RUNTIME_SHARD_exposedVariables(Ref);
RUNTIME_SHARD_setParam(Ref);
RUNTIME_SHARD_getParam(Ref);
RUNTIME_SHARD_activate(Ref);
RUNTIME_SHARD_END(Ref);

// Register Update
RUNTIME_CORE_SHARD_FACTORY(Update);
RUNTIME_SHARD_cleanup(Update);
RUNTIME_SHARD_warmup(Update);
RUNTIME_SHARD_help(Update);
RUNTIME_SHARD_inputTypes(Update);
RUNTIME_SHARD_inputHelp(Update);
RUNTIME_SHARD_outputTypes(Update);
RUNTIME_SHARD_outputHelp(Update);
RUNTIME_SHARD_parameters(Update);
RUNTIME_SHARD_compose(Update);
RUNTIME_SHARD_requiredVariables(Update);
RUNTIME_SHARD_setParam(Update);
RUNTIME_SHARD_getParam(Update);
RUNTIME_SHARD_activate(Update);
RUNTIME_SHARD_END(Update);

// Register Push
RUNTIME_CORE_SHARD_FACTORY(Push);
RUNTIME_SHARD_cleanup(Push);
RUNTIME_SHARD_destroy(Push);
RUNTIME_SHARD_warmup(Push);
RUNTIME_SHARD_help(Push);
RUNTIME_SHARD_inputTypes(Push);
RUNTIME_SHARD_inputHelp(Push);
RUNTIME_SHARD_outputTypes(Push);
RUNTIME_SHARD_outputHelp(Push);
RUNTIME_SHARD_parameters(Push);
RUNTIME_SHARD_compose(Push);
RUNTIME_SHARD_exposedVariables(Push);
RUNTIME_SHARD_setParam(Push);
RUNTIME_SHARD_getParam(Push);
RUNTIME_SHARD_activate(Push);
RUNTIME_SHARD_END(Push);

// Register Sequence
RUNTIME_CORE_SHARD_FACTORY(Sequence);
RUNTIME_SHARD_cleanup(Sequence);
RUNTIME_SHARD_destroy(Sequence);
RUNTIME_SHARD_warmup(Sequence);
RUNTIME_SHARD_help(Sequence);
RUNTIME_SHARD_inputTypes(Sequence);
RUNTIME_SHARD_inputHelp(Sequence);
RUNTIME_SHARD_outputTypes(Sequence);
RUNTIME_SHARD_outputHelp(Sequence);
RUNTIME_SHARD_parameters(Sequence);
RUNTIME_SHARD_compose(Sequence);
RUNTIME_SHARD_exposedVariables(Sequence);
RUNTIME_SHARD_setParam(Sequence);
RUNTIME_SHARD_getParam(Sequence);
RUNTIME_SHARD_activate(Sequence);
RUNTIME_SHARD_END(Sequence);

// Register Pop
RUNTIME_CORE_SHARD_FACTORY(Pop);
RUNTIME_SHARD_cleanup(Pop);
RUNTIME_SHARD_warmup(Pop);
RUNTIME_SHARD_destroy(Pop);
RUNTIME_SHARD_help(Pop);
RUNTIME_SHARD_inputTypes(Pop);
RUNTIME_SHARD_inputHelp(Pop);
RUNTIME_SHARD_outputTypes(Pop);
RUNTIME_SHARD_outputHelp(Pop);
RUNTIME_SHARD_parameters(Pop);
RUNTIME_SHARD_compose(Pop);
RUNTIME_SHARD_requiredVariables(Pop);
RUNTIME_SHARD_setParam(Pop);
RUNTIME_SHARD_getParam(Pop);
RUNTIME_SHARD_activate(Pop);
RUNTIME_SHARD_END(Pop);

// Register PopFront
RUNTIME_CORE_SHARD_FACTORY(PopFront);
RUNTIME_SHARD_cleanup(PopFront);
RUNTIME_SHARD_warmup(PopFront);
RUNTIME_SHARD_destroy(PopFront);
RUNTIME_SHARD_help(PopFront);
RUNTIME_SHARD_inputTypes(PopFront);
RUNTIME_SHARD_inputHelp(PopFront);
RUNTIME_SHARD_outputTypes(PopFront);
RUNTIME_SHARD_outputHelp(PopFront);
RUNTIME_SHARD_parameters(PopFront);
RUNTIME_SHARD_compose(PopFront);
RUNTIME_SHARD_requiredVariables(PopFront);
RUNTIME_SHARD_setParam(PopFront);
RUNTIME_SHARD_getParam(PopFront);
RUNTIME_SHARD_activate(PopFront);
RUNTIME_SHARD_END(PopFront);

// Register Count
RUNTIME_CORE_SHARD_FACTORY(Count);
RUNTIME_SHARD_cleanup(Count);
RUNTIME_SHARD_warmup(Count);
RUNTIME_SHARD_help(Count);
RUNTIME_SHARD_inputTypes(Count);
RUNTIME_SHARD_inputHelp(Count);
RUNTIME_SHARD_outputTypes(Count);
RUNTIME_SHARD_outputHelp(Count);
RUNTIME_SHARD_parameters(Count);
RUNTIME_SHARD_setParam(Count);
RUNTIME_SHARD_getParam(Count);
RUNTIME_SHARD_activate(Count);
RUNTIME_SHARD_END(Count);

// Register Clear
RUNTIME_CORE_SHARD_FACTORY(Clear);
RUNTIME_SHARD_cleanup(Clear);
RUNTIME_SHARD_warmup(Clear);
RUNTIME_SHARD_help(Clear);
RUNTIME_SHARD_inputTypes(Clear);
RUNTIME_SHARD_inputHelp(Clear);
RUNTIME_SHARD_outputTypes(Clear);
RUNTIME_SHARD_outputHelp(Clear);
RUNTIME_SHARD_parameters(Clear);
RUNTIME_SHARD_setParam(Clear);
RUNTIME_SHARD_getParam(Clear);
RUNTIME_SHARD_activate(Clear);
RUNTIME_SHARD_END(Clear);

// Register Drop
RUNTIME_CORE_SHARD_FACTORY(Drop);
RUNTIME_SHARD_cleanup(Drop);
RUNTIME_SHARD_warmup(Drop);
RUNTIME_SHARD_help(Drop);
RUNTIME_SHARD_inputTypes(Drop);
RUNTIME_SHARD_inputHelp(Drop);
RUNTIME_SHARD_outputTypes(Drop);
RUNTIME_SHARD_outputHelp(Drop);
RUNTIME_SHARD_parameters(Drop);
RUNTIME_SHARD_setParam(Drop);
RUNTIME_SHARD_getParam(Drop);
RUNTIME_SHARD_activate(Drop);
RUNTIME_SHARD_END(Drop);

// Register DropFront
RUNTIME_CORE_SHARD_FACTORY(DropFront);
RUNTIME_SHARD_cleanup(DropFront);
RUNTIME_SHARD_warmup(DropFront);
RUNTIME_SHARD_help(DropFront);
RUNTIME_SHARD_inputTypes(DropFront);
RUNTIME_SHARD_inputHelp(DropFront);
RUNTIME_SHARD_outputTypes(DropFront);
RUNTIME_SHARD_outputHelp(DropFront);
RUNTIME_SHARD_parameters(DropFront);
RUNTIME_SHARD_setParam(DropFront);
RUNTIME_SHARD_getParam(DropFront);
RUNTIME_SHARD_activate(DropFront);
RUNTIME_SHARD_END(DropFront);

// Register Get
RUNTIME_CORE_SHARD_FACTORY(Get);
RUNTIME_SHARD_cleanup(Get);
RUNTIME_SHARD_warmup(Get);
RUNTIME_SHARD_destroy(Get);
RUNTIME_SHARD_help(Get);
RUNTIME_SHARD_inputTypes(Get);
RUNTIME_SHARD_inputHelp(Get);
RUNTIME_SHARD_outputTypes(Get);
RUNTIME_SHARD_outputHelp(Get);
RUNTIME_SHARD_parameters(Get);
RUNTIME_SHARD_compose(Get);
RUNTIME_SHARD_requiredVariables(Get);
RUNTIME_SHARD_setParam(Get);
RUNTIME_SHARD_getParam(Get);
RUNTIME_SHARD_activate(Get);
RUNTIME_SHARD_END(Get);

// Register Swap
RUNTIME_CORE_SHARD_FACTORY(Swap);
RUNTIME_SHARD_inputTypes(Swap);
RUNTIME_SHARD_warmup(Swap);
RUNTIME_SHARD_cleanup(Swap);
RUNTIME_SHARD_help(Swap);
RUNTIME_SHARD_inputTypes(Swap);
RUNTIME_SHARD_inputHelp(Swap);
RUNTIME_SHARD_outputTypes(Swap);
RUNTIME_SHARD_outputHelp(Swap);
RUNTIME_SHARD_parameters(Swap);
RUNTIME_SHARD_requiredVariables(Swap);
RUNTIME_SHARD_setParam(Swap);
RUNTIME_SHARD_getParam(Swap);
RUNTIME_SHARD_activate(Swap);
RUNTIME_SHARD_END(Swap);

// Register Take
RUNTIME_CORE_SHARD_FACTORY(Take);
RUNTIME_SHARD_destroy(Take);
RUNTIME_SHARD_cleanup(Take);
RUNTIME_SHARD_requiredVariables(Take);
RUNTIME_SHARD_help(Take);
RUNTIME_SHARD_inputTypes(Take);
RUNTIME_SHARD_inputHelp(Take);
RUNTIME_SHARD_outputTypes(Take);
RUNTIME_SHARD_outputHelp(Take);
RUNTIME_SHARD_parameters(Take);
RUNTIME_SHARD_compose(Take);
RUNTIME_SHARD_setParam(Take);
RUNTIME_SHARD_getParam(Take);
RUNTIME_SHARD_warmup(Take);
RUNTIME_SHARD_activate(Take);
RUNTIME_SHARD_END(Take);

// Register RTake
RUNTIME_CORE_SHARD_FACTORY(RTake);
RUNTIME_SHARD_destroy(RTake);
RUNTIME_SHARD_cleanup(RTake);
RUNTIME_SHARD_requiredVariables(RTake);
RUNTIME_SHARD_help(RTake);
RUNTIME_SHARD_inputTypes(RTake);
RUNTIME_SHARD_inputHelp(RTake);
RUNTIME_SHARD_outputTypes(RTake);
RUNTIME_SHARD_outputHelp(RTake);
RUNTIME_SHARD_parameters(RTake);
RUNTIME_SHARD_compose(RTake);
RUNTIME_SHARD_setParam(RTake);
RUNTIME_SHARD_getParam(RTake);
RUNTIME_SHARD_warmup(RTake);
RUNTIME_SHARD_activate(RTake);
RUNTIME_SHARD_END(RTake);

// Register Slice
RUNTIME_CORE_SHARD_FACTORY(Slice);
RUNTIME_SHARD_destroy(Slice);
RUNTIME_SHARD_cleanup(Slice);
RUNTIME_SHARD_requiredVariables(Slice);
RUNTIME_SHARD_help(Slice);
RUNTIME_SHARD_inputTypes(Slice);
RUNTIME_SHARD_inputHelp(Slice);
RUNTIME_SHARD_outputTypes(Slice);
RUNTIME_SHARD_outputHelp(Slice);
RUNTIME_SHARD_parameters(Slice);
RUNTIME_SHARD_compose(Slice);
RUNTIME_SHARD_setParam(Slice);
RUNTIME_SHARD_getParam(Slice);
RUNTIME_SHARD_activate(Slice);
RUNTIME_SHARD_END(Slice);

// Register Limit
RUNTIME_CORE_SHARD_FACTORY(Limit);
RUNTIME_SHARD_destroy(Limit);
RUNTIME_SHARD_inputTypes(Limit);
RUNTIME_SHARD_outputTypes(Limit);
RUNTIME_SHARD_parameters(Limit);
RUNTIME_SHARD_compose(Limit);
RUNTIME_SHARD_setParam(Limit);
RUNTIME_SHARD_getParam(Limit);
RUNTIME_SHARD_activate(Limit);
RUNTIME_SHARD_END(Limit);

// Register Repeat
RUNTIME_CORE_SHARD_FACTORY(Repeat);
RUNTIME_SHARD_help(Repeat);
RUNTIME_SHARD_inputTypes(Repeat);
RUNTIME_SHARD_inputHelp(Repeat);
RUNTIME_SHARD_outputTypes(Repeat);
RUNTIME_SHARD_outputHelp(Repeat);
RUNTIME_SHARD_parameters(Repeat);
RUNTIME_SHARD_setParam(Repeat);
RUNTIME_SHARD_getParam(Repeat);
RUNTIME_SHARD_activate(Repeat);
RUNTIME_SHARD_cleanup(Repeat);
RUNTIME_SHARD_warmup(Repeat);
RUNTIME_SHARD_requiredVariables(Repeat);
RUNTIME_SHARD_compose(Repeat);
RUNTIME_SHARD_END(Repeat);

// Register Sort
RUNTIME_CORE_SHARD(Sort);
RUNTIME_SHARD_setup(Sort);
RUNTIME_SHARD_help(Sort);
RUNTIME_SHARD_inputTypes(Sort);
RUNTIME_SHARD_inputHelp(Sort);
RUNTIME_SHARD_outputTypes(Sort);
RUNTIME_SHARD_outputHelp(Sort);
RUNTIME_SHARD_compose(Sort);
RUNTIME_SHARD_activate(Sort);
RUNTIME_SHARD_parameters(Sort);
RUNTIME_SHARD_setParam(Sort);
RUNTIME_SHARD_getParam(Sort);
RUNTIME_SHARD_cleanup(Sort);
RUNTIME_SHARD_warmup(Sort);
RUNTIME_SHARD_END(Sort);

// Register Remove
RUNTIME_CORE_SHARD(Remove);
RUNTIME_SHARD_help(Remove);
RUNTIME_SHARD_inputTypes(Remove);
RUNTIME_SHARD_inputHelp(Remove);
RUNTIME_SHARD_outputTypes(Remove);
RUNTIME_SHARD_outputHelp(Remove);
RUNTIME_SHARD_parameters(Remove);
RUNTIME_SHARD_setParam(Remove);
RUNTIME_SHARD_getParam(Remove);
RUNTIME_SHARD_activate(Remove);
RUNTIME_SHARD_cleanup(Remove);
RUNTIME_SHARD_warmup(Remove);
RUNTIME_SHARD_compose(Remove);
RUNTIME_SHARD_END(Remove);

LOGIC_OP_DESC(Is);
LOGIC_OP_DESC(IsNot);
LOGIC_OP_DESC(IsMore);
LOGIC_OP_DESC(IsLess);
LOGIC_OP_DESC(IsMoreEqual);
LOGIC_OP_DESC(IsLessEqual);

LOGIC_OP_DESC(IsAny);
LOGIC_OP_DESC(IsAll);
LOGIC_OP_DESC(IsAnyNot);
LOGIC_OP_DESC(IsAllNot);
LOGIC_OP_DESC(IsAnyMore);
LOGIC_OP_DESC(IsAllMore);
LOGIC_OP_DESC(IsAnyLess);
LOGIC_OP_DESC(IsAllLess);
LOGIC_OP_DESC(IsAnyMoreEqual);
LOGIC_OP_DESC(IsAllMoreEqual);
LOGIC_OP_DESC(IsAnyLessEqual);
LOGIC_OP_DESC(IsAllLessEqual);

SHVar unreachableActivation(const SHVar &input) { throw; }

SHVar exitProgramActivation(const SHVar &input) { exit(input.payload.intValue); }

SHVar hashActivation(const SHVar &input) { return shards::hash(input); }

SHVar blockingSleepActivation(const SHVar &input) {
  if (input.valueType == SHType::Int) {
    sleep(double(input.payload.intValue) / 1000.0, false);
  } else if (input.valueType == SHType::Float) {
    sleep(input.payload.floatValue, false);
  } else {
    throw ActivationError("Expected either SHType::Int (ms) or SHType::Float (seconds)");
  }
  return input;
}

struct LastError {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto &msg = context->getErrorMessage();
    if (msg.empty()) {
      return Var("");
    } else {
      return Var(msg);
    }
  }
};

struct LowestHighestShard {
  static SHTypesInfo inputTypes() { return CoreInfo::AnySeqType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  SHTypeInfo compose(const SHInstanceData &data) {
    // compose, if input is a single type sequence we can know output type!
    assert(data.inputType.basicType == SHType::Seq);
    if (data.inputType.seqTypes.len == 1) {
      return data.inputType.seqTypes.elements[0];
    } else {
      throw ComposeError("Expected a single type sequence");
    }
  }
};

struct LowestShard : LowestHighestShard {
  SHVar activate(SHContext *context, const SHVar &input) {
    auto &seq = input.payload.seqValue;
    if (seq.len == 0) {
      throw ActivationError("Cannot find lowest value in an empty sequence");
    }
    auto lowest = seq.elements[0];
    for (uint32_t i = 1; i < seq.len; i++) {
      if (lowest > seq.elements[i]) {
        lowest = seq.elements[i];
      }
    }
    return lowest;
  }
};

struct HighestShard : LowestHighestShard {
  SHVar activate(SHContext *context, const SHVar &input) {
    auto &seq = input.payload.seqValue;
    if (seq.len == 0) {
      throw ActivationError("Cannot find highest value in an empty sequence");
    }
    auto highest = seq.elements[0];
    for (uint32_t i = 1; i < seq.len; i++) {
      if (highest < seq.elements[i]) {
        highest = seq.elements[i];
      }
    }
    return highest;
  }
};

#ifdef __EMSCRIPTEN__
extern "C" {
char *emEval(const char *code);
double now();
bool emEvalAsyncRun(const char *code, size_t index);
int emEvalAsyncCheck(size_t index);
char *emEvalAsyncGet(size_t index);
void emBrowsePage(const char *url);
}

SHVar emscriptenEvalActivation(const SHVar &input) {
  static thread_local std::string str;
  auto res = emEval(input.payload.stringValue);
  const auto check = reinterpret_cast<intptr_t>(res);
  if (check == -1) {
    throw ActivationError("Failure on the javascript side, check console");
  }
  str.clear();
  if (res) {
    str.assign(res);
    free(res);
  }
  return Var(str);
}

/*
using embind fails inside workers... that's why we implemented it using js
yet using async in workers is still broken so let's use embind for now
*/
struct EmscriptenAsyncEval {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    static thread_local std::string str;
    const static emscripten::val eval = emscripten::val::global("eval");
    str = emscripten_wait<std::string>(context, eval(emscripten::val(input.payload.stringValue)));
    return Var(str);
  }
};

// struct EmscriptenAsyncEval {
//   static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
//   static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

//   SHVar activate(SHContext *context, const SHVar &input) {
//     static thread_local std::string str;
//     static thread_local size_t slots;

//     size_t slot = ++slots;
//     const auto startCheck = emEvalAsyncRun(input.payload.stringValue, slot);
//     if (!startCheck) {
//       throw ActivationError("Failed to start a javascript async task.");
//     }

//     while (true) {
//       const auto runCheck = emEvalAsyncCheck(slot);
//       if (runCheck == 0) {
//         suspend(context, 0.0);
//       } else if (runCheck == -1) {
//         throw ActivationError("Failure on the javascript side, check
//         console");
//       } else {
//         break;
//       }
//     }

//     const auto res = emEvalAsyncGet(slot);
//     str.clear();
//     if (res) {
//       str.assign(res);
//       free(res);
//     }
//     return Var(str);
//   }
// };

SHVar emscriptenBrowseActivation(const SHVar &input) {
  emBrowsePage(input.payload.stringValue);
  return Var(input);
}
#endif

#define SH_ONCE_TIMER_DEBUG 0

struct LogIntervalTimer {
  const double interval = 1.0f;
  SHTimeDiff nextLog;

  template <typename T> void tick(SHClock::time_point now, T cb) {
    if (now >= nextLog) {
      nextLog = now + SHDuration(interval);
      cb();
    }
  }
};

struct Once {
  SHTimeDiff _next;

#if SH_ONCE_TIMER_DEBUG
  LogIntervalTimer _logIntervalTimer;
  SHTime _lastInnerActivation;
  SHTime _lastTick;
  gfx::MovingAverage<double> _granularityBuffer{32};
  gfx::MovingAverage<double> _avgRate{32};
#endif

  ShardsVar _blks;
  ExposedInfo _requiredInfo{};
  SHComposeResult _validation{};
  ParamVar _repeat{};
  Shard *self{nullptr};

  void cleanup(SHContext *context) {
    _repeat.cleanup(context);
    _blks.cleanup(context);
    if (self)
      self->inlineShardId = InlineShard::CoreOnce;
    _next = {};
  }

  void warmup(SHContext *ctx) {
    _repeat.warmup(ctx);
    _blks.warmup(ctx);
    _next = SHClock::now();
  }

  static inline Parameters params{{"Action", SHCCSTR("The shard or sequence of shards to execute."), {CoreInfo::Shards}},
                                  {"Every",
                                   SHCCSTR("The number of seconds to wait until repeating the action, if 0 "
                                           "the action will happen only once per wire flow execution."),
                                   {CoreInfo::FloatType, CoreInfo::FloatVarType}}};

  static SHOptionalString help() {
    return SHCCSTR("Executes the shard or sequence of shards with the desired frequency in a wire flow execution.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static SHParametersInfo parameters() { return params; }

  bool isRepeating() const { return !_repeat.isNone(); }
  SHDuration getRepeatTime() const { return SHDuration(_repeat.get().payload.floatValue); }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _blks = value;
      break;
    case 1:
      _repeat = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _blks;
    case 1:
      return _repeat;
    default:
      break;
    }
    throw SHException("Parameter out of range.");
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    _requiredInfo.clear();

    self = data.shard;
    _validation = _blks.compose(data);

    collectRequiredVariables(data.shared, _requiredInfo, _repeat);

    return data.inputType;
  }

  SHExposedTypesInfo requiredVariables() { return SHExposedTypesInfo(_requiredInfo); }
  SHExposedTypesInfo exposedVariables() { return _validation.exposedInfo; }

  ALWAYS_INLINE void activateOnce(SHContext *context, const SHVar &input) {
    SHVar output{};
    _blks.activate(context, input, output);
    // let's cheat in this case and stop triggering this call
    shassert(self != nullptr && "self is null");
    self->inlineShardId = InlineShard::NoopShard;
  }

  ALWAYS_INLINE void activateTimed(SHContext *context, const SHVar &input) {
    SHDuration dsleep = getRepeatTime();

    // Get the current time
    SHTime start = SHClock::now();

#if SH_ONCE_TIMER_DEBUG
    SHDuration timeSinceLastTick = start - _lastTick;
    _lastTick = start;
    _granularityBuffer.add(timeSinceLastTick.count());
#endif

    if (start >= (_next)) {
#if SH_ONCE_TIMER_DEBUG
      _avgRate.add(SHDuration(start - _lastInnerActivation).count());
      _lastInnerActivation = start;
#endif
      _next = _next + dsleep;
      if(_next < start)
        _next = start + dsleep;

      // Call the activation function
      SHVar output{};
      _blks.activate(context, input, output);
    }

#if SH_ONCE_TIMER_DEBUG
    _logIntervalTimer.tick(start, [&]() {
      double granularity = _granularityBuffer.getAverage();
      double rate = _avgRate.getAverage();
      SHDuration errorDur = SHDuration(rate) - dsleep;
      double error = errorDur.count();
      SHLOG_INFO("Once(Rate: {:.4f} / {:.2f}/s Error: {:.8f} Granularity: {:.4f} / {:.2f}/s)", rate, 1.0 / rate, error,
                 granularity, 1.0 / granularity);
    });
#endif
  }

  ALWAYS_INLINE SHVar activate(SHContext *context, const SHVar &input) {
    if (!isRepeating()) {
      activateOnce(context, input);
    } else {
      activateTimed(context, input);
    }
    return input;
  }
};

SHARDS_REGISTER_FN(core) {
  REGISTER_ENUM(CoreInfo2::TypeEnumInfo);

  REGISTER_SHARD("Const", Const);
  REGISTER_CORE_SHARD(Set);
  REGISTER_CORE_SHARD(Ref);
  REGISTER_CORE_SHARD(Update);
  REGISTER_CORE_SHARD(Push);
  REGISTER_CORE_SHARD(Sequence);
  REGISTER_CORE_SHARD(Clear);
  REGISTER_CORE_SHARD(Pop);
  REGISTER_CORE_SHARD(PopFront);
  REGISTER_CORE_SHARD(Drop);
  REGISTER_CORE_SHARD(DropFront);
  REGISTER_CORE_SHARD(Count);
  REGISTER_CORE_SHARD(Get);
  REGISTER_CORE_SHARD(Swap);
  REGISTER_CORE_SHARD(And);
  REGISTER_CORE_SHARD(Or);
  REGISTER_CORE_SHARD(Not);
  REGISTER_CORE_SHARD(IsValidNumber);
  REGISTER_CORE_SHARD(Take);
  REGISTER_CORE_SHARD(RTake);
  REGISTER_CORE_SHARD(Slice);
  REGISTER_CORE_SHARD(Limit);
  REGISTER_CORE_SHARD(Repeat);
  REGISTER_CORE_SHARD(Sort);
  REGISTER_CORE_SHARD(Remove);
  REGISTER_CORE_SHARD(Is);
  REGISTER_CORE_SHARD(IsAlmost);
  REGISTER_CORE_SHARD(IsNot);
  REGISTER_CORE_SHARD(IsMore);
  REGISTER_CORE_SHARD(IsLess);
  REGISTER_CORE_SHARD(IsMoreEqual);
  REGISTER_CORE_SHARD(IsLessEqual);
  REGISTER_CORE_SHARD(IsAny);
  REGISTER_CORE_SHARD(IsAll);
  REGISTER_CORE_SHARD(IsAnyNot);
  REGISTER_CORE_SHARD(IsAllNot);
  REGISTER_CORE_SHARD(IsAnyMore);
  REGISTER_CORE_SHARD(IsAllMore);
  REGISTER_CORE_SHARD(IsAnyLess);
  REGISTER_CORE_SHARD(IsAllLess);
  REGISTER_CORE_SHARD(IsAnyMoreEqual);
  REGISTER_CORE_SHARD(IsAllMoreEqual);
  REGISTER_CORE_SHARD(IsAnyLessEqual);
  REGISTER_CORE_SHARD(IsAllLessEqual);

  REGISTER_SHARD("Profile", Profile);

  REGISTER_SHARD("ForEach", ForEachShard);
  REGISTER_SHARD("ForRange", ForRangeShard);
  REGISTER_SHARD("Map", Map);
  REGISTER_SHARD("Reduce", Reduce);
  REGISTER_SHARD("Erase", Erase);
  REGISTER_SHARD("Once", Once);
  REGISTER_SHARD("Table", TableDecl);

  REGISTER_SHARD("Pause", Pause);
  REGISTER_SHARD("PauseMs", PauseMs);

  REGISTER_SHARD("AppendTo", AppendTo);
  REGISTER_SHARD("PrependTo", PrependTo);
  REGISTER_SHARD("Assoc", Assoc);

  using PassMockShard = LambdaShard<unreachableActivation, CoreInfo::AnyType, CoreInfo::AnyType>;
  using ExitShard = LambdaShard<exitProgramActivation, CoreInfo::IntType, CoreInfo::NoneType>;
  REGISTER_SHARD("Pass", PassMockShard);
  REGISTER_SHARD("Exit", ExitShard);

  using HasherShard = LambdaShard<hashActivation, CoreInfo::AnyType, CoreInfo::Int2Type>;
  REGISTER_SHARD("Hash", HasherShard);

  using BlockingSleepShard = LambdaShard<blockingSleepActivation, CoreInfo::AnyType, CoreInfo::AnyType>;
  REGISTER_SHARD("SleepBlocking!", BlockingSleepShard);

  REGISTER_SHARD("Lowest", LowestShard);
  REGISTER_SHARD("Highest", HighestShard);

#ifdef __EMSCRIPTEN__
  using EmscriptenEvalShard = LambdaShard<emscriptenEvalActivation, CoreInfo::StringType, CoreInfo::StringType>;
  // _ prefix = internal shard
  REGISTER_SHARD("_Emscripten.Eval", EmscriptenEvalShard);
  // _ prefix = internal shard
  REGISTER_SHARD("_Emscripten.EvalAsync", EmscriptenAsyncEval);
  using EmscriptenBrowseShard = LambdaShard<emscriptenBrowseActivation, CoreInfo::StringType, CoreInfo::StringType>;
  REGISTER_SHARD("Browse", EmscriptenBrowseShard);
#endif

  REGISTER_SHARD("Return", Return);
  REGISTER_SHARD("Restart", Restart);
  REGISTER_SHARD("Fail", Fail);
  REGISTER_SHARD("NaNTo0", NaNTo0);
  REGISTER_SHARD("IsNone", IsNone);
  REGISTER_SHARD("IsNotNone", IsNotNone);
  REGISTER_SHARD("IsTrue", IsTrue);
  REGISTER_SHARD("IsFalse", IsFalse);
  REGISTER_SHARD("Input", Input);
  REGISTER_SHARD("Comment", Comment);
  REGISTER_SHARD("Replace", Replace);
  REGISTER_SHARD("Reverse", Reverse);
  REGISTER_SHARD("UnsafeActivate!", UnsafeActivate);
  REGISTER_SHARD("Shards.Enumerate", GetShards);
  REGISTER_SHARD("Shards.Help", GetShardHelp);
  REGISTER_SHARD("LastError", LastError);
}
}; // namespace shards
