/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#pragma once

#include "../blocks_macros.hpp"
// circular warning this is self inclusive on purpose
#include "../foundation.hpp"
#include "chainblocks.h"
#include "chainblocks.hpp"
#include <cassert>
#include <cmath>

namespace chainblocks {

enum class BasicTypes {
  Any,
  Bool,
  Int,
  Int2,
  Int3,
  Int4,
  Int8,
  Int16,
  Float,
  Float2,
  Float3,
  Float4,
  Color,
  Chain,
  Block,
  Bytes,
  String,
  Image
};

struct CoreInfo {
  static inline EnumInfo<BasicTypes> BasicTypesEnum{"Type", 'sink', 'type'};
  static inline Type BasicTypesType{
      {CBType::Enum, {.enumeration = {'sink', 'type'}}}};
  static inline Type BasicTypesSeqType{
      {CBType::Seq, {.seqTypes = BasicTypesType}}};
  static inline Types BasicTypesTypes{{BasicTypesType, BasicTypesSeqType}};

  static inline Type NoneType{{CBType::None}};

#define CB_CORE_TYPE_DEF(_cbtype_)                                             \
  static inline Type _cbtype_##Type{{CBType::_cbtype_}};                       \
  static inline Type _cbtype_##SeqType{                                        \
      {CBType::Seq, {.seqTypes = _cbtype_##Type}}};                            \
  static inline Type _cbtype_##TableType{                                      \
      {CBType::Table, {.table = {.types = _cbtype_##Type}}}};                  \
  static inline Type _cbtype_##VarType{                                        \
      {CBType::ContextVar, {.contextVarTypes = _cbtype_##Type}}};              \
  static inline Type _cbtype_##VarSeqType {                                    \
    {                                                                          \
      CBType::ContextVar, { .contextVarTypes = _cbtype_##SeqType }             \
    }                                                                          \
  }

  CB_CORE_TYPE_DEF(Any);
  CB_CORE_TYPE_DEF(Bool);
  CB_CORE_TYPE_DEF(Int);
  CB_CORE_TYPE_DEF(Int2);
  CB_CORE_TYPE_DEF(Int3);
  CB_CORE_TYPE_DEF(Int4);
  CB_CORE_TYPE_DEF(Int8);
  CB_CORE_TYPE_DEF(Int16);
  CB_CORE_TYPE_DEF(Float);
  CB_CORE_TYPE_DEF(Float2);
  CB_CORE_TYPE_DEF(Float3);
  CB_CORE_TYPE_DEF(Float4);
  CB_CORE_TYPE_DEF(Color);
  CB_CORE_TYPE_DEF(Chain);
  CB_CORE_TYPE_DEF(Block);
  CB_CORE_TYPE_DEF(Bytes);
  CB_CORE_TYPE_DEF(String);
  CB_CORE_TYPE_DEF(Image);

  static inline Types IntOrFloat{{IntType, FloatType}};

  static inline Types NoneIntOrFloat{{NoneType, IntType, FloatType}};

  static inline Types Indexables{{Int2Type, Int3Type, Int4Type, Int8Type,
                                  Int16Type, Float2Type, Float3Type, Float4Type,
                                  BytesType, ColorType, StringType, AnySeqType,
                                  AnyTableType}};

  static inline Types RIndexables{{BytesType, StringType, AnySeqType}};

  static inline Types FloatVectors{{
      Float2Type,
      Float2SeqType,
      Float3Type,
      Float3SeqType,
      Float4Type,
      Float4SeqType,
  }};
  static inline Types FloatVectorsOrVar{FloatVectors,
                                        {
                                            Float2VarType,
                                            Float2VarSeqType,
                                            Float3VarType,
                                            Float3VarSeqType,
                                            Float4VarType,
                                            Float4VarSeqType,
                                        }};

  static inline Types IntOrNone{{IntType, NoneType}};

  static inline Types IntsVar{{IntType, IntSeqType, IntVarType, IntVarSeqType}};

  static inline Types TakeTypes{{IntType, IntSeqType, IntVarType, IntVarSeqType,
                                 StringType, StringSeqType, StringVarType,
                                 StringVarSeqType}};

  static inline Types RTakeTypes{
      {IntType, IntSeqType, IntVarType, IntVarSeqType}};

  static inline Types IntsVarOrNone{IntsVar, {NoneType}};

  static inline Types IntIntSeqOrNone{{IntType, IntSeqType, NoneType}};

  static inline Types BlockSeqOrNone{{BlockSeqType, NoneType}};

  static inline Types Blocks{{BlockType, BlockSeqType}};

  static inline Types BlocksOrNone{Blocks, {NoneType}};

  static inline Type BlocksOrNoneSeq{{CBType::Seq, {.seqTypes = BlocksOrNone}}};

  static inline Types StringOrBytes{{StringType, BytesType}};

  static inline Types StringOrNone{{StringType, NoneType}};

  static inline Types StringOrStringVar{{StringType, StringVarType}};

  static inline Types StringOrAnyVar{{StringType, AnyVarType}};

  static inline Types ColorOrNone{{ColorType, NoneType}};

  static inline Types AnyNumbers{{
      IntType,       IntSeqType,   Int2Type,      Int2SeqType,  Int3Type,
      Int3SeqType,   Int4Type,     Int4SeqType,   Int8Type,     Int8SeqType,
      Int16Type,     Int16SeqType, FloatType,     FloatSeqType, Float2Type,
      Float2SeqType, Float3Type,   Float3SeqType, Float4Type,   Float4SeqType,
      ColorType,     ColorSeqType,
  }};

  static inline Types StringOrBytesVarOrNone{
      {StringVarType, BytesVarType, NoneType}};

  static inline Types StringStringVarOrNone{
      {StringType, StringVarType, NoneType}};

  static inline Types IntOrIntVar{{IntType, IntVarType}};
};

struct Const {
  static inline ParamsInfo constParamsInfo = ParamsInfo(
      ParamsInfo::Param("Value", "The constant value to insert in the chain.",
                        CoreInfo::AnyType));

  CBVar _value{};
  CBTypeInfo _innerInfo{};

  void destroy() {
    destroyVar(_value);
    freeDerivedInfo(_innerInfo);
  }

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static CBParametersInfo parameters() {
    return CBParametersInfo(constParamsInfo);
  }

  void setParam(int index, CBVar value) { cloneVar(_value, value); }

  CBVar getParam(int index) { return _value; }

  CBTypeInfo compose(const CBInstanceData &data) {
    freeDerivedInfo(_innerInfo);
    _innerInfo = deriveTypeInfo(_value);
    return _innerInfo;
  }

  CBVar activate(CBContext *context, const CBVar &input) { return _value; }
};

static ParamsInfo compareParamsInfo = ParamsInfo(ParamsInfo::Param(
    "Value", "The value to test against for equality.", CoreInfo::AnyType));

struct BaseOpsBin {
  CBVar _value{};
  CBVar *_target = nullptr;

  void destroy() { destroyVar(_value); }

  void cleanup() {
    if (_target && _value.valueType == ContextVar) {
      releaseVariable(_target);
    }
    _target = nullptr;
  }

  CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  CBTypesInfo outputTypes() { return CoreInfo::BoolType; }

  CBParametersInfo parameters() { return CBParametersInfo(compareParamsInfo); }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      cloneVar(_value, value);
      cleanup();
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _value;
    default:
      return Var::Empty;
    }
  }

  void warmup(CBContext *context) {
    _target = _value.valueType == ContextVar
                  ? referenceVariable(context, _value.payload.stringValue)
                  : &_value;
  }
};

#define LOGIC_OP(NAME, OP)                                                     \
  struct NAME : public BaseOpsBin {                                            \
    FLATTEN ALWAYS_INLINE CBVar activate(CBContext *context,                   \
                                         const CBVar &input) {                 \
      const auto &value = *_target;                                            \
      if (input OP value) {                                                    \
        return Var::True;                                                      \
      }                                                                        \
      return Var::False;                                                       \
    }                                                                          \
  };                                                                           \
  RUNTIME_CORE_BLOCK_TYPE(NAME);

LOGIC_OP(Is, ==);
LOGIC_OP(IsNot, !=);
LOGIC_OP(IsMore, >);
LOGIC_OP(IsLess, <);
LOGIC_OP(IsMoreEqual, >=);
LOGIC_OP(IsLessEqual, <=);

#define LOGIC_ANY_SEQ_OP(NAME, OP)                                             \
  struct NAME : public BaseOpsBin {                                            \
    CBVar activate(CBContext *context, const CBVar &input) {                   \
      const auto &value = *_target;                                            \
      if (input.valueType == Seq && value.valueType == Seq) {                  \
        auto vlen = value.payload.seqValue.len;                                \
        auto ilen = input.payload.seqValue.len;                                \
        if (ilen > vlen)                                                       \
          throw ActivationError("Failed to compare, input len > value len.");  \
        for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {            \
          if (input.payload.seqValue.elements[i] OP value.payload.seqValue     \
                  .elements[i]) {                                              \
            return Var::True;                                                  \
          }                                                                    \
        }                                                                      \
        return Var::False;                                                     \
      } else if (input.valueType == Seq && value.valueType != Seq) {           \
        for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {            \
          if (input.payload.seqValue.elements[i] OP value) {                   \
            return Var::True;                                                  \
          }                                                                    \
        }                                                                      \
        return Var::False;                                                     \
      } else if (input.valueType != Seq && value.valueType == Seq) {           \
        for (uint32_t i = 0; i < value.payload.seqValue.len; i++) {            \
          if (input OP value.payload.seqValue.elements[i]) {                   \
            return Var::True;                                                  \
          }                                                                    \
        }                                                                      \
        return Var::False;                                                     \
      } else if (input.valueType != Seq && value.valueType != Seq) {           \
        if (input OP value) {                                                  \
          return Var::True;                                                    \
        }                                                                      \
        return Var::False;                                                     \
      }                                                                        \
      return Var::False;                                                       \
    }                                                                          \
  };                                                                           \
  RUNTIME_CORE_BLOCK_TYPE(NAME);

#define LOGIC_ALL_SEQ_OP(NAME, OP)                                             \
  struct NAME : public BaseOpsBin {                                            \
    CBVar activate(CBContext *context, const CBVar &input) {                   \
      const auto &value = *_target;                                            \
      if (input.valueType == Seq && value.valueType == Seq) {                  \
        auto vlen = value.payload.seqValue.len;                                \
        auto ilen = input.payload.seqValue.len;                                \
        if (ilen > vlen)                                                       \
          throw ActivationError("Failed to compare, input len > value len.");  \
        for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {            \
          if (!(input.payload.seqValue.elements[i] OP value.payload.seqValue   \
                    .elements[i])) {                                           \
            return Var::False;                                                 \
          }                                                                    \
        }                                                                      \
        return Var::True;                                                      \
      } else if (input.valueType == Seq && value.valueType != Seq) {           \
        for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {            \
          if (!(input.payload.seqValue.elements[i] OP value)) {                \
            return Var::False;                                                 \
          }                                                                    \
        }                                                                      \
        return Var::True;                                                      \
      } else if (input.valueType != Seq && value.valueType == Seq) {           \
        for (uint32_t i = 0; i < value.payload.seqValue.len; i++) {            \
          if (!(input OP value.payload.seqValue.elements[i])) {                \
            return Var::False;                                                 \
          }                                                                    \
        }                                                                      \
        return Var::True;                                                      \
      } else if (input.valueType != Seq && value.valueType != Seq) {           \
        if (!(input OP value)) {                                               \
          return Var::False;                                                   \
        }                                                                      \
        return Var::True;                                                      \
      }                                                                        \
      return Var::False;                                                       \
    }                                                                          \
  };                                                                           \
  RUNTIME_CORE_BLOCK_TYPE(NAME);

LOGIC_ANY_SEQ_OP(Any, ==);
LOGIC_ALL_SEQ_OP(All, ==);
LOGIC_ANY_SEQ_OP(AnyNot, !=);
LOGIC_ALL_SEQ_OP(AllNot, !=);
LOGIC_ANY_SEQ_OP(AnyMore, >);
LOGIC_ALL_SEQ_OP(AllMore, >);
LOGIC_ANY_SEQ_OP(AnyLess, <);
LOGIC_ALL_SEQ_OP(AllLess, <);
LOGIC_ANY_SEQ_OP(AnyMoreEqual, >=);
LOGIC_ALL_SEQ_OP(AllMoreEqual, >=);
LOGIC_ANY_SEQ_OP(AnyLessEqual, <=);
LOGIC_ALL_SEQ_OP(AllLessEqual, <=);

#define LOGIC_OP_DESC(NAME)                                                    \
  RUNTIME_CORE_BLOCK_FACTORY(NAME);                                            \
  RUNTIME_BLOCK_destroy(NAME);                                                 \
  RUNTIME_BLOCK_cleanup(NAME);                                                 \
  RUNTIME_BLOCK_warmup(NAME);                                                  \
  RUNTIME_BLOCK_inputTypes(NAME);                                              \
  RUNTIME_BLOCK_outputTypes(NAME);                                             \
  RUNTIME_BLOCK_parameters(NAME);                                              \
  RUNTIME_BLOCK_setParam(NAME);                                                \
  RUNTIME_BLOCK_getParam(NAME);                                                \
  RUNTIME_BLOCK_activate(NAME);                                                \
  RUNTIME_BLOCK_END(NAME);

struct Input {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  FLATTEN ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    context->rebaseFlow();
    return input;
  }
};

struct SetInput {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    chainblocks::cloneVar(const_cast<CBChain *>(context->main)->rootTickInput,
                          input);
    return input;
  }
};

struct Pause {
  ExposedInfo reqs{};
  static inline Parameters params{
      {"Time",
       "The amount of time in seconds (float) to pause this chain.",
       {CoreInfo::NoneType, CoreInfo::FloatType}}};

  ParamVar time{};

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, CBVar value) { time = value; }

  CBVar getParam(int index) { return time; }

  void warmup(CBContext *context) { time.warmup(context); }

  void cleanup() { time.cleanup(); }

  CBExposedTypesInfo requiredVariables() {
    if (time.isVariable()) {
      reqs = ExposedInfo(ExposedInfo::Variable(
          time.variableName(), "The required variable", CoreInfo::FloatType));
    } else {
      reqs = {};
    }
    return CBExposedTypesInfo(reqs);
  }

  FLATTEN ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    const auto &t = time.get();
    if (t.valueType == None)
      suspend(context, 0.0);
    else
      suspend(context, t.payload.floatValue);
    return input;
  }
};

struct PauseMs : public Pause {
  static inline Parameters params{
      {"Time",
       "The amount of time in milliseconds to pause this chain.",
       {CoreInfo::NoneType, CoreInfo::IntType}}};

  static CBParametersInfo parameters() { return params; }

  CBExposedTypesInfo requiredVariables() {
    if (time.isVariable()) {
      reqs = ExposedInfo(ExposedInfo::Variable(
          time.variableName(), "The required variable", CoreInfo::IntType));
    } else {
      reqs = {};
    }
    return CBExposedTypesInfo(reqs);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    const auto &t = time.get();
    if (t.valueType == None)
      suspend(context, 0.0);
    else
      suspend(context, t.payload.intValue);
    return input;
  }
};

struct And {
  static CBTypesInfo inputTypes() { return CoreInfo::BoolType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }
  CBVar activate(CBContext *context, const CBVar &input) {
    if (input.payload.boolValue) {
      // Continue the flow
      context->rebaseFlow();
      return input;
    } else {
      // Reason: We are done, input IS FALSE so we stop this flow
      context->returnFlow(input);
      return input;
    }
  }
};

struct Or {
  static CBTypesInfo inputTypes() { return CoreInfo::BoolType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }
  CBVar activate(CBContext *context, const CBVar &input) {
    if (input.payload.boolValue) {
      // Reason: We are done, input IS TRUE so we succeed
      context->returnFlow(input);
      return input;
    } else {
      // Continue the flow, with the initial input as next input!
      context->rebaseFlow();
      return input;
    }
  }
};

struct Not {
  static CBTypesInfo inputTypes() { return CoreInfo::BoolType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }
  CBVar activate(CBContext *context, const CBVar &input) {
    return Var(!input.payload.boolValue);
  }
};

struct Stop {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::NoneType; }
  CBVar activate(CBContext *context, const CBVar &input) {
    context->stopFlow(input);
    return input;
  }
};

struct Restart {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::NoneType; }
  CBVar activate(CBContext *context, const CBVar &input) {
    context->restartFlow(input);
    return input;
  }
};

struct Return {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::NoneType; }
  CBVar activate(CBContext *context, const CBVar &input) {
    context->returnFlow(input);
    return input;
  }
};

struct IsValidNumber {
  static CBTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }
  CBVar activate(CBContext *context, const CBVar &input) {
    return Var(std::isnormal(input.payload.floatValue));
  }
};

struct VariableBase {
  CBVar *_target = nullptr;
  CBVar *_cell = nullptr;
  std::string _name;
  std::string _key;
  ExposedInfo _exposedInfo{};
  bool _isTable = false;
  bool _global = false;

  static inline ParamsInfo variableParamsInfo = ParamsInfo(
      ParamsInfo::Param("Name", "The name of the variable.",
                        CoreInfo::StringOrAnyVar),
      ParamsInfo::Param("Key",
                        "The key of the value to read/write from/in the table "
                        "(this variable will become a table).",
                        CoreInfo::StringType),
      ParamsInfo::Param("Global",
                        "If the variable is or should be available to all "
                        "of the chains in the same node.",
                        CoreInfo::BoolType));

  static CBParametersInfo parameters() {
    return CBParametersInfo(variableParamsInfo);
  }

  void cleanup() {
    if (_target) {
      releaseVariable(_target);
    }
    _target = nullptr;
    _cell = nullptr;
  }

  void setParam(int index, CBVar value) {
    if (index == 0) {
      _name = value.payload.stringValue;
    } else if (index == 1) {
      _key = value.payload.stringValue;
      if (_key.size() > 0)
        _isTable = true;
      else
        _isTable = false;
    } else {
      _global = value.payload.boolValue;
    }
    cleanup();
  }

  CBVar getParam(int index) {
    if (index == 0)
      return Var(_name.c_str());
    else if (index == 1)
      return Var(_key.c_str());
    else
      return Var(_global);
  }
};

struct SetBase : public VariableBase {
  Type _tableTypeInfo{};
  CBString _tableContentKey{};
  CBTypeInfo _tableContentInfo{};

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  void sanityChecks(const CBInstanceData &data, bool warnIfExists) {
    for (uint32_t i = 0; i < data.shared.len; i++) {
      auto &reference = data.shared.elements[i];
      if (strcmp(reference.name, _name.c_str()) == 0) {
        if (_isTable && !reference.isTableEntry) {
          throw CBException("Set/Ref/Update, variable was not a table: " +
                            _name);
        } else if (!_isTable && reference.isTableEntry) {
          throw CBException("Set/Ref/Update, variable was a table: " + _name);
        } else if (!_isTable && data.inputType != reference.exposedType) {
          throw CBException("Set/Ref/Update, variable already set as another "
                            "type: " +
                            _name);
        }
        if (!_isTable && warnIfExists) {
          LOG(INFO)
              << "Set/Ref - Warning: setting an already exposed variable, "
                 "use Update to avoid this warning, variable: "
              << _name;
        }
        if (!reference.isMutable) {
          throw CBException(
              "Set/Ref/Update, attempted to write a immutable variable.");
        }
      }
    }
  }

  void warmup(CBContext *context) {
    if (_global)
      _target = referenceGlobalVariable(context, _name.c_str());
    else
      _target = referenceVariable(context, _name.c_str());
  }

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    if (likely(_cell != nullptr)) {
      cloneVar(*_cell, input);
      return input;
    }

    if (_isTable) {
      if (_target->valueType != Table) {
        if (_target->valueType != None)
          destroyVar(*_target);

        // Not initialized yet
        _target->valueType = Table;
        _target->payload.tableValue.api = &Globals::TableInterface;
        _target->payload.tableValue.opaque = new CBMap();
      }

      CBVar *vptr = _target->payload.tableValue.api->tableAt(
          _target->payload.tableValue, _key.c_str());

      // Clone will try to recyle memory and such
      cloneVar(*vptr, input);

      // use fast cell from now
      _cell = vptr;
    } else {
      // Clone will try to recyle memory and such
      cloneVar(*_target, input);

      // use fast cell from now
      _cell = _target;
    }

    return input;
  }
};

struct Set : public SetBase {
  CBTypeInfo compose(const CBInstanceData &data) {
    sanityChecks(data, true);

    // bake exposed types
    if (_isTable) {
      // we are a table!
      _tableContentInfo = data.inputType;
      _tableContentKey = _key.c_str();
      _tableTypeInfo =
          CBTypeInfo{CBType::Table,
                     {.table = {.keys = {&_tableContentKey, 1, 0},
                                .types = {&_tableContentInfo, 1, 0}}}};
      if (_global) {
        _exposedInfo = ExposedInfo(ExposedInfo::GlobalVariable(
            _name.c_str(), "The exposed table.", _tableTypeInfo, true, true));
      } else {
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(
            _name.c_str(), "The exposed table.", _tableTypeInfo, true, true));
      }
    } else {
      // just a variable!
      if (_global) {
        _exposedInfo = ExposedInfo(
            ExposedInfo::GlobalVariable(_name.c_str(), "The exposed variable.",
                                        CBTypeInfo(data.inputType), true));
      } else {
        _exposedInfo = ExposedInfo(
            ExposedInfo::Variable(_name.c_str(), "The exposed variable.",
                                  CBTypeInfo(data.inputType), true));
      }
    }
    return data.inputType;
  }

  CBExposedTypesInfo exposedVariables() {
    return CBExposedTypesInfo(_exposedInfo);
  }
};

struct Ref : public SetBase {
  CBTypeInfo compose(const CBInstanceData &data) {
    sanityChecks(data, true);

    // bake exposed types
    if (_isTable) {
      // we are a table!
      _tableContentInfo = data.inputType;
      _tableContentKey = _key.c_str();
      _tableTypeInfo =
          CBTypeInfo{CBType::Table,
                     {.table = {.keys = {&_tableContentKey, 1, 0},
                                .types = {&_tableContentInfo, 1, 0}}}};
      _exposedInfo = ExposedInfo(ExposedInfo::Variable(
          _name.c_str(), "The exposed table.", _tableTypeInfo, false, true));
    } else {
      // just a variable!
      _exposedInfo = ExposedInfo(ExposedInfo::Variable(
          _name.c_str(), "The exposed variable.", CBTypeInfo(data.inputType)));
    }
    return data.inputType;
  }

  CBExposedTypesInfo exposedVariables() {
    return CBExposedTypesInfo(_exposedInfo);
  }

  void cleanup() {
    // this is a special case
    // Ref will reference previous block result..
    // And so we are almost sure the memory will be junk after this..
    // so if we detect refcount > 1, we except signaling a dangling reference
    if (_target) {
      if (_target->refcount > 1) {
        throw CBException("Ref - detected a dangling reference.");
      }
      memset(_target, 0x0, sizeof(CBVar));
      _target = nullptr;
    }
    _cell = nullptr;
  }

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    if (likely(_cell != nullptr)) {
      // must keep refcount!
      auto rc = _cell->refcount;
      auto rcflag = _cell->flags & CBVAR_FLAGS_REF_COUNTED;
      memcpy(_cell, &input, sizeof(CBVar));
      _cell->refcount = rc;
      _cell->flags |= rcflag;
      return input;
    }

    if (_isTable) {
      if (_target->valueType != Table) {
        // Not initialized yet
        _target->valueType = Table;
        _target->payload.tableValue.api = &Globals::TableInterface;
        _target->payload.tableValue.opaque = new CBMap();
      }

      CBVar *vptr = _target->payload.tableValue.api->tableAt(
          _target->payload.tableValue, _key.c_str());

      // Notice, NO Cloning!
      *vptr = input;

      // use fast cell from now
      _cell = vptr;
    } else {
      // use fast cell from now
      _cell = _target;

      // Notice, NO Cloning!
      // must keep refcount!
      auto rc = _cell->refcount;
      auto rcflag = _cell->flags & CBVAR_FLAGS_REF_COUNTED;
      memcpy(_cell, &input, sizeof(CBVar));
      _cell->refcount = rc;
      _cell->flags |= rcflag;
    }

    return input;
  }
};

struct Update : public SetBase {
  CBTypeInfo compose(const CBInstanceData &data) {
    sanityChecks(data, false);

    // make sure we update to the same type
    if (_isTable) {
      for (uint32_t i = 0; data.shared.len > i; i++) {
        auto &name = data.shared.elements[i].name;
        if (name == _name &&
            data.shared.elements[i].exposedType.basicType == Table &&
            data.shared.elements[i].exposedType.table.types.elements) {
          auto &tableKeys = data.shared.elements[i].exposedType.table.keys;
          auto &tableTypes = data.shared.elements[i].exposedType.table.types;
          for (uint32_t y = 0; y < tableKeys.len; y++) {
            auto &key = tableKeys.elements[y];
            if (_key == key) {
              if (data.inputType != tableTypes.elements[y]) {
                throw CBException(
                    "Update: error, update is changing the variable type.");
              }
            }
          }
        }
      }
    } else {
      for (uint32_t i = 0; i < data.shared.len; i++) {
        auto &cv = data.shared.elements[i];
        if (_name == cv.name) {
          if (data.inputType != cv.exposedType) {
            throw CBException(
                "Update: error, update is changing the variable type.");
          }
        }
      }
    }

    // bake exposed types
    if (_isTable) {
      // we are a table!
      _tableContentInfo = data.inputType;
      _tableContentKey = _key.c_str();
      _tableTypeInfo =
          CBTypeInfo{CBType::Table,
                     {.table = {.keys = {&_tableContentKey, 1, 0},
                                .types = {&_tableContentInfo, 1, 0}}}};
      _exposedInfo = ExposedInfo(ExposedInfo::Variable(
          _name.c_str(), "The exposed table.", _tableTypeInfo, true, true));
    } else {
      // just a variable!
      _exposedInfo = ExposedInfo(
          ExposedInfo::Variable(_name.c_str(), "The exposed variable.",
                                CBTypeInfo(data.inputType), true));
    }

    return data.inputType;
  }

  CBExposedTypesInfo requiredVariables() {
    return CBExposedTypesInfo(_exposedInfo);
  }
};

struct Get : public VariableBase {
  CBVar _defaultValue{};
  CBTypeInfo _defaultType{};

  static inline ParamsInfo getParamsInfo = ParamsInfo(
      variableParamsInfo,
      ParamsInfo::Param(
          "Default",
          "The default value to use to infer types and output if the variable "
          "is not set, key is not there and/or type mismatches.",
          CoreInfo::AnyType));

  static CBParametersInfo parameters() {
    return CBParametersInfo(getParamsInfo);
  }

  void setParam(int index, CBVar value) {
    if (index <= 2)
      VariableBase::setParam(index, value);
    else if (index == 3) {
      cloneVar(_defaultValue, value);
    }
  }

  CBVar getParam(int index) {
    if (index <= 2)
      return VariableBase::getParam(index);
    else if (index == 3)
      return _defaultValue;
    throw CBException("Param index out of range.");
  }

  void destroy() { freeDerivedInfo(_defaultType); }

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  CBTypeInfo compose(const CBInstanceData &data) {
    if (_isTable) {
      for (uint32_t i = 0; data.shared.len > i; i++) {
        auto &name = data.shared.elements[i].name;
        if (strcmp(name, _name.c_str()) == 0 &&
            data.shared.elements[i].exposedType.basicType == Table) {
          auto &tableKeys = data.shared.elements[i].exposedType.table.keys;
          auto &tableTypes = data.shared.elements[i].exposedType.table.types;
          if (tableKeys.len == tableTypes.len) {
            // if we have a name use it
            for (uint32_t y = 0; y < tableKeys.len; y++) {
              auto &key = tableKeys.elements[y];
              if (strcmp(_key.c_str(), key) == 0) {
                return tableTypes.elements[y];
              }
            }
          } else {
            // we got no key names
            if (tableTypes.len == 1) {
              // 1 type only so we assume we return that type
              return tableTypes.elements[0];
            } else {
              // multiple types...
              // return any, can still be used with ExpectX
              return CoreInfo::AnyType;
            }
          }
        }
      }
      if (_defaultValue.valueType != None) {
        freeDerivedInfo(_defaultType);
        _defaultType = deriveTypeInfo(_defaultValue);
        return _defaultType;
      } else {
        throw ComposeError("Get (" + _name + "/" + _key +
                           "): Could not infer an output type, key not found "
                           "and no Default value provided.");
      }
    } else {
      for (uint32_t i = 0; i < data.shared.len; i++) {
        auto &cv = data.shared.elements[i];
        if (strcmp(_name.c_str(), cv.name) == 0) {
          return cv.exposedType;
        }
      }
    }
    if (_defaultValue.valueType != None) {
      freeDerivedInfo(_defaultType);
      _defaultType = deriveTypeInfo(_defaultValue);
      return _defaultType;
    } else {
      throw ComposeError(
          "Get (" + _name +
          "): Could not infer an output type and no Default value provided.");
    }
  }

  CBExposedTypesInfo requiredVariables() {
    if (_defaultValue.valueType != None) {
      return {};
    } else {
      if (_isTable) {
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(
            _name.c_str(), "The required table.", CoreInfo::AnyTableType));
      } else {
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(
            _name.c_str(), "The required variable.", CoreInfo::AnyType));
      }
      return CBExposedTypesInfo(_exposedInfo);
    }
  }

  bool defaultTypeCheck(const CBVar &value) {
    auto warn = false;
    DEFER({
      if (warn) {
        LOG(INFO)
            << "Get found a variable but it's using the default value because "
               "the type found did not match with the default type.";
      }
    });
    if (value.valueType != _defaultValue.valueType)
      return false;

    if (value.valueType == CBType::Object &&
        (value.payload.objectVendorId != _defaultValue.payload.objectVendorId ||
         value.payload.objectTypeId != _defaultValue.payload.objectTypeId))
      return false;

    if (value.valueType == CBType::Enum &&
        (value.payload.enumVendorId != _defaultValue.payload.enumVendorId ||
         value.payload.enumTypeId != _defaultValue.payload.enumTypeId))
      return false;

    return true;
  }

  void warmup(CBContext *context) {
    if (_global)
      _target = referenceGlobalVariable(context, _name.c_str());
    else
      _target = referenceVariable(context, _name.c_str());
  }

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    if (likely(_cell != nullptr))
      return *_cell;

    // sanity check that the variable is not pristine...
    // the setter chain might have stopped actually!
    if (_defaultValue.valueType == None && _target->refcount == 1) {
      throw ActivationError(
          "Variable: " + _name +
          " did not exist in the context and no default value "
          "was given, likely "
          "the Set block was in chain that already ended.");
    }

    if (_isTable) {
      if (_target->valueType == Table) {
        if (_target->payload.tableValue.api->tableContains(
                _target->payload.tableValue, _key.c_str())) {
          // Has it
          CBVar *vptr = _target->payload.tableValue.api->tableAt(
              _target->payload.tableValue, _key.c_str());

          if (unlikely(_defaultValue.valueType != None &&
                       !defaultTypeCheck(*vptr))) {
            return _defaultValue;
          } else {
            // Pin fast cell
            _cell = vptr;
            return *vptr;
          }
        } else {
          // No record
          if (_defaultType.basicType != None) {
            return _defaultValue;
          } else {
            throw ActivationError("Get - Key not found in table.");
          }
        }
      } else {
        if (_defaultType.basicType != None) {
          return _defaultValue;
        } else {
          throw ActivationError("Get - Table is empty or does not exist yet.");
        }
      }
    } else {
      auto &value = *_target;
      if (unlikely(_defaultValue.valueType != None &&
                   !defaultTypeCheck(value))) {
        return _defaultValue;
      } else {
        // Pin fast cell
        _cell = _target;
        return value;
      }
    }
  }
};

struct Swap {
  static inline ParamsInfo swapParamsInfo =
      ParamsInfo(ParamsInfo::Param("NameA", "The name of first variable.",
                                   CoreInfo::StringType),
                 ParamsInfo::Param("NameB", "The name of second variable.",
                                   CoreInfo::StringType));

  std::string _nameA;
  std::string _nameB;
  CBVar *_targetA{};
  CBVar *_targetB{};
  ExposedInfo _exposedInfo;

  void cleanup() {
    if (_targetA) {
      releaseVariable(_targetA);
      releaseVariable(_targetB);
    }
    _targetA = nullptr;
    _targetB = nullptr;
  }

  void warmup(CBContext *ctx) {
    assert(!_targetA);
    assert(!_targetB);
    _targetA = referenceVariable(ctx, _nameA.c_str());
    _targetB = referenceVariable(ctx, _nameB.c_str());
  }

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  CBExposedTypesInfo requiredVariables() {
    _exposedInfo = ExposedInfo(
        ExposedInfo::Variable(_nameA.c_str(), "The required variable.",
                              CoreInfo::AnyType),
        ExposedInfo::Variable(_nameB.c_str(), "The required variable.",
                              CoreInfo::AnyType));
    return CBExposedTypesInfo(_exposedInfo);
  }

  static CBParametersInfo parameters() {
    return CBParametersInfo(swapParamsInfo);
  }

  void setParam(int index, CBVar value) {
    if (index == 0)
      _nameA = value.payload.stringValue;
    else if (index == 1) {
      _nameB = value.payload.stringValue;
    }
  }

  CBVar getParam(int index) {
    if (index == 0)
      return Var(_nameA.c_str());
    else if (index == 1)
      return Var(_nameB.c_str());
    throw CBException("Param index out of range.");
  }

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    auto tmp = *_targetA;
    *_targetA = *_targetB;
    *_targetB = tmp;
    return input;
  }
};

struct SeqBase : public VariableBase {
  bool _clear = true;
  CBTypeInfo _tableInfo{};

  void initSeq() {
    if (_isTable) {
      if (_target->valueType != Table) {
        // Not initialized yet
        _target->valueType = Table;
        _target->payload.tableValue.api = &Globals::TableInterface;
        _target->payload.tableValue.opaque = new CBMap();
      }

      _cell = _target->payload.tableValue.api->tableAt(
          _target->payload.tableValue, _key.c_str());

      auto &seq = *_cell;
      if (seq.valueType != Seq) {
        seq.valueType = Seq;
        seq.payload.seqValue = {};
      }
    } else {
      if (_target->valueType != Seq) {
        _target->valueType = Seq;
        _target->payload.seqValue = {};
      }
      _cell = _target;
    }

    assert(_cell);
  }

  void warmup(CBContext *context) {
    if (_global)
      _target = referenceGlobalVariable(context, _name.c_str());
    else
      _target = referenceVariable(context, _name.c_str());

    initSeq();
  }

  CBExposedTypesInfo exposedVariables() {
    return CBExposedTypesInfo(_exposedInfo);
  }

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  void destroy() {
    if (_tableInfo.table.keys.elements)
      chainblocks::arrayFree(_tableInfo.table.keys);
    if (_tableInfo.table.types.elements)
      chainblocks::arrayFree(_tableInfo.table.types);
  }
};

struct Push : public SeqBase {
  bool _firstPush = false;
  CBTypeInfo _seqInfo{};
  CBTypeInfo _seqInnerInfo{};

  static inline ParamsInfo pushParams = ParamsInfo(
      variableParamsInfo,
      ParamsInfo::Param(
          "Clear",
          "If we should clear this sequence at every chain iteration; works "
          "only if this is the first push; default: true.",
          CoreInfo::BoolType));

  static CBParametersInfo parameters() { return CBParametersInfo(pushParams); }

  void setParam(int index, CBVar value) {
    if (index <= 2)
      VariableBase::setParam(index, value);
    else if (index == 3) {
      _clear = value.payload.boolValue;
    }
  }

  CBVar getParam(int index) {
    if (index <= 2)
      return VariableBase::getParam(index);
    else if (index == 3)
      return Var(_clear);
    throw CBException("Param index out of range.");
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    const auto updateSeqInfo = [this, &data] {
      _seqInfo.basicType = Seq;
      _seqInnerInfo = data.inputType;
      _seqInfo.seqTypes = {&_seqInnerInfo, 1, 0};
      if (_global) {
        _exposedInfo = ExposedInfo(ExposedInfo::GlobalVariable(
            _name.c_str(), "The exposed sequence.", _seqInfo, true));
      } else {
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(
            _name.c_str(), "The exposed sequence.", _seqInfo, true));
      }
    };

    const auto updateTableInfo = [this, &data] {
      _tableInfo.basicType = Table;
      if (_tableInfo.table.types.elements) {
        chainblocks::arrayFree(_tableInfo.table.types);
      }
      if (_tableInfo.table.keys.elements) {
        chainblocks::arrayFree(_tableInfo.table.keys);
      }
      _seqInfo.basicType = Seq;
      _seqInnerInfo = data.inputType;
      _seqInfo.seqTypes = {&_seqInnerInfo, 1, 0};
      chainblocks::arrayPush(_tableInfo.table.types, _seqInfo);
      chainblocks::arrayPush(_tableInfo.table.keys, _key.c_str());
      if (_global) {
        _exposedInfo = ExposedInfo(ExposedInfo::GlobalVariable(
            _name.c_str(), "The exposed table.", CBTypeInfo(_tableInfo), true));
      } else {
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(
            _name.c_str(), "The exposed table.", CBTypeInfo(_tableInfo), true));
      }
    };

    if (_isTable) {
      for (uint32_t i = 0; data.shared.len > i; i++) {
        if (data.shared.elements[i].name == _name &&
            data.shared.elements[i].exposedType.table.types.elements) {
          auto &tableKeys = data.shared.elements[i].exposedType.table.keys;
          auto &tableTypes = data.shared.elements[i].exposedType.table.types;
          for (uint32_t y = 0; y < tableKeys.len; y++) {
            if (_key == tableKeys.elements[y] &&
                tableTypes.elements[y].basicType == Seq) {
              updateTableInfo();
              return data.inputType; // found lets escape
            }
          }
        }
      }
      // not found
      // implicitly initialize anyway
      updateTableInfo();
      _firstPush = true;
    } else {
      for (uint32_t i = 0; i < data.shared.len; i++) {
        auto &cv = data.shared.elements[i];
        if (_name == cv.name && cv.exposedType.basicType == Seq) {
          // found, let's just update our info
          updateSeqInfo();
          return data.inputType; // found lets escape
        }
      }
      // not found
      // implicitly initialize anyway
      updateSeqInfo();
      _firstPush = true;
    }
    return data.inputType;
  }

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    assert(_cell);
    if (_clear && _firstPush) {
      chainblocks::arrayResize(_cell->payload.seqValue, 0);
    }
    const auto len = _cell->payload.seqValue.len;
    chainblocks::arrayResize(_cell->payload.seqValue, len + 1);
    cloneVar(_cell->payload.seqValue.elements[len], input);
    return input;
  }
};

struct Sequence : public SeqBase {
  ParamVar _types{Var::Enum(BasicTypes::Any, 'sink', 'type')};
  Types _seqTypes{};
  std::deque<Types> _innerTypes;

  static inline ParamsInfo pushParams = ParamsInfo(
      variableParamsInfo,
      ParamsInfo::Param(
          "Clear",
          "If we should clear this sequence at every chain iteration; works "
          "only if this is the first push; default: true.",
          CoreInfo::BoolType),
      ParamsInfo::Param("Types", "The sequence inner types to forward declare.",
                        CoreInfo::BasicTypesTypes));

  static CBParametersInfo parameters() { return CBParametersInfo(pushParams); }

  void destroy() {
    if (_tableInfo.table.keys.elements)
      chainblocks::arrayFree(_tableInfo.table.keys);
    if (_tableInfo.table.types.elements)
      chainblocks::arrayFree(_tableInfo.table.types);
  }

  void setParam(int index, CBVar value) {
    if (index <= 2)
      VariableBase::setParam(index, value);
    else if (index == 3) {
      _clear = value.payload.boolValue;
    } else if (index == 4) {
      _types = value;
    }
  }

  CBVar getParam(int index) {
    if (index <= 2)
      return VariableBase::getParam(index);
    else if (index == 3)
      return Var(_clear);
    else if (index == 4)
      return _types;
    throw CBException("Param index out of range.");
  }

  void addType(Types &inner, BasicTypes type) {
    switch (type) {
    case BasicTypes::Any:
      inner._types.emplace_back(CoreInfo::AnyType);
      break;
    case BasicTypes::Bool:
      inner._types.emplace_back(CoreInfo::BoolType);
      break;
    case BasicTypes::Int:
      inner._types.emplace_back(CoreInfo::IntType);
      break;
    case BasicTypes::Int2:
      inner._types.emplace_back(CoreInfo::Int2Type);
      break;
    case BasicTypes::Int3:
      inner._types.emplace_back(CoreInfo::Int3Type);
      break;
    case BasicTypes::Int4:
      inner._types.emplace_back(CoreInfo::Int4Type);
      break;
    case BasicTypes::Int8:
      inner._types.emplace_back(CoreInfo::Int8Type);
      break;
    case BasicTypes::Int16:
      inner._types.emplace_back(CoreInfo::Int16Type);
      break;
    case BasicTypes::Float:
      inner._types.emplace_back(CoreInfo::FloatType);
      break;
    case BasicTypes::Float2:
      inner._types.emplace_back(CoreInfo::Float2Type);
      break;
    case BasicTypes::Float3:
      inner._types.emplace_back(CoreInfo::Float3Type);
      break;
    case BasicTypes::Float4:
      inner._types.emplace_back(CoreInfo::Float4Type);
      break;
    case BasicTypes::Color:
      inner._types.emplace_back(CoreInfo::ColorType);
      break;
    case BasicTypes::Chain:
      inner._types.emplace_back(CoreInfo::ChainType);
      break;
    case BasicTypes::Block:
      inner._types.emplace_back(CoreInfo::BlockType);
      break;
    case BasicTypes::Bytes:
      inner._types.emplace_back(CoreInfo::BytesType);
      break;
    case BasicTypes::String:
      inner._types.emplace_back(CoreInfo::StringType);
      break;
    case BasicTypes::Image:
      inner._types.emplace_back(CoreInfo::ImageType);
      break;
    }
  }

  void processTypes(Types &inner, const IterableSeq &s) {
    for (auto &v : s) {
      if (v.valueType == Seq) {
        auto &sinner = _innerTypes.emplace_back();
        IterableSeq ss(v);
        processTypes(sinner, ss);
        CBTypeInfo stype{CBType::Seq, {.seqTypes = sinner}};
        inner._types.emplace_back(stype);
      } else {
        const auto type = BasicTypes(v.payload.enumValue);
        // assume enum
        addType(inner, type);
      }
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    const auto updateTableInfo = [this] {
      _tableInfo.basicType = Table;
      if (_tableInfo.table.types.elements) {
        chainblocks::arrayFree(_tableInfo.table.types);
      }
      if (_tableInfo.table.keys.elements) {
        chainblocks::arrayFree(_tableInfo.table.keys);
      }
      CBTypeInfo stype{CBType::Seq, {.seqTypes = _seqTypes}};
      chainblocks::arrayPush(_tableInfo.table.types, stype);
      chainblocks::arrayPush(_tableInfo.table.keys, _key.c_str());
      if (_global) {
        _exposedInfo = ExposedInfo(ExposedInfo::GlobalVariable(
            _name.c_str(), "The exposed table.", CBTypeInfo(_tableInfo), true));
      } else {
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(
            _name.c_str(), "The exposed table.", CBTypeInfo(_tableInfo), true));
      }
    };

    // Ensure variable did not exist
    if (!_isTable) {
      for (uint32_t i = 0; i < data.shared.len; i++) {
        auto &reference = data.shared.elements[i];
        if (strcmp(reference.name, _name.c_str()) == 0) {
          throw ComposeError("Sequence - Variable " + _name +
                             " already exists.");
        }
      }
    } else {
      for (uint32_t i = 0; data.shared.len > i; i++) {
        if (data.shared.elements[i].name == _name &&
            data.shared.elements[i].exposedType.table.types.elements) {
          auto &tableKeys = data.shared.elements[i].exposedType.table.keys;
          for (uint32_t y = 0; y < tableKeys.len; y++) {
            if (_key == tableKeys.elements[y]) {
              throw ComposeError("Sequence - Variable " + _key + " in table " +
                                 _name + " already exists.");
            }
          }
        }
      }
    }

    // Process types to expose
    // cleaning up previous first
    _seqTypes._types.clear();
    _innerTypes.clear();
    if (_types->valueType == Enum) {
      // a single type
      addType(_seqTypes, BasicTypes(_types->payload.enumValue));
    } else {
      IterableSeq st(_types);
      processTypes(_seqTypes, st);
    }

    if (!_isTable) {
      CBTypeInfo stype{CBType::Seq, {.seqTypes = _seqTypes}};
      if (_global) {
        _exposedInfo = ExposedInfo(ExposedInfo::GlobalVariable(
            _name.c_str(), "The exposed sequence.", stype, true));
      } else {
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(
            _name.c_str(), "The exposed sequence.", stype, true));
      }
    } else {
      updateTableInfo();
    }

    return data.inputType;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    assert(_cell);
    if (_clear) {
      auto &seq = *_cell;
      chainblocks::arrayResize(seq.payload.seqValue, 0);
    }
    return input;
  }
};

struct SeqUser : VariableBase {
  bool _blittable = false;

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  void initSeq() {
    if (_isTable) {
      if (_target->valueType != Table) {
        // We need to init this in order to fetch cell addr
        // Not initialized yet
        _target->valueType = Table;
        _target->payload.tableValue.api = &Globals::TableInterface;
        _target->payload.tableValue.opaque = new CBMap();
      }

      _cell = _target->payload.tableValue.api->tableAt(
          _target->payload.tableValue, _key.c_str());
    } else {
      _cell = _target;
    }

    assert(_cell);
  }

  void warmup(CBContext *context) {
    if (_global)
      _target = referenceGlobalVariable(context, _name.c_str());
    else
      _target = referenceVariable(context, _name.c_str());

    initSeq();
  }

  CBExposedTypesInfo requiredVariables() {
    if (_name.size() > 0) {
      if (_isTable) {
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(
            _name.c_str(), "The required table.", CoreInfo::AnyTableType));
      } else {
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(
            _name.c_str(), "The required variable.", CoreInfo::AnyType));
      }
      return CBExposedTypesInfo(_exposedInfo);
    } else {
      return {};
    }
  }
};

struct Count : SeqUser {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::IntType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (likely(_cell->valueType == Seq)) {
      return Var(int64_t(_cell->payload.seqValue.len));
    } else if (_cell->valueType == Table) {
      return Var(int64_t(
          _cell->payload.tableValue.api->tableSize(_cell->payload.tableValue)));
    } else if (_cell->valueType == Bytes) {
      return Var(int64_t(_cell->payload.bytesSize));
    } else if (_cell->valueType == String) {
      return Var(int64_t(_cell->payload.stringLen > 0
                             ? _cell->payload.stringLen
                             : strlen(_cell->payload.stringValue)));
    } else {
      return Var(0);
    }
  }
};

struct Clear : SeqUser {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (likely(_cell->valueType == Seq)) {
      // notice this is fine because destroyVar will destroy .cap later
      // so we make sure we are not leaking Vars
      chainblocks::arrayResize(_cell->payload.seqValue, 0);

      // sometimes we might have as input the same _cell!
      // this is kind of a hack but helps UX
      // we in that case output the same _cell with adjusted len!
      if (input.payload.seqValue.elements == _cell->payload.seqValue.elements)
        const_cast<CBVar &>(input).payload.seqValue.len = 0;
    } else if (_cell->valueType == Table) {
      _cell->payload.tableValue.api->tableClear(_cell->payload.tableValue);
    }

    return input;
  }
};

struct Drop : SeqUser {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (likely(_cell->valueType == Seq)) {
      auto len = _cell->payload.seqValue.len;
      // notice this is fine because destroyVar will destroy .cap later
      // so we make sure we are not leaking Vars
      if (len > 0) {
        chainblocks::arrayResize(_cell->payload.seqValue, len - 1);
      }

      // sometimes we might have as input the same _cell!
      // this is kind of a hack but helps UX
      // we in that case output the same _cell with adjusted len!
      if (input.payload.seqValue.elements == _cell->payload.seqValue.elements)
        const_cast<CBVar &>(input).payload.seqValue.len = len - 1;
    } else {
      throw ActivationError("Variable is not a sequence, failed to Drop.");
    }

    return input;
  }
};

struct DropFront : SeqUser {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (likely(_cell->valueType == Seq) && _cell->payload.seqValue.len > 0) {
      auto &arr = _cell->payload.seqValue;
      chainblocks::arrayDel(arr, 0);
      // sometimes we might have as input the same _cell!
      // this is kind of a hack but helps UX
      // we in that case output the same _cell with adjusted len!
      if (input.payload.seqValue.elements == arr.elements)
        const_cast<CBVar &>(input).payload.seqValue.len = arr.len;
    } else {
      throw ActivationError("Variable is not a sequence, failed to DropFront.");
    }

    return input;
  }
};

struct Pop : SeqUser {
  // TODO refactor like Push

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  CBVar _output{};

  void destroy() { destroyVar(_output); }

  CBTypeInfo compose(const CBInstanceData &data) {
    if (_isTable) {
      for (uint32_t i = 0; data.shared.len > i; i++) {
        if (data.shared.elements[i].name == _name &&
            data.shared.elements[i].exposedType.table.types.elements) {
          auto &tableKeys = data.shared.elements[i].exposedType.table.keys;
          auto &tableTypes = data.shared.elements[i].exposedType.table.types;
          for (uint32_t y = 0; y < tableKeys.len; y++) {
            if (_key == tableKeys.elements[y] &&
                tableTypes.elements[y].basicType == Seq) {
              // if we have 1 type we can predict the output
              // with more just make us a any seq, will need ExpectX blocks
              // likely
              if (tableTypes.elements[y].seqTypes.len == 1)
                return tableTypes.elements[y].seqTypes.elements[0];
              else
                return CoreInfo::AnySeqType;
            }
          }
        }
      }
      throw CBException("Pop: key not found or key value is not a sequence!.");
    } else {
      for (uint32_t i = 0; i < data.shared.len; i++) {
        auto &cv = data.shared.elements[i];
        if (_name == cv.name && cv.exposedType.basicType == Seq) {
          // if we have 1 type we can predict the output
          // with more just make us a any seq, will need ExpectX blocks likely
          if (cv.exposedType.seqTypes.len == 1)
            return cv.exposedType.seqTypes.elements[0];
          else
            return CoreInfo::AnySeqType;
        }
      }
    }
    throw CBException("Variable is not a sequence.");
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (_cell->valueType != Seq) {
      throw ActivationError("Variable is not a sequence, failed to Pop.");
    }

    if (_cell->payload.seqValue.len == 0) {
      throw ActivationError("Pop: sequence was empty.");
    }

    // Clone
    auto pops = chainblocks::arrayPop<CBSeq, CBVar>(_cell->payload.seqValue);
    cloneVar(_output, pops);
    return _output;
  }
};

struct PopFront : SeqUser {
  // TODO refactor like push

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  CBVar _output{};

  void destroy() { destroyVar(_output); }

  CBTypeInfo compose(const CBInstanceData &data) {
    if (_isTable) {
      for (uint32_t i = 0; data.shared.len > i; i++) {
        if (data.shared.elements[i].name == _name &&
            data.shared.elements[i].exposedType.table.types.elements) {
          auto &tableKeys = data.shared.elements[i].exposedType.table.keys;
          auto &tableTypes = data.shared.elements[i].exposedType.table.types;
          for (uint32_t y = 0; y < tableKeys.len; y++) {
            if (_key == tableKeys.elements[y] &&
                tableTypes.elements[y].basicType == Seq) {
              // if we have 1 type we can predict the output
              // with more just make us a any seq, will need ExpectX blocks
              // likely
              if (tableTypes.elements[y].seqTypes.len == 1)
                return tableTypes.elements[y].seqTypes.elements[0];
              else
                return CoreInfo::AnySeqType;
            }
          }
        }
      }
      throw CBException("Pop: key not found or key value is not a sequence!.");
    } else {
      for (uint32_t i = 0; i < data.shared.len; i++) {
        auto &cv = data.shared.elements[i];
        if (_name == cv.name && cv.exposedType.basicType == Seq) {
          // if we have 1 type we can predict the output
          // with more just make us a any seq, will need ExpectX blocks likely
          if (cv.exposedType.seqTypes.len == 1)
            return cv.exposedType.seqTypes.elements[0];
          else
            return CoreInfo::AnySeqType;
        }
      }
    }
    throw CBException("Variable is not a sequence.");
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (_cell->valueType != Seq) {
      throw ActivationError("Variable is not a sequence, failed to Pop.");
    }

    if (_cell->payload.seqValue.len == 0) {
      throw ActivationError("Pop: sequence was empty.");
    }

    auto &arr = _cell->payload.seqValue;
    const auto len = arr.len - 1;
    // store to put back at end
    // we do this to allow further grows
    // to recycle this _cell (well if not blittable that is)
    auto first = arr.elements[0];
    static_assert(sizeof(*arr.elements) == sizeof(CBVar),
                  "Wrong seq elements size!");
    // shift backward current elements
    memmove(&arr.elements[0], &arr.elements[1], sizeof(*arr.elements) * len);
    // put first at end
    arr.elements[len] = first;
    // resize, will cut first out too
    chainblocks::arrayResize(arr, len);

    cloneVar(_output, first);
    return _output;
  }
};

struct Take {
  static inline ParamsInfo indicesParamsInfo = ParamsInfo(ParamsInfo::Param(
      "Indices",
      "One or multiple indices/keys to extract from a sequence/table.",
      CoreInfo::TakeTypes));

  CBSeq _cachedSeq{};
  CBVar _output{};
  CBVar _indices{};
  CBVar *_indicesVar = nullptr;
  ExposedInfo _exposedInfo{};
  bool _seqOutput = false;
  CBVar _vectorOutput{};
  uint8_t _vectorInputLen = 0;
  uint8_t _vectorOutputLen = 0;

  void destroy() {
    destroyVar(_indices);
    destroyVar(_output);
  }

  void cleanup() {
    if (_indicesVar) {
      releaseVariable(_indicesVar);
      _indicesVar = nullptr;
    }
    if (_cachedSeq.elements) {
      for (auto i = _cachedSeq.len; i > 0; i--) {
        destroyVar(_cachedSeq.elements[i - 1]);
      }
      chainblocks::arrayFree(_cachedSeq);
    }
  }

  static CBTypesInfo inputTypes() { return CoreInfo::Indexables; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static CBParametersInfo parameters() {
    return CBParametersInfo(indicesParamsInfo);
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    bool valid = false;
    bool isTable = data.inputType.basicType == Table;
    // Figure if we output a sequence or not
    if (_indices.valueType == Seq) {
      if (_indices.payload.seqValue.len > 0) {
        if ((_indices.payload.seqValue.elements[0].valueType == Int &&
             !isTable) ||
            (_indices.payload.seqValue.elements[0].valueType == String &&
             isTable)) {
          _seqOutput = true;
          valid = true;
        }
      }
    } else if ((!isTable && _indices.valueType == Int) ||
               (isTable && _indices.valueType == String)) {
      _seqOutput = false;
      valid = true;
    } else { // ContextVar
      IterableExposedInfo infos(data.shared);
      for (auto &info : infos) {
        if (strcmp(info.name, _indices.payload.stringValue) == 0) {
          if (info.exposedType.basicType == Seq &&
              info.exposedType.seqTypes.len == 1 &&
              ((info.exposedType.seqTypes.elements[0].basicType == Int &&
                !isTable) ||
               (info.exposedType.seqTypes.elements[0].basicType == String &&
                isTable))) {
            _seqOutput = true;
            valid = true;
            break;
          } else if (info.exposedType.basicType == Int && !isTable) {
            _seqOutput = false;
            valid = true;
            break;
          } else if (info.exposedType.basicType == String && isTable) {
            _seqOutput = false;
            valid = true;
            break;
          } else {
            auto msg = "Take indices variable " + std::string(info.name) +
                       " expected to be either Seq, Int or String";
            throw CBException(msg);
          }
        }
      }
    }

    if (!valid)
      throw CBException("Take, invalid indices or malformed input.");

    if (data.inputType.basicType == Seq) {
      if (_seqOutput) {
        // multiple values, leave Seq
        return data.inputType;
      } else if (data.inputType.seqTypes.len == 1) {
        // single unique seq type
        return data.inputType.seqTypes.elements[0];
      } else {
        // value from seq but not unique
        return CoreInfo::AnyType;
      }
    } else if (data.inputType.basicType >= Int2 &&
               data.inputType.basicType <= Int16) {
      // int vector
    } else if (data.inputType.basicType >= Float2 &&
               data.inputType.basicType <= Float4) {
      if (_indices.valueType == ContextVar)
        throw CBException(
            "A Take on a vector cannot have Indices as a variable.");

      // Floats
      switch (data.inputType.basicType) {
      case Float2:
        _vectorInputLen = 2;
        break;
      case Float3:
        _vectorInputLen = 3;
        break;
      case Float4:
      default:
        _vectorInputLen = 4;
        break;
      }

      if (!_seqOutput) {
        _vectorOutputLen = 1;
        return CoreInfo::FloatType;
      } else {
        _vectorOutputLen = (uint8_t)_indices.payload.seqValue.len;
        switch (_vectorOutputLen) {
        case 2:
          _vectorOutput.valueType = Float2;
          return CoreInfo::Float2Type;
        case 3:
          _vectorOutput.valueType = Float3;
          return CoreInfo::Float3Type;
        case 4:
        default:
          _vectorOutput.valueType = Float4;
          return CoreInfo::Float4Type;
        }
      }
    } else if (data.inputType.basicType == Color) {
      // todo
    } else if (data.inputType.basicType == Bytes) {
      // todo
    } else if (data.inputType.basicType == String) {
      // todo
    } else if (data.inputType.basicType == Table) {
      if (_seqOutput) {
        // multiple values, leave Seq
        return CoreInfo::AnySeqType;
      } else if (data.inputType.table.types.len == 1) {
        // single unique seq type
        return data.inputType.table.types.elements[0];
      } else {
        // value from seq but not unique
        return CoreInfo::AnyType;
      }
    }

    throw CBException("Take, invalid input type or not implemented.");
  }

  CBExposedTypesInfo requiredVariables() {
    if (_indices.valueType == ContextVar) {
      if (_seqOutput)
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(
            _indices.payload.stringValue, "The required variables.",
            CoreInfo::IntSeqType));
      else
        _exposedInfo = ExposedInfo(
            ExposedInfo::Variable(_indices.payload.stringValue,
                                  "The required variable.", CoreInfo::IntType));
      return CBExposedTypesInfo(_exposedInfo);
    } else {
      return {};
    }
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      cloneVar(_indices, value);
      cleanup();
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _indices;
    default:
      break;
    }
    return Var::Empty;
  }

  struct OutOfRangeEx : public ActivationError {
    OutOfRangeEx(int64_t len, int64_t index)
        : ActivationError("Take out of range!") {
      LOG(ERROR) << "Out of range! len: " << len << " wanted index: " << index;
    }
  };

  void warmup(CBContext *context) {
    if (_indices.valueType == ContextVar && !_indicesVar) {
      _indicesVar = referenceVariable(context, _indices.payload.stringValue);
    }
  }

  ALWAYS_INLINE CBVar activateSeq(CBContext *context, const CBVar &input) {
    const auto inputLen = input.payload.seqValue.len;
    const auto &indices = _indicesVar ? *_indicesVar : _indices;

    if (!_seqOutput) {
      const auto index = indices.payload.intValue;
      if (index >= inputLen || index < 0) {
        throw OutOfRangeEx(inputLen, index);
      }
      cloneVar(_output, input.payload.seqValue.elements[index]);
      return _output;
    } else {
      const uint32_t nindices = indices.payload.seqValue.len;
      chainblocks::arrayResize(_cachedSeq, nindices);
      for (uint32_t i = 0; nindices > i; i++) {
        const auto index =
            indices.payload.seqValue.elements[i].payload.intValue;
        if (index >= inputLen || index < 0) {
          throw OutOfRangeEx(inputLen, index);
        }
        cloneVar(_cachedSeq.elements[i],
                 input.payload.seqValue.elements[index]);
      }
      return Var(_cachedSeq);
    }
  }

  CBVar activateTable(CBContext *context, const CBVar &input) {
    const auto &indices = _indicesVar ? *_indicesVar : _indices;

    if (!_seqOutput) {
      const auto key = indices.payload.stringValue;
      const auto val =
          input.payload.tableValue.api->tableAt(input.payload.tableValue, key);
      cloneVar(_output, *val);
      return _output;
    } else {
      const uint32_t nkeys = indices.payload.seqValue.len;
      chainblocks::arrayResize(_cachedSeq, nkeys);
      for (uint32_t i = 0; nkeys > i; i++) {
        const auto key =
            indices.payload.seqValue.elements[i].payload.stringValue;
        const auto val = input.payload.tableValue.api->tableAt(
            input.payload.tableValue, key);
        cloneVar(_cachedSeq.elements[i], *val);
      }
      return Var(_cachedSeq);
    }
  }

  CBVar activateFloats(CBContext *context, const CBVar &input) {
    const auto inputLen = (int64_t)_vectorInputLen;
    const auto &indices = _indices;

    if (!_seqOutput) {
      const auto index = indices.payload.intValue;
      if (index >= inputLen || index < 0) {
        throw OutOfRangeEx(inputLen, index);
      }

      switch (_vectorInputLen) {
      case Float2:
        return Var(input.payload.float2Value[index]);
      case Float3:
        return Var(input.payload.float3Value[index]);
      case Float4:
      default:
        return Var(input.payload.float4Value[index]);
      }
    } else {
      const uint64_t nindices = indices.payload.seqValue.len;
      for (uint64_t i = 0; nindices > i; i++) {
        const auto index =
            indices.payload.seqValue.elements[i].payload.intValue;
        if (index >= inputLen || index < 0 || nindices != _vectorOutputLen) {
          throw OutOfRangeEx(inputLen, index);
        }

        switch (_vectorOutputLen) {
        case 2:
          switch (_vectorInputLen) {
          case 2:
            _vectorOutput.payload.float2Value[i] =
                input.payload.float2Value[index];
            break;
          case 3:
            _vectorOutput.payload.float2Value[i] =
                input.payload.float3Value[index];
            break;
          case 4:
          default:
            _vectorOutput.payload.float2Value[i] =
                input.payload.float4Value[index];
            break;
          }
          break;
        case 3:
          switch (_vectorInputLen) {
          case 2:
            _vectorOutput.payload.float3Value[i] =
                input.payload.float2Value[index];
            break;
          case 3:
            _vectorOutput.payload.float3Value[i] =
                input.payload.float3Value[index];
            break;
          case 4:
          default:
            _vectorOutput.payload.float3Value[i] =
                input.payload.float4Value[index];
            break;
          }
          break;
        case 4:
        default:
          switch (_vectorInputLen) {
          case 2:
            _vectorOutput.payload.float4Value[i] =
                input.payload.float2Value[index];
            break;
          case 3:
            _vectorOutput.payload.float4Value[i] =
                input.payload.float3Value[index];
            break;
          case 4:
          default:
            _vectorOutput.payload.float4Value[i] =
                input.payload.float4Value[index];
            break;
          }
          break;
        }
      }
      return _vectorOutput;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    // Take branches during validation into different inlined blocks
    // If we hit this, maybe that type of input is not yet implemented
    throw ActivationError("Take path not implemented for this type.");
  }
};

struct RTake : public Take {
  // works only for seqs tho
  // TODO need to add string and bytes
  static inline ParamsInfo indicesParamsInfo = ParamsInfo(ParamsInfo::Param(
      "Indices", "One or multiple indices to extract from a sequence.",
      CoreInfo::RTakeTypes));

  static CBParametersInfo parameters() {
    return CBParametersInfo(indicesParamsInfo);
  }

  static CBTypesInfo inputTypes() { return CoreInfo::RIndexables; }

  CBVar activate(CBContext *context, const CBVar &input) {
    const auto inputLen = input.payload.seqValue.len;
    const auto &indices = _indicesVar ? *_indicesVar : _indices;

    if (!_seqOutput) {
      const auto index = indices.payload.intValue;
      if (index >= inputLen || index < 0) {
        throw OutOfRangeEx(inputLen, index);
      }
      cloneVar(_output, input.payload.seqValue.elements[inputLen - 1 - index]);
      return _output;
    } else {
      const uint32_t nindices = indices.payload.seqValue.len;
      chainblocks::arrayResize(_cachedSeq, nindices);
      for (uint32_t i = 0; nindices > i; i++) {
        const auto index =
            indices.payload.seqValue.elements[i].payload.intValue;
        if (index >= inputLen || index < 0) {
          throw OutOfRangeEx(inputLen, index);
        }
        cloneVar(_cachedSeq.elements[i],
                 input.payload.seqValue.elements[inputLen - 1 - index]);
      }
      return Var(_cachedSeq);
    }
  }
};

struct Slice {
  static inline ParamsInfo indicesParamsInfo =
      ParamsInfo(ParamsInfo::Param("From", "From index.", CoreInfo::IntsVar),
                 ParamsInfo::Param("To", "To index.", CoreInfo::IntsVarOrNone),
                 ParamsInfo::Param("Step", "The increment between each index.",
                                   CoreInfo::IntType));

  CBSeq _cachedSeq{};
  CBVar _from{Var(0)};
  CBVar *_fromVar = nullptr;
  CBVar _to{};
  CBVar *_toVar = nullptr;
  ExposedInfo _exposedInfo{};
  int64_t _step = 1;

  void destroy() {
    destroyVar(_from);
    destroyVar(_to);
  }

  void cleanup() {
    if (_fromVar) {
      releaseVariable(_fromVar);
      _fromVar = nullptr;
    }
    if (_toVar) {
      releaseVariable(_toVar);
      _toVar = nullptr;
    }
    if (_cachedSeq.elements) {
      for (auto i = _cachedSeq.len; i > 0; i--) {
        destroyVar(_cachedSeq.elements[i - 1]);
      }
      chainblocks::arrayFree(_cachedSeq);
    }
  }

  static CBTypesInfo inputTypes() { return CoreInfo::AnySeqType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static CBParametersInfo parameters() {
    return CBParametersInfo(indicesParamsInfo);
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    bool valid = false;

    if (_from.valueType == Int) {
      valid = true;
    } else { // ContextVar
      IterableExposedInfo infos(data.shared);
      for (auto &info : infos) {
        if (strcmp(info.name, _from.payload.stringValue) == 0) {
          valid = true;
          break;
        }
      }
    }

    if (!valid)
      throw CBException("Slice, invalid From variable.");

    if (_to.valueType == Int || _to.valueType == None) {
      valid = true;
    } else { // ContextVar
      IterableExposedInfo infos(data.shared);
      for (auto &info : infos) {
        if (strcmp(info.name, _to.payload.stringValue) == 0) {
          valid = true;
          break;
        }
      }
    }

    if (!valid)
      throw CBException("Slice, invalid To variable.");

    return data.inputType;
  }

  CBExposedTypesInfo requiredVariables() {
    if (_from.valueType == ContextVar && _to.valueType == ContextVar) {
      _exposedInfo = ExposedInfo(
          ExposedInfo::Variable(_from.payload.stringValue,
                                "The required variable.", CoreInfo::IntType),
          ExposedInfo::Variable(_to.payload.stringValue,
                                "The required variable.", CoreInfo::IntType));
      return CBExposedTypesInfo(_exposedInfo);
    } else if (_from.valueType == ContextVar) {
      _exposedInfo = ExposedInfo(
          ExposedInfo::Variable(_from.payload.stringValue,
                                "The required variable.", CoreInfo::IntType));
      return CBExposedTypesInfo(_exposedInfo);
    } else if (_to.valueType == ContextVar) {
      _exposedInfo = ExposedInfo(ExposedInfo::Variable(_to.payload.stringValue,
                                                       "The required variable.",
                                                       CoreInfo::IntType));
      return CBExposedTypesInfo(_exposedInfo);
    } else {
      return {};
    }
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      cloneVar(_from, value);
      cleanup();
      break;
    case 1:
      cloneVar(_to, value);
      cleanup();
      break;
    case 2:
      _step = value.payload.intValue;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _from;
    case 1:
      return _to;
    case 2:
      return Var(_step);
    default:
      break;
    }
    return Var::Empty;
  }

  struct OutOfRangeEx : public ActivationError {
    OutOfRangeEx(int64_t len, int64_t from, int64_t to)
        : ActivationError("Slice out of range!") {
      LOG(ERROR) << "Out of range! from: " << from << " to: " << to
                 << " len: " << len;
    }
  };

  CBVar activate(CBContext *context, const CBVar &input) {
    if (_from.valueType == ContextVar && !_fromVar) {
      _fromVar = referenceVariable(context, _from.payload.stringValue);
    }
    if (_to.valueType == ContextVar && !_toVar) {
      _toVar = referenceVariable(context, _to.payload.stringValue);
    }

    const auto inputLen = input.payload.seqValue.len;
    const auto &vfrom = _fromVar ? *_fromVar : _from;
    const auto &vto = _toVar ? *_toVar : _to;
    auto from = vfrom.payload.intValue;
    auto to = vto.valueType == None ? inputLen : vto.payload.intValue;
    if (to < 0) {
      to = inputLen + to;
    }

    if (from > to || to < 0 || to > inputLen) {
      throw OutOfRangeEx(inputLen, from, to);
    }

    const auto len = to - from;
    const auto actualLen = len / _step + (len % _step != 0);
    chainblocks::arrayResize(_cachedSeq, actualLen);
    auto idx = 0;
    for (auto i = from; i < to; i += _step) {
      cloneVar(_cachedSeq.elements[idx], input.payload.seqValue.elements[i]);
      idx++;
    }

    return Var(_cachedSeq);
  }
};

struct Limit {
  static inline ParamsInfo indicesParamsInfo = ParamsInfo(ParamsInfo::Param(
      "Max", "How many maximum elements to take from the input sequence.",
      CoreInfo::IntType));

  CBSeq _cachedResult{};
  int64_t _max = 0;

  void destroy() {
    if (_cachedResult.elements) {
      chainblocks::arrayFree(_cachedResult);
    }
  }

  static CBTypesInfo inputTypes() { return CoreInfo::AnySeqType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static CBParametersInfo parameters() {
    return CBParametersInfo(indicesParamsInfo);
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    // Figure if we output a sequence or not
    if (_max > 1) {
      if (data.inputType.basicType == Seq) {
        return data.inputType; // multiple values
      }
    } else {
      if (data.inputType.basicType == Seq && data.inputType.seqTypes.len == 1) {
        // single unique type
        return data.inputType.seqTypes.elements[0];
      } else {
        // multiple types
        return CoreInfo::AnySeqType;
      }
    }
    throw CBException("Limit expected a sequence as input.");
  }

  void setParam(int index, CBVar value) { _max = value.payload.intValue; }

  CBVar getParam(int index) { return Var(_max); }

  CBVar activate(CBContext *context, const CBVar &input) {
    int64_t inputLen = input.payload.seqValue.len;

    if (_max == 1) {
      auto index = 0;
      if (index >= inputLen) {
        LOG(ERROR) << "Limit out of range! len:" << inputLen
                   << " wanted index: " << index;
        throw ActivationError("Limit out of range!");
      }
      return input.payload.seqValue.elements[index];
    } else {
      // Else it's a seq
      auto nindices = (uint64_t)std::max((int64_t)0, std::min(inputLen, _max));
      chainblocks::arrayResize(_cachedResult, nindices);
      for (uint64_t i = 0; i < nindices; i++) {
        _cachedResult.elements[i] = input.payload.seqValue.elements[i];
      }
      return Var(_cachedResult);
    }
  }
};

struct Repeat {
  BlocksVar _blks{};
  BlocksVar _pred{};
  std::string _ctxVar;
  CBVar *_ctxTimes = nullptr;
  int64_t _times = 0;
  int64_t *_repeats{nullptr};
  bool _forever = false;
  ExposedInfo _requiredInfo{};

  void cleanup() {
    if (_ctxTimes) {
      releaseVariable(_ctxTimes);
    }
    _blks.cleanup();
    _pred.cleanup();
    _ctxTimes = nullptr;
    _repeats = nullptr;
  }

  void warmup(CBContext *ctx) {
    _blks.warmup(ctx);
    _pred.warmup(ctx);
    if (_ctxVar.size()) {
      assert(!_ctxTimes);
      _ctxTimes = referenceVariable(ctx, _ctxVar.c_str());
      _repeats = &_ctxTimes->payload.intValue;
    } else {
      _repeats = &_times;
    }
  }

  static inline Parameters _params{
      {"Action", "The blocks to repeat.", CoreInfo::Blocks},
      {"Times", "How many times we should repeat the action.",
       CoreInfo::IntsVar},
      {"Forever",
       "If we should repeat the action forever.",
       {CoreInfo::BoolType}},
      {"Until",
       "Optional blocks to use as predicate to continue repeating until "
       "it's true",
       CoreInfo::BlocksOrNone}};

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _blks = value;
      break;
    case 1:
      if (value.valueType == Int) {
        _ctxVar.clear();
        _times = value.payload.intValue;
      } else {
        _ctxVar.assign(value.payload.stringValue);
        _ctxTimes = nullptr;
      }
      break;
    case 2:
      _forever = value.payload.boolValue;
      break;
    case 3:
      _pred = value;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _blks;
    case 1:
      if (_ctxVar.size() == 0) {
        return Var(_times);
      } else {
        auto ctxTimes = Var(_ctxVar);
        ctxTimes.valueType = ContextVar;
        return ctxTimes;
      }
    case 2:
      return Var(_forever);
    case 3:
      return _pred;
    default:
      break;
    }
    throw CBException("Parameter out of range.");
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    _blks.compose(data);
    const auto predres = _pred.compose(data);
    if (_pred && predres.outputType.basicType != Bool) {
      throw ComposeError(
          "Repeat block Until predicate should output a boolean!");
    }
    return data.inputType;
  }

  CBExposedTypesInfo requiredVariables() {
    if (_ctxVar.size() == 0) {
      return {};
    } else {
      _requiredInfo = ExposedInfo(ExposedInfo::Variable(
          _ctxVar.c_str(), "The Int number of repeats variable.",
          CoreInfo::IntType));
      return CBExposedTypesInfo(_requiredInfo);
    }
  }

  FLATTEN ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    auto repeats = *_repeats;
    while (repeats || _forever) {
      CBVar repeatOutput{};
      CBVar blks = _blks;
      auto state = activateBlocks(blks.payload.seqValue, context, input,
                                  repeatOutput, true);
      if (state != CBChainState::Continue)
        break;

      if (_pred) {
        CBVar pres{};
        state = _pred.activate(context, input, pres, true);
        // logic flow here!
        if (unlikely(state > CBChainState::Return || pres.payload.boolValue))
          break;
      }
    }
    return input;
  }
};

struct Once {
  BlocksVar _blks;
  ExposedInfo _requiredInfo{};
  CBValidationResult _validation{};
  bool done = false;

  void cleanup() {
    _blks.cleanup();
    done = false;
  }

  void warmup(CBContext *ctx) { _blks.warmup(ctx); }

  static inline ParamsInfo params = ParamsInfo(
      ParamsInfo::Param("Action", "The blocks to repeat.", CoreInfo::Blocks));

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _blks = value;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _blks;
    default:
      break;
    }
    throw CBException("Parameter out of range.");
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    _validation = _blks.compose(data);
    return data.inputType;
  }

  CBExposedTypesInfo exposedVariables() { return _validation.exposedInfo; }

  FLATTEN ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(!done)) {
      done = true;
      CBVar repeatOutput{};
      CBVar blks = _blks;
      activateBlocks(blks.payload.seqValue, context, input, repeatOutput);
    }
    return input;
  }
};

RUNTIME_CORE_BLOCK_TYPE(Const);
RUNTIME_CORE_BLOCK_TYPE(Input);
RUNTIME_CORE_BLOCK_TYPE(SetInput);
RUNTIME_CORE_BLOCK_TYPE(And);
RUNTIME_CORE_BLOCK_TYPE(Or);
RUNTIME_CORE_BLOCK_TYPE(Not);
RUNTIME_CORE_BLOCK_TYPE(Stop);
RUNTIME_CORE_BLOCK_TYPE(Restart);
RUNTIME_CORE_BLOCK_TYPE(Return);
RUNTIME_CORE_BLOCK_TYPE(IsValidNumber);
RUNTIME_CORE_BLOCK_TYPE(Set);
RUNTIME_CORE_BLOCK_TYPE(Ref);
RUNTIME_CORE_BLOCK_TYPE(Update);
RUNTIME_CORE_BLOCK_TYPE(Get);
RUNTIME_CORE_BLOCK_TYPE(Swap);
RUNTIME_CORE_BLOCK_TYPE(Take);
RUNTIME_CORE_BLOCK_TYPE(RTake);
RUNTIME_CORE_BLOCK_TYPE(Slice);
RUNTIME_CORE_BLOCK_TYPE(Limit);
RUNTIME_CORE_BLOCK_TYPE(Push);
RUNTIME_CORE_BLOCK_TYPE(Sequence);
RUNTIME_CORE_BLOCK_TYPE(Pop);
RUNTIME_CORE_BLOCK_TYPE(PopFront);
RUNTIME_CORE_BLOCK_TYPE(Clear);
RUNTIME_CORE_BLOCK_TYPE(Drop);
RUNTIME_CORE_BLOCK_TYPE(DropFront);
RUNTIME_CORE_BLOCK_TYPE(Count);
RUNTIME_CORE_BLOCK_TYPE(Repeat);
}; // namespace chainblocks
