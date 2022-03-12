/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#pragma once

#include "../blocks_macros.hpp"
// circular warning this is self inclusive on purpose
#include "../foundation.hpp"
#include "chainblocks.h"
#include "chainblocks.hpp"
#include "common_types.hpp"
#include "number_types.hpp"
#include <cassert>
#include <cmath>
#include <sstream>

namespace chainblocks {
struct CoreInfo2 {
  // 'type' == 0x74797065 or 1954115685
  static inline EnumInfo<BasicTypes> BasicTypesEnum{"Type", CoreCC, 'type'};
  static inline Type BasicTypesType{{CBType::Enum, {.enumeration = {CoreCC, 'type'}}}};
  static inline Type BasicTypesSeqType{{CBType::Seq, {.seqTypes = BasicTypesType}}};
  static inline Types BasicTypesTypes{{BasicTypesType, BasicTypesSeqType}, true};
};

struct Const {
  static inline ParamsInfo constParamsInfo =
      ParamsInfo(ParamsInfo::Param("Value", CBCCSTR("The constant value to insert in the chain."), CoreInfo::AnyType));

  OwnedVar _value{};
  OwnedVar _clone{};
  CBTypeInfo _innerInfo{};
  VariableResolver resolver;

  void destroy() { freeDerivedInfo(_innerInfo); }

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static CBParametersInfo parameters() { return CBParametersInfo(constParamsInfo); }

  void setParam(int index, const CBVar &value) { _value = value; }

  CBVar getParam(int index) { return _value; }

  CBTypeInfo compose(const CBInstanceData &data) {
    freeDerivedInfo(_innerInfo);
    bool hasVariables = false;
    _innerInfo = deriveTypeInfo(_value, data, &hasVariables);
    if (hasVariables) {
      _clone = _value;
      const_cast<CBlock *>(data.block)->inlineBlockId = CBInlineBlocks::NotInline;
    } else {
      _clone = Var::Empty;
      const_cast<CBlock *>(data.block)->inlineBlockId = CBInlineBlocks::CoreConst;
    }
    return _innerInfo;
  }

  void cleanup() { resolver.cleanup(); }

  void warmup(CBContext *context) {
    if (_clone != Var::Empty)
      resolver.warmup(_value, _clone, context);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    // we need to reassign values every frame
    resolver.reassign();
    return _clone;
  }
};

static ParamsInfo compareParamsInfo =
    ParamsInfo(ParamsInfo::Param("Value", CBCCSTR("The value to test against for equality."), CoreInfo::AnyType));

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

  void setParam(int index, const CBVar &value) {
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
    // TODO deep resolve variables like const
    _target = _value.valueType == ContextVar ? referenceVariable(context, _value.payload.stringValue) : &_value;
  }
};

#define LOGIC_OP(NAME, OP)                                                         \
  struct NAME : public BaseOpsBin {                                                \
    FLATTEN ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) { \
      const auto &value = *_target;                                                \
      if (input OP value) {                                                        \
        return Var::True;                                                          \
      }                                                                            \
      return Var::False;                                                           \
    }                                                                              \
  };                                                                               \
  RUNTIME_CORE_BLOCK_TYPE(NAME);

LOGIC_OP(Is, ==);
LOGIC_OP(IsNot, !=);
LOGIC_OP(IsMore, >);
LOGIC_OP(IsLess, <);
LOGIC_OP(IsMoreEqual, >=);
LOGIC_OP(IsLessEqual, <=);

#define LOGIC_ANY_SEQ_OP(NAME, OP)                                                        \
  struct NAME : public BaseOpsBin {                                                       \
    CBVar activate(CBContext *context, const CBVar &input) {                              \
      const auto &value = *_target;                                                       \
      if (input.valueType == Seq && value.valueType == Seq) {                             \
        auto vlen = value.payload.seqValue.len;                                           \
        auto ilen = input.payload.seqValue.len;                                           \
        if (ilen > vlen)                                                                  \
          throw ActivationError("Failed to compare, input len > value len.");             \
        for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {                       \
          if (input.payload.seqValue.elements[i] OP value.payload.seqValue.elements[i]) { \
            return Var::True;                                                             \
          }                                                                               \
        }                                                                                 \
        return Var::False;                                                                \
      } else if (input.valueType == Seq && value.valueType != Seq) {                      \
        for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {                       \
          if (input.payload.seqValue.elements[i] OP value) {                              \
            return Var::True;                                                             \
          }                                                                               \
        }                                                                                 \
        return Var::False;                                                                \
      } else if (input.valueType != Seq && value.valueType == Seq) {                      \
        for (uint32_t i = 0; i < value.payload.seqValue.len; i++) {                       \
          if (input OP value.payload.seqValue.elements[i]) {                              \
            return Var::True;                                                             \
          }                                                                               \
        }                                                                                 \
        return Var::False;                                                                \
      } else if (input.valueType != Seq && value.valueType != Seq) {                      \
        if (input OP value) {                                                             \
          return Var::True;                                                               \
        }                                                                                 \
        return Var::False;                                                                \
      }                                                                                   \
      return Var::False;                                                                  \
    }                                                                                     \
  };                                                                                      \
  RUNTIME_CORE_BLOCK_TYPE(NAME);

#define LOGIC_ALL_SEQ_OP(NAME, OP)                                                           \
  struct NAME : public BaseOpsBin {                                                          \
    CBVar activate(CBContext *context, const CBVar &input) {                                 \
      const auto &value = *_target;                                                          \
      if (input.valueType == Seq && value.valueType == Seq) {                                \
        auto vlen = value.payload.seqValue.len;                                              \
        auto ilen = input.payload.seqValue.len;                                              \
        if (ilen > vlen)                                                                     \
          throw ActivationError("Failed to compare, input len > value len.");                \
        for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {                          \
          if (!(input.payload.seqValue.elements[i] OP value.payload.seqValue.elements[i])) { \
            return Var::False;                                                               \
          }                                                                                  \
        }                                                                                    \
        return Var::True;                                                                    \
      } else if (input.valueType == Seq && value.valueType != Seq) {                         \
        for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {                          \
          if (!(input.payload.seqValue.elements[i] OP value)) {                              \
            return Var::False;                                                               \
          }                                                                                  \
        }                                                                                    \
        return Var::True;                                                                    \
      } else if (input.valueType != Seq && value.valueType == Seq) {                         \
        for (uint32_t i = 0; i < value.payload.seqValue.len; i++) {                          \
          if (!(input OP value.payload.seqValue.elements[i])) {                              \
            return Var::False;                                                               \
          }                                                                                  \
        }                                                                                    \
        return Var::True;                                                                    \
      } else if (input.valueType != Seq && value.valueType != Seq) {                         \
        if (!(input OP value)) {                                                             \
          return Var::False;                                                                 \
        }                                                                                    \
        return Var::True;                                                                    \
      }                                                                                      \
      return Var::False;                                                                     \
    }                                                                                        \
  };                                                                                         \
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

#define LOGIC_OP_DESC(NAME)         \
  RUNTIME_CORE_BLOCK_FACTORY(NAME); \
  RUNTIME_BLOCK_destroy(NAME);      \
  RUNTIME_BLOCK_cleanup(NAME);      \
  RUNTIME_BLOCK_warmup(NAME);       \
  RUNTIME_BLOCK_inputTypes(NAME);   \
  RUNTIME_BLOCK_outputTypes(NAME);  \
  RUNTIME_BLOCK_parameters(NAME);   \
  RUNTIME_BLOCK_setParam(NAME);     \
  RUNTIME_BLOCK_getParam(NAME);     \
  RUNTIME_BLOCK_activate(NAME);     \
  RUNTIME_BLOCK_END(NAME);

struct Input {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  FLATTEN ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    return context->chainStack.back()->currentInput;
  }
};

struct Pause {
  ExposedInfo reqs{};
  static inline Parameters params{
      {"Time",
       CBCCSTR("The amount of time in seconds (can be fractional) to pause "
               "this chain."),
       {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::IntType, CoreInfo::FloatVarType, CoreInfo::IntVarType}}};

  ParamVar time{};

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) { time = value; }

  CBVar getParam(int index) { return time; }

  void warmup(CBContext *context) { time.warmup(context); }

  void cleanup() { time.cleanup(); }

  CBExposedTypesInfo requiredVariables() {
    if (time.isVariable()) {
      reqs = ExposedInfo(ExposedInfo::Variable(time.variableName(), CBCCSTR("The required variable"), CoreInfo::FloatType),
                         ExposedInfo::Variable(time.variableName(), CBCCSTR("The required variable"), CoreInfo::IntType));
    } else {
      reqs = {};
    }
    return CBExposedTypesInfo(reqs);
  }

  FLATTEN ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    const auto &t = time.get();
    if (t.valueType == None)
      suspend(context, 0.0);
    else if (t.valueType == Int)
      suspend(context, double(t.payload.intValue));
    else
      suspend(context, t.payload.floatValue);
    return input;
  }
};

struct PauseMs : public Pause {
  static inline Parameters params{{"Time",
                                   CBCCSTR("The amount of time in milliseconds to pause this chain."),
                                   {CoreInfo::NoneType, CoreInfo::IntType, CoreInfo::IntVarType}}};

  static CBParametersInfo parameters() { return params; }

  CBExposedTypesInfo requiredVariables() {
    if (time.isVariable()) {
      reqs = ExposedInfo(ExposedInfo::Variable(time.variableName(), CBCCSTR("The required variable"), CoreInfo::IntType));
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
      suspend(context, double(t.payload.intValue) / 1000.0);
    return input;
  }
};

struct Comment {
  std::string _comment;

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static inline Parameters params{{"Text", CBCCSTR("The comment's text."), {CoreInfo::StringType}}};

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) { _comment = value.payload.stringValue; }

  CBVar getParam(int index) { return Var(_comment); }

  CBVar activate(CBContext *context, const CBVar &input) {
    // We are a NOOP block
    CBLOG_FATAL("invalid state");
    return input;
  }
};

struct OnCleanup {
  BlocksVar _blocks{};
  CBContext *_context{nullptr};

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static inline Parameters params{{"Blocks",
                                   CBCCSTR("The blocks to execute on chain's cleanup. Notice that blocks "
                                           "that suspend execution are not allowed."),
                                   {CoreInfo::BlocksOrNone}}};

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) { _blocks = value; }

  CBVar getParam(int index) { return _blocks; }

  CBTypeInfo compose(const CBInstanceData &data) {
    _blocks.compose(data);
    return data.inputType;
  }

  void warmup(CBContext *ctx) {
    _context = ctx;
    _blocks.warmup(ctx);
  }

  void cleanup() {
    // also run the blocks here!
    if (_context) {
      // cleanup might be called multiple times
      CBVar output{};
      std::string error;
      if (_context->failed()) {
        error = _context->getErrorMessage();
      }
      // we need to reset the state or only the first block will run
      _context->resetCancelFlow();
      _context->onCleanup = true; // this is kind of a hack
      _blocks.activate(_context, Var(error), output);
      // restore the terminal state
      if (error.size() > 0) {
        _context->cancelFlow(error);
      } else {
        // var should not matter at this point
        _context->stopFlow(Var::Empty);
      }
      _context = nullptr;
    }
    // and cleanup after
    _blocks.cleanup();
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    // We are a NOOP block
    CBLOG_FATAL("invalid state");
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
  CBVar activate(CBContext *context, const CBVar &input) { return Var(!input.payload.boolValue); }
};

struct IsNone {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }
  CBVar activate(CBContext *context, const CBVar &input) { return Var(input.valueType == None); }
};

struct IsNotNone {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }
  CBVar activate(CBContext *context, const CBVar &input) { return Var(input.valueType != None); }
};

struct Restart {
  // Must ensure input is the same kind of chain root input
  CBTypeInfo compose(const CBInstanceData &data) {
    if (data.chain->inputType->basicType != CBType::None && data.inputType != data.chain->inputType)
      throw ComposeError("Restart input and chain input type mismatch, Restart "
                         "feeds back to the chain input, chain: " +
                         data.chain->name + " expected: " + type2Name(data.chain->inputType->basicType));
    return data.inputType; // actually we are flow stopper
  }

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

struct Fail {
  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return CoreInfo::NoneType; }
  CBVar activate(CBContext *context, const CBVar &input) {
    context->cancelFlow(input.payload.stringValue);
    return input;
  }
};

struct IsValidNumber {
  static CBTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }
  CBVar activate(CBContext *context, const CBVar &input) { return Var(std::isnormal(input.payload.floatValue)); }
};

struct NaNTo0 {
  static CBTypesInfo inputTypes() { return CoreInfo::FloatOrFloatSeq; }
  static CBTypesInfo outputTypes() { return CoreInfo::FloatOrFloatSeq; }

  CBTypeInfo compose(const CBInstanceData &data) {
    if (data.inputType.basicType == Float) {
      OVERRIDE_ACTIVATE(data, activateSingle);
    } else {
      OVERRIDE_ACTIVATE(data, activateSeq);
    }
    return data.inputType;
  }

  CBVar activateSingle(CBContext *context, const CBVar &input) {
    if (std::isnan(input.payload.floatValue)) {
      return Var(0);
    } else {
      return input;
    }
  }

  CBVar activateSeq(CBContext *context, const CBVar &input) {
    // violate const.. edit in place
    auto violatedInput = const_cast<CBVar &>(input);
    for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {
      auto val = violatedInput.payload.seqValue.elements[i];
      if (std::isnan(val.payload.floatValue)) {
        violatedInput.payload.seqValue.elements[i].payload.floatValue = 0.0;
      }
    }
    return input;
  }

  CBVar activate(CBContext *context, const CBVar &input) { throw ActivationError("Unexpected code path"); }
};

struct VariableBase {
  CBVar *_target{nullptr};
  CBVar *_cell{nullptr};
  std::string _name;
  ParamVar _key{};
  ExposedInfo _exposedInfo{};
  bool _isTable{false};
  bool _global{false};

  static inline ParamsInfo variableParamsInfo =
      ParamsInfo(ParamsInfo::Param("Name", CBCCSTR("The name of the variable."), CoreInfo::StringOrAnyVar),
                 ParamsInfo::Param("Key",
                                   CBCCSTR("The key of the value to read/write from/in the table (this "
                                           "variable will become a table)."),
                                   CoreInfo::StringStringVarOrNone),
                 ParamsInfo::Param("Global",
                                   CBCCSTR("If the variable is or should be available to "
                                           "all of the chains in the same node."),
                                   CoreInfo::BoolType));

  static constexpr int variableParamsInfoLen = 3;

  static CBParametersInfo parameters() { return CBParametersInfo(variableParamsInfo); }

  void cleanup() {
    if (_target) {
      releaseVariable(_target);
    }
    _key.cleanup();
    _target = nullptr;
    _cell = nullptr;
  }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _name = value.payload.stringValue;
      break;
    case 1:
      if (value.valueType == None) {
        _isTable = false;
      } else {
        _isTable = true;
      }
      _key = value;
      break;
    case 2:
      _global = value.payload.boolValue;
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_name);
    case 1:
      return _key;
    case 2:
      return Var(_global);
    default:
      throw InvalidParameterIndex();
    }
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
        if (_isTable && !reference.isTableEntry && reference.exposedType.basicType != Table) {
          throw ComposeError("Set/Ref/Update, variable was not a table: " + _name);
        } else if (!_isTable && reference.isTableEntry) {
          throw ComposeError("Set/Ref/Update, variable was a table: " + _name);
        } else if (!_isTable &&
                   // need to check if this was just a any table definition {}
                   !(reference.exposedType.basicType == Table && reference.exposedType.table.types.len == 0) &&
                   data.inputType != reference.exposedType) {
          throw ComposeError("Set/Ref/Update, variable already set as another "
                             "type: " +
                             _name);
        }
        if (!_isTable && !reference.isMutable) {
          CBLOG_ERROR("Error with variable: {}", _name);
          throw ComposeError("Set/Ref/Update, attempted to write an immutable variable.");
        }
        if (!_isTable && reference.isProtected) {
          CBLOG_ERROR("Error with variable: {}", _name);
          throw ComposeError("Set/Ref/Update, attempted to write a protected variable.");
        }
        if (!_isTable && warnIfExists) {
          CBLOG_INFO("Set - Warning: setting an already exposed variable, use "
                     "Update to avoid this warning, variable: {}",
                     _name);
        }
      }
    }
  }

  void warmup(CBContext *context) {
    if (_global)
      _target = referenceGlobalVariable(context, _name.c_str());
    else
      _target = referenceVariable(context, _name.c_str());
    _key.warmup(context);
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
        _target->payload.tableValue.api = &GetGlobals().TableInterface;
        _target->payload.tableValue.opaque = new CBMap();
      }

      auto &kv = _key.get();
      CBVar *vptr = _target->payload.tableValue.api->tableAt(_target->payload.tableValue, kv.payload.stringValue);

      // Clone will try to recyle memory and such
      cloneVar(*vptr, input);

      // use fast cell from now
      // if variable we cannot do this tho!
      if (!_key.isVariable())
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
      if (_key.isVariable()) {
        // only add types info, we don't know keys
        _tableTypeInfo = CBTypeInfo{CBType::Table, {.table = {.types = {&_tableContentInfo, 1, 0}}}};
      } else {
        CBVar kv = _key;
        _tableContentKey = kv.payload.stringValue;
        _tableTypeInfo =
            CBTypeInfo{CBType::Table, {.table = {.keys = {&_tableContentKey, 1, 0}, .types = {&_tableContentInfo, 1, 0}}}};
      }
      if (_global) {
        _exposedInfo =
            ExposedInfo(ExposedInfo::GlobalVariable(_name.c_str(), CBCCSTR("The exposed table."), _tableTypeInfo, true, true));
      } else {
        _exposedInfo =
            ExposedInfo(ExposedInfo::Variable(_name.c_str(), CBCCSTR("The exposed table."), _tableTypeInfo, true, true));
      }
    } else {
      // just a variable!
      if (_global) {
        _exposedInfo = ExposedInfo(
            ExposedInfo::GlobalVariable(_name.c_str(), CBCCSTR("The exposed variable."), CBTypeInfo(data.inputType), true));
      } else {
        _exposedInfo =
            ExposedInfo(ExposedInfo::Variable(_name.c_str(), CBCCSTR("The exposed variable."), CBTypeInfo(data.inputType), true));
      }
    }
    return data.inputType;
  }

  CBExposedTypesInfo exposedVariables() { return CBExposedTypesInfo(_exposedInfo); }
};

struct Ref : public SetBase {
  CBTypeInfo compose(const CBInstanceData &data) {
    sanityChecks(data, true);

    // bake exposed types
    if (_isTable) {
      // we are a table!
      _tableContentInfo = data.inputType;
      if (_key.isVariable()) {
        // only add types info, we don't know keys
        _tableTypeInfo = CBTypeInfo{CBType::Table, {.table = {.types = {&_tableContentInfo, 1, 0}}}};
      } else {
        CBVar kv = _key;
        _tableContentKey = kv.payload.stringValue;
        _tableTypeInfo =
            CBTypeInfo{CBType::Table, {.table = {.keys = {&_tableContentKey, 1, 0}, .types = {&_tableContentInfo, 1, 0}}}};
      }
      _exposedInfo =
          ExposedInfo(ExposedInfo::Variable(_name.c_str(), CBCCSTR("The exposed table."), _tableTypeInfo, false, true));

      // properly link the right call
      const_cast<CBlock *>(data.block)->inlineBlockId = CBInlineBlocks::CoreRefTable;
    } else {
      // just a variable!
      _exposedInfo =
          ExposedInfo(ExposedInfo::Variable(_name.c_str(), CBCCSTR("The exposed variable."), CBTypeInfo(data.inputType), false));

      const_cast<CBlock *>(data.block)->inlineBlockId = CBInlineBlocks::CoreRefRegular;
    }
    return data.inputType;
  }

  CBExposedTypesInfo exposedVariables() { return CBExposedTypesInfo(_exposedInfo); }

  void cleanup() {
    if (_target) {
      // this is a special case
      // Ref will reference previous block result..
      // Let's cleanup our storage so that release, if calls destroy
      // won't mangle the block's variable memory
      const auto rc = _target->refcount;
      const auto rcflag = _target->flags & CBVAR_FLAGS_REF_COUNTED;
      memset(_target, 0x0, sizeof(CBVar));
      _target->refcount = rc;
      _target->flags |= rcflag;
      releaseVariable(_target);
    }
    _target = nullptr;

    _cell = nullptr;
    _key.cleanup();
  }

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    throw ActivationError("Invalid Ref activation function.");
  }

  ALWAYS_INLINE CBVar activateTable(CBContext *context, const CBVar &input) {
    if (likely(_cell != nullptr)) {
      memcpy(_cell, &input, sizeof(CBVar));
      return input;
    } else {
      if (_target->valueType != Table) {
        // Not initialized yet
        _target->valueType = Table;
        _target->payload.tableValue.api = &GetGlobals().TableInterface;
        _target->payload.tableValue.opaque = new CBMap();
      }

      auto &kv = _key.get();
      CBVar *vptr = _target->payload.tableValue.api->tableAt(_target->payload.tableValue, kv.payload.stringValue);

      // Notice, NO Cloning!
      memcpy(vptr, &input, sizeof(CBVar));

      // use fast cell from now
      if (!_key.isVariable())
        _cell = vptr;

      return input;
    }
  }

  ALWAYS_INLINE CBVar activateRegular(CBContext *context, const CBVar &input) {
    // must keep refcount!
    const auto rc = _target->refcount;
    const auto rcflag = _target->flags & CBVAR_FLAGS_REF_COUNTED;
    memcpy(_target, &input, sizeof(CBVar));
    _target->refcount = rc;
    _target->flags |= rcflag;
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
        if (name == _name && data.shared.elements[i].exposedType.basicType == Table &&
            data.shared.elements[i].exposedType.table.types.elements) {
          auto &tableKeys = data.shared.elements[i].exposedType.table.keys;
          auto &tableTypes = data.shared.elements[i].exposedType.table.types;
          for (uint32_t y = 0; y < tableKeys.len; y++) {
            // if keys are populated they are not variables
            CBVar kv = _key;
            auto &key = tableKeys.elements[y];
            if (strcmp(kv.payload.stringValue, key) == 0) {
              if (data.inputType != tableTypes.elements[y]) {
                throw CBException("Update: error, update is changing the variable type.");
              }
            }
          }
        }
      }
    } else {
      for (uint32_t i = 0; i < data.shared.len; i++) {
        auto &cv = data.shared.elements[i];
        if (_name == cv.name) {
          if (cv.exposedType.basicType != CBType::Table && data.inputType != cv.exposedType) {
            throw CBException("Update: error, update is changing the variable type.");
          }
        }
      }
    }

    // bake exposed types
    if (_isTable) {
      // we are a table!
      _tableContentInfo = data.inputType;
      if (_key.isVariable()) {
        // only add types info, we don't know keys
        _tableTypeInfo = CBTypeInfo{CBType::Table, {.table = {.types = {&_tableContentInfo, 1, 0}}}};
      } else {
        CBVar kv = _key;
        _tableContentKey = kv.payload.stringValue;
        _tableTypeInfo =
            CBTypeInfo{CBType::Table, {.table = {.keys = {&_tableContentKey, 1, 0}, .types = {&_tableContentInfo, 1, 0}}}};
      }
      _exposedInfo = ExposedInfo(ExposedInfo::Variable(_name.c_str(), CBCCSTR("The exposed table."), _tableTypeInfo, true, true));
    } else {
      // just a variable!
      _exposedInfo =
          ExposedInfo(ExposedInfo::Variable(_name.c_str(), CBCCSTR("The exposed variable."), CBTypeInfo(data.inputType), true));
    }

    return data.inputType;
  }

  CBExposedTypesInfo requiredVariables() { return CBExposedTypesInfo(_exposedInfo); }
};

struct Get : public VariableBase {
  CBVar _defaultValue{};
  CBTypeInfo _defaultType{};
  std::vector<CBTypeInfo> _tableTypes{};
  std::vector<CBString> _tableKeys{};
  CBlock *_block{nullptr};

  static inline ParamsInfo getParamsInfo =
      ParamsInfo(variableParamsInfo, ParamsInfo::Param("Default",
                                                       CBCCSTR("The default value to use to infer types and output if the "
                                                               "variable is not set, key is not there and/or type "
                                                               "mismatches."),
                                                       CoreInfo::AnyType));

  static CBParametersInfo parameters() { return CBParametersInfo(getParamsInfo); }

  void setParam(int index, const CBVar &value) {
    if (index < variableParamsInfoLen)
      VariableBase::setParam(index, value);
    else if (index == variableParamsInfoLen + 0)
      cloneVar(_defaultValue, value);
  }

  CBVar getParam(int index) {
    if (index < variableParamsInfoLen)
      return VariableBase::getParam(index);
    else if (index == variableParamsInfoLen + 0)
      return _defaultValue;
    throw CBException("Param index out of range.");
  }

  void destroy() { freeDerivedInfo(_defaultType); }

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  CBTypeInfo compose(const CBInstanceData &data) {
    _block = const_cast<CBlock *>(data.block);
    if (_isTable) {
      for (uint32_t i = 0; data.shared.len > i; i++) {
        auto &name = data.shared.elements[i].name;
        if (strcmp(name, _name.c_str()) == 0 && data.shared.elements[i].exposedType.basicType == Table) {
          auto &tableKeys = data.shared.elements[i].exposedType.table.keys;
          auto &tableTypes = data.shared.elements[i].exposedType.table.types;
          if (tableKeys.len == tableTypes.len) {
            // if we have a name use it
            for (uint32_t y = 0; y < tableKeys.len; y++) {
              // if keys are populated they are not variables
              CBVar kv = _key;
              auto &key = tableKeys.elements[y];
              if (strcmp(kv.payload.stringValue, key) == 0) {
                return tableTypes.elements[y];
              }
            }
          } else {
            // we got no key names
            if (_defaultValue.valueType != None) {
              freeDerivedInfo(_defaultType);
              _defaultType = deriveTypeInfo(_defaultValue, data);
              return _defaultType;
            } else if (tableTypes.len == 1) {
              // 1 type only so we assume we return that type
              return tableTypes.elements[0];
            } else {
              // multiple types...
              // return any, can still be used with ExpectX
              return CoreInfo::AnyType;
            }
          }

          if (data.shared.elements[i].isProtected) {
            throw ComposeError("Get (" + _name + "): Cannot Get, variable is protected.");
          }
        }
      }
      if (_defaultValue.valueType != None) {
        freeDerivedInfo(_defaultType);
        _defaultType = deriveTypeInfo(_defaultValue, data);
        return _defaultType;
      } else {
        if (_key.isVariable()) {
          throw ComposeError("Get (" + _name + "/" + std::string(_key.variableName()) +
                             "[variable]): Could not infer an output type, key not found "
                             "and no Default value provided.");
        } else {
          CBVar kv = _key;
          throw ComposeError("Get (" + _name + "/" + std::string(kv.payload.stringValue) +
                             "): Could not infer an output type, key not found "
                             "and no Default value provided.");
        }
      }
    } else {
      _tableTypes.clear();
      _tableKeys.clear();
      for (uint32_t i = 0; i < data.shared.len; i++) {
        auto &cv = data.shared.elements[i];
        if (strcmp(_name.c_str(), cv.name) == 0) {
          if (cv.isProtected) {
            throw ComposeError("Get (" + _name + "): Cannot Get, variable is protected.");
          }
          if (cv.exposedType.basicType == CBType::Table && cv.isTableEntry && cv.exposedType.table.types.len == 1) {
            // in this case we need to gather all types of the table
            _tableTypes.emplace_back(cv.exposedType.table.types.elements[0]);
            if (cv.exposedType.table.keys.len == 1) {
              _tableKeys.emplace_back(cv.exposedType.table.keys.elements[0]);
            } else {
              _tableKeys.emplace_back("");
            }
          } else {
            return cv.exposedType;
          }
        }
      }

      // check if we can compose a table type
      if (_tableTypes.size() > 0) {
        auto outputTableType = CBTypeInfo(CoreInfo::AnyTableType);
        outputTableType.table.types = {&_tableTypes[0], uint32_t(_tableTypes.size()), 0};
        outputTableType.table.keys = {&_tableKeys[0], uint32_t(_tableKeys.size()), 0};
        return outputTableType;
      }
    }

    if (_defaultValue.valueType != None) {
      freeDerivedInfo(_defaultType);
      _defaultType = deriveTypeInfo(_defaultValue, data);
      return _defaultType;
    } else {
      throw ComposeError("Get (" + _name + "): Could not infer an output type and no Default value provided.");
    }
  }

  CBExposedTypesInfo requiredVariables() {
    if (_defaultValue.valueType != None) {
      return {};
    } else {
      if (_isTable) {
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(_name.c_str(), CBCCSTR("The required table."), CoreInfo::AnyTableType));
      } else {
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(_name.c_str(), CBCCSTR("The required variable."), CoreInfo::AnyType));
      }
      return CBExposedTypesInfo(_exposedInfo);
    }
  }

  bool defaultTypeCheck(const CBVar &value) {
    auto warn = false;
    DEFER({
      if (warn) {
        CBLOG_INFO("Get found a variable but it's using the default value because the "
                   "type found did not match with the default type.");
      }
    });
    if (value.valueType != _defaultValue.valueType)
      return false;

    if (value.valueType == CBType::Object && (value.payload.objectVendorId != _defaultValue.payload.objectVendorId ||
                                              value.payload.objectTypeId != _defaultValue.payload.objectTypeId))
      return false;

    if (value.valueType == CBType::Enum && (value.payload.enumVendorId != _defaultValue.payload.enumVendorId ||
                                            value.payload.enumTypeId != _defaultValue.payload.enumTypeId))
      return false;

    return true;
  }

  void warmup(CBContext *context) {
    if (_global)
      _target = referenceGlobalVariable(context, _name.c_str());
    else
      _target = referenceVariable(context, _name.c_str());

    _key.warmup(context);
  }

  void cleanup() {
    // reset block id
    if (_block) {
      _block->inlineBlockId = CBInlineBlocks::NotInline;
    }
    VariableBase::cleanup();
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(_cell != nullptr)) {
      // we override block id, this should not happen
      assert(false);
      return Var::Empty;
    } else {
      if (_isTable) {
        if (_target->valueType == Table) {
          auto &kv = _key.get();
          if (_target->payload.tableValue.api->tableContains(_target->payload.tableValue, kv.payload.stringValue)) {
            // Has it
            CBVar *vptr = _target->payload.tableValue.api->tableAt(_target->payload.tableValue, kv.payload.stringValue);

            if (unlikely(_defaultValue.valueType != None && !defaultTypeCheck(*vptr))) {
              return _defaultValue;
            } else {
              // Pin fast cell
              // skip if variable
              if (!_key.isVariable()) {
                _cell = vptr;
                // override block internal id
                _block->inlineBlockId = CBInlineBlocks::CoreGet;
              }
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
        if (unlikely(_defaultValue.valueType != None && !defaultTypeCheck(value))) {
          return _defaultValue;
        } else {
          // Pin fast cell
          _cell = _target;
          // override block internal id
          _block->inlineBlockId = CBInlineBlocks::CoreGet;
          return value;
        }
      }
    }
  }
};

struct Swap {
  static inline ParamsInfo swapParamsInfo =
      ParamsInfo(ParamsInfo::Param("NameA", CBCCSTR("The name of first variable."), CoreInfo::StringOrAnyVar),
                 ParamsInfo::Param("NameB", CBCCSTR("The name of second variable."), CoreInfo::StringOrAnyVar));

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
    _exposedInfo = ExposedInfo(ExposedInfo::Variable(_nameA.c_str(), CBCCSTR("The required variable."), CoreInfo::AnyType),
                               ExposedInfo::Variable(_nameB.c_str(), CBCCSTR("The required variable."), CoreInfo::AnyType));
    return CBExposedTypesInfo(_exposedInfo);
  }

  static CBParametersInfo parameters() { return CBParametersInfo(swapParamsInfo); }

  void setParam(int index, const CBVar &value) {
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
        _target->payload.tableValue.api = &GetGlobals().TableInterface;
        _target->payload.tableValue.opaque = new CBMap();
      }

      if (!_key.isVariable()) {
        auto &kv = _key.get();
        _cell = _target->payload.tableValue.api->tableAt(_target->payload.tableValue, kv.payload.stringValue);

        auto &seq = *_cell;
        if (seq.valueType != Seq) {
          seq.valueType = Seq;
          seq.payload.seqValue = {};
        }
      } else {
        return; // we will check during activate
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

  void fillVariableCell() {
    auto &kv = _key.get();
    _cell = _target->payload.tableValue.api->tableAt(_target->payload.tableValue, kv.payload.stringValue);

    auto &seq = *_cell;
    if (seq.valueType != Seq) {
      seq.valueType = Seq;
      seq.payload.seqValue = {};
    }
  }

  void warmup(CBContext *context) {
    if (_global)
      _target = referenceGlobalVariable(context, _name.c_str());
    else
      _target = referenceVariable(context, _name.c_str());
    _key.warmup(context);
    initSeq();
  }

  CBExposedTypesInfo exposedVariables() { return CBExposedTypesInfo(_exposedInfo); }

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

  static inline ParamsInfo pushParams =
      ParamsInfo(variableParamsInfo, ParamsInfo::Param("Clear",
                                                       CBCCSTR("If we should clear this sequence at every chain iteration; "
                                                               "works only if this is the first push; default: true."),
                                                       CoreInfo::BoolType));

  static CBParametersInfo parameters() { return CBParametersInfo(pushParams); }

  void setParam(int index, const CBVar &value) {
    if (index < variableParamsInfoLen)
      VariableBase::setParam(index, value);
    else if (index == variableParamsInfoLen + 0) {
      _clear = value.payload.boolValue;
    }
  }

  CBVar getParam(int index) {
    if (index < variableParamsInfoLen)
      return VariableBase::getParam(index);
    else if (index == variableParamsInfoLen + 0)
      return Var(_clear);
    throw CBException("Param index out of range.");
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    const auto updateSeqInfo = [this, &data] {
      _seqInfo.basicType = Seq;
      _seqInnerInfo = data.inputType;
      _seqInfo.seqTypes = {&_seqInnerInfo, 1, 0};
      if (_global) {
        _exposedInfo = ExposedInfo(ExposedInfo::GlobalVariable(_name.c_str(), CBCCSTR("The exposed sequence."), _seqInfo, true));
      } else {
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(_name.c_str(), CBCCSTR("The exposed sequence."), _seqInfo, true));
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
      if (!_key.isVariable()) {
        CBVar kv = _key;
        chainblocks::arrayPush(_tableInfo.table.keys, kv.payload.stringValue);
      }
      if (_global) {
        _exposedInfo =
            ExposedInfo(ExposedInfo::GlobalVariable(_name.c_str(), CBCCSTR("The exposed table."), CBTypeInfo(_tableInfo), true));
      } else {
        _exposedInfo =
            ExposedInfo(ExposedInfo::Variable(_name.c_str(), CBCCSTR("The exposed table."), CBTypeInfo(_tableInfo), true));
      }
    };

    if (_isTable) {
      for (uint32_t i = 0; data.shared.len > i; i++) {
        if (data.shared.elements[i].name == _name && data.shared.elements[i].exposedType.table.types.elements) {
          auto &tableKeys = data.shared.elements[i].exposedType.table.keys;
          auto &tableTypes = data.shared.elements[i].exposedType.table.types;
          for (uint32_t y = 0; y < tableKeys.len; y++) {
            // if we got key it's not a variable
            CBVar kv = _key;
            if (strcmp(kv.payload.stringValue, tableKeys.elements[y]) == 0 && tableTypes.elements[y].basicType == Seq) {
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
    if (unlikely(_isTable && _key.isVariable())) {
      fillVariableCell();
    }

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
  ParamVar _types{Var::Enum(BasicTypes::Any, CoreCC, 'type')};
  Types _seqTypes{};
  std::deque<Types> _innerTypes;

  static inline ParamsInfo pushParams =
      ParamsInfo(variableParamsInfo,
                 ParamsInfo::Param("Clear",
                                   CBCCSTR("If we should clear this sequence at every chain iteration; "
                                           "works only if this is the first push; default: true."),
                                   CoreInfo::BoolType),
                 ParamsInfo::Param("Types", CBCCSTR("The sequence inner types to forward declare."), CoreInfo2::BasicTypesTypes));

  static CBParametersInfo parameters() { return CBParametersInfo(pushParams); }

  void setParam(int index, const CBVar &value) {
    if (index < variableParamsInfoLen)
      VariableBase::setParam(index, value);
    else if (index == variableParamsInfoLen + 0) {
      _clear = value.payload.boolValue;
    } else if (index == variableParamsInfoLen + 1) {
      _types = value;
    }
  }

  CBVar getParam(int index) {
    if (index < variableParamsInfoLen)
      return VariableBase::getParam(index);
    else if (index == variableParamsInfoLen + 0)
      return Var(_clear);
    else if (index == variableParamsInfoLen + 1)
      return _types;
    throw CBException("Param index out of range.");
  }

  void addType(Types &inner, BasicTypes type) {
    switch (type) {
    case BasicTypes::None:
      inner._types.emplace_back(CoreInfo::NoneType);
      break;
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
    case BasicTypes::Audio:
      inner._types.emplace_back(CoreInfo::AudioType);
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
      if (!_key.isVariable()) {
        CBVar kv = _key;
        chainblocks::arrayPush(_tableInfo.table.keys, kv.payload.stringValue);
      }
      if (_global) {
        _exposedInfo =
            ExposedInfo(ExposedInfo::GlobalVariable(_name.c_str(), CBCCSTR("The exposed table."), CBTypeInfo(_tableInfo), true));
      } else {
        _exposedInfo =
            ExposedInfo(ExposedInfo::Variable(_name.c_str(), CBCCSTR("The exposed table."), CBTypeInfo(_tableInfo), true));
      }
    };

    // Ensure variable did not exist
    if (!_isTable) {
      for (uint32_t i = 0; i < data.shared.len; i++) {
        auto &reference = data.shared.elements[i];
        if (strcmp(reference.name, _name.c_str()) == 0) {
          throw ComposeError("Sequence - Variable " + _name + " already exists.");
        }
      }
    } else {
      for (uint32_t i = 0; data.shared.len > i; i++) {
        if (data.shared.elements[i].name == _name && data.shared.elements[i].exposedType.table.types.elements) {
          auto &tableKeys = data.shared.elements[i].exposedType.table.keys;
          for (uint32_t y = 0; y < tableKeys.len; y++) {
            // if here, key is not variable
            CBVar kv = _key;
            if (strcmp(kv.payload.stringValue, tableKeys.elements[y]) == 0) {
              throw ComposeError("Sequence - Variable " + std::string(kv.payload.stringValue) + " in table " + _name +
                                 " already exists.");
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
        _exposedInfo = ExposedInfo(ExposedInfo::GlobalVariable(_name.c_str(), CBCCSTR("The exposed sequence."), stype, true));
      } else {
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(_name.c_str(), CBCCSTR("The exposed sequence."), stype, true));
      }
    } else {
      updateTableInfo();
    }

    return data.inputType;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(_isTable && _key.isVariable())) {
      fillVariableCell();
    }

    if (_clear) {
      auto &seq = *_cell;
      chainblocks::arrayResize(seq.payload.seqValue, 0);
    }

    return input;
  }
};

struct TableDecl : public VariableBase {
  CBTypeInfo _tableInfo{};

  void initTable() {
    if (_isTable) {
      if (_target->valueType != Table) {
        // Not initialized yet
        _target->valueType = Table;
        _target->payload.tableValue.api = &GetGlobals().TableInterface;
        _target->payload.tableValue.opaque = new CBMap();
      }

      if (!_key.isVariable()) {
        auto &kv = _key.get();
        _cell = _target->payload.tableValue.api->tableAt(_target->payload.tableValue, kv.payload.stringValue);

        auto table = _cell;
        if (table->valueType != Table) {
          // Not initialized yet
          table->valueType = Table;
          table->payload.tableValue.api = &GetGlobals().TableInterface;
          table->payload.tableValue.opaque = new CBMap();
        }
      } else {
        return; // we will check during activate
      }
    } else {
      if (_target->valueType != Table) {
        _target->valueType = Table;
        _target->payload.tableValue.api = &GetGlobals().TableInterface;
        _target->payload.tableValue.opaque = new CBMap();
      }
      _cell = _target;
    }

    assert(_cell);
  }

  void fillVariableCell() {
    auto &kv = _key.get();
    _cell = _target->payload.tableValue.api->tableAt(_target->payload.tableValue, kv.payload.stringValue);

    auto table = _cell;
    if (table->valueType != Table) {
      // Not initialized yet
      table->valueType = Table;
      table->payload.tableValue.api = &GetGlobals().TableInterface;
      table->payload.tableValue.opaque = new CBMap();
    }
  }

  void warmup(CBContext *context) {
    if (_global)
      _target = referenceGlobalVariable(context, _name.c_str());
    else
      _target = referenceVariable(context, _name.c_str());
    _key.warmup(context);
    initTable();
  }

  CBExposedTypesInfo exposedVariables() { return CBExposedTypesInfo(_exposedInfo); }

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  void destroy() {
    if (_tableInfo.table.keys.elements)
      chainblocks::arrayFree(_tableInfo.table.keys);
    if (_tableInfo.table.types.elements)
      chainblocks::arrayFree(_tableInfo.table.types);
  }

  ParamVar _types{Var::Enum(BasicTypes::Any, CoreCC, 'type')};
  Types _seqTypes{};
  std::deque<Types> _innerTypes;

  static inline ParamsInfo pushParams =
      ParamsInfo(variableParamsInfo,
                 ParamsInfo::Param("Types", CBCCSTR("The table inner types to forward declare."), CoreInfo2::BasicTypesTypes));

  static CBParametersInfo parameters() { return CBParametersInfo(pushParams); }

  void setParam(int index, const CBVar &value) {
    if (index < variableParamsInfoLen)
      VariableBase::setParam(index, value);
    else if (index == variableParamsInfoLen + 0) {
      _types = value;
    }
  }

  CBVar getParam(int index) {
    if (index < variableParamsInfoLen)
      return VariableBase::getParam(index);
    else if (index == variableParamsInfoLen + 0)
      return _types;
    throw CBException("Param index out of range.");
  }

  void addType(Types &inner, BasicTypes type) {
    switch (type) {
    case BasicTypes::None:
      inner._types.emplace_back(CoreInfo::NoneType);
      break;
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
    case BasicTypes::Audio:
      inner._types.emplace_back(CoreInfo::AudioType);
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
      auto stype = CBTypeInfo{CBType::Table, {.table = {.keys = {nullptr, 0, 0}, .types = _seqTypes}}};
      chainblocks::arrayPush(_tableInfo.table.types, stype);
      if (!_key.isVariable()) {
        CBVar kv = _key;
        chainblocks::arrayPush(_tableInfo.table.keys, kv.payload.stringValue);
      }
      if (_global) {
        _exposedInfo =
            ExposedInfo(ExposedInfo::GlobalVariable(_name.c_str(), CBCCSTR("The exposed table."), CBTypeInfo(_tableInfo), true));
      } else {
        _exposedInfo =
            ExposedInfo(ExposedInfo::Variable(_name.c_str(), CBCCSTR("The exposed table."), CBTypeInfo(_tableInfo), true));
      }
    };

    // Ensure variable did not exist
    if (!_isTable) {
      for (uint32_t i = 0; i < data.shared.len; i++) {
        auto &reference = data.shared.elements[i];
        if (strcmp(reference.name, _name.c_str()) == 0) {
          throw ComposeError("Table - Variable " + _name + " already exists.");
        }
      }
    } else {
      for (uint32_t i = 0; data.shared.len > i; i++) {
        if (data.shared.elements[i].name == _name && data.shared.elements[i].exposedType.table.types.elements) {
          auto &tableKeys = data.shared.elements[i].exposedType.table.keys;
          for (uint32_t y = 0; y < tableKeys.len; y++) {
            // if here, key is not variable
            CBVar kv = _key;
            if (strcmp(kv.payload.stringValue, tableKeys.elements[y]) == 0) {
              throw ComposeError("Table - Variable " + std::string(kv.payload.stringValue) + " in table " + _name +
                                 " already exists.");
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
      auto stype = CBTypeInfo{CBType::Table, {.table = {.keys = {nullptr, 0, 0}, .types = _seqTypes}}};
      if (_global) {
        _exposedInfo = ExposedInfo(ExposedInfo::GlobalVariable(_name.c_str(), CBCCSTR("The exposed table."), stype, true));
      } else {
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(_name.c_str(), CBCCSTR("The exposed table."), stype, true));
      }
    } else {
      updateTableInfo();
    }

    return data.inputType;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(_isTable && _key.isVariable())) {
      fillVariableCell();
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
        _target->payload.tableValue.api = &GetGlobals().TableInterface;
        _target->payload.tableValue.opaque = new CBMap();
      }

      if (!_key.isVariable()) {
        CBVar kv = _key;
        _cell = _target->payload.tableValue.api->tableAt(_target->payload.tableValue, kv.payload.stringValue);
      } else {
        return; // checked during activate
      }
    } else {
      _cell = _target;
    }

    assert(_cell);
  }

  void fillVariableCell() {
    auto &kv = _key.get();
    _cell = _target->payload.tableValue.api->tableAt(_target->payload.tableValue, kv.payload.stringValue);
  }

  void warmup(CBContext *context) {
    if (_global)
      _target = referenceGlobalVariable(context, _name.c_str());
    else
      _target = referenceVariable(context, _name.c_str());
    _key.warmup(context);
    initSeq();
  }

  CBExposedTypesInfo requiredVariables() {
    if (_name.size() > 0) {
      if (_isTable) {
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(_name.c_str(), CBCCSTR("The required table."), CoreInfo::AnyTableType));
      } else {
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(_name.c_str(), CBCCSTR("The required variable."), CoreInfo::AnyType));
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
    if (unlikely(_isTable && _key.isVariable())) {
      fillVariableCell();
    }

    if (likely(_cell->valueType == Seq)) {
      return Var(int64_t(_cell->payload.seqValue.len));
    } else if (_cell->valueType == Table) {
      return Var(int64_t(_cell->payload.tableValue.api->tableSize(_cell->payload.tableValue)));
    } else if (_cell->valueType == Bytes) {
      return Var(int64_t(_cell->payload.bytesSize));
    } else if (_cell->valueType == String) {
      return Var(int64_t(_cell->payload.stringLen > 0 || _cell->payload.stringValue == nullptr
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
    if (unlikely(_isTable && _key.isVariable())) {
      fillVariableCell();
    }

    if (likely(_cell->valueType == Seq)) {
      // notice this is fine because destroyVar will destroy .cap later
      // so we make sure we are not leaking Vars
      chainblocks::arrayResize(_cell->payload.seqValue, 0);

      // sometimes we might have as input the same _cell!
      // this is kind of a hack but helps UX
      // we in that case output the same _cell with adjusted len!
      if (input.payload.seqValue.elements == _cell->payload.seqValue.elements)
        const_cast<CBVar &>(input).payload.seqValue.len = 0;
    }

    return input;
  }
};

struct Drop : SeqUser {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(_isTable && _key.isVariable())) {
      fillVariableCell();
    }

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
    if (unlikely(_isTable && _key.isVariable())) {
      fillVariableCell();
    }

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
        if (data.shared.elements[i].name == _name && data.shared.elements[i].exposedType.table.types.elements) {
          auto &tableKeys = data.shared.elements[i].exposedType.table.keys;
          auto &tableTypes = data.shared.elements[i].exposedType.table.types;
          for (uint32_t y = 0; y < tableKeys.len; y++) {
            // if here _key is not variable
            CBVar kv = _key;
            if (strcmp(kv.payload.stringValue, tableKeys.elements[y]) == 0 && tableTypes.elements[y].basicType == Seq) {
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
    if (unlikely(_isTable && _key.isVariable())) {
      fillVariableCell();
    }

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
        if (data.shared.elements[i].name == _name && data.shared.elements[i].exposedType.table.types.elements) {
          auto &tableKeys = data.shared.elements[i].exposedType.table.keys;
          auto &tableTypes = data.shared.elements[i].exposedType.table.types;
          for (uint32_t y = 0; y < tableKeys.len; y++) {
            // if here _key is not variable
            CBVar kv = _key;
            if (strcmp(kv.payload.stringValue, tableKeys.elements[y]) == 0 && tableTypes.elements[y].basicType == Seq) {
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
    if (unlikely(_isTable && _key.isVariable())) {
      fillVariableCell();
    }

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
    static_assert(sizeof(*arr.elements) == sizeof(CBVar), "Wrong seq elements size!");
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
      "Indices/Keys", CBCCSTR("One or more indices/keys to extract from a sequence/table."), CoreInfo::TakeTypes));
  static CBOptionalString help() {
    return CBCCSTR("Extracts one or more elements/key-values from a sequence or a table by using the provided sequence "
                   "index/indices or table key(s). Operation is non-destructive; doesn't modify target sequence/table.");
  }

  CBSeq _cachedSeq{};
  CBVar _output{};
  CBVar _indices{};
  CBVar *_indicesVar{nullptr};
  ExposedInfo _exposedInfo{};
  bool _seqOutput{false};
  bool _tableOutput{false};

  CBVar _vectorOutput{};
  const VectorTypeTraits *_vectorInputType{nullptr};
  const VectorTypeTraits *_vectorOutputType{nullptr};
  const NumberConversion *_vectorConversion{nullptr};

  Type _seqOutputType{};
  std::vector<CBTypeInfo> _seqOutputTypes;

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
      chainblocks::arrayFree(_cachedSeq);
    }
  }

  static CBTypesInfo inputTypes() { return CoreInfo::Indexables; }
  static CBOptionalString inputHelp() {
    return CBCCSTR("The sequence or table from which elements/key-values have to be extracted.");
  }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static CBOptionalString outputHelp() { return CBCCSTR("The extracted elements/key-values."); }

  static CBParametersInfo parameters() { return CBParametersInfo(indicesParamsInfo); }

  CBTypeInfo compose(const CBInstanceData &data) {
    bool valid = false;
    bool isTable = data.inputType.basicType == Table;
    // Figure if we output a sequence or not
    if (_indices.valueType == Seq) {
      if (_indices.payload.seqValue.len > 0) {
        if ((_indices.payload.seqValue.elements[0].valueType == Int && !isTable) ||
            (_indices.payload.seqValue.elements[0].valueType == String && isTable)) {
          _seqOutput = true;
          valid = true;
        }
      }
    } else if ((!isTable && _indices.valueType == Int) || (isTable && _indices.valueType == String)) {
      _seqOutput = false;
      valid = true;
    } else { // ContextVar
      for (auto &info : data.shared) {
        if (strcmp(info.name, _indices.payload.stringValue) == 0) {
          if (info.exposedType.basicType == Seq && info.exposedType.seqTypes.len == 1 &&
              ((info.exposedType.seqTypes.elements[0].basicType == Int && !isTable) ||
               (info.exposedType.seqTypes.elements[0].basicType == String && isTable))) {
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
            auto msg = "Take indices variable " + std::string(info.name) + " expected to be either Seq, Int or String";
            throw CBException(msg);
          }
        }
      }
    }

    _tableOutput = isTable;

    if (!valid)
      throw CBException("Take, invalid indices or malformed input.");

    if (data.inputType.basicType == Seq) {
      OVERRIDE_ACTIVATE(data, activateSeq);
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
    } else {
      _vectorInputType = VectorTypeLookup::getInstance().get(data.inputType.basicType);
      if (_vectorInputType) {
        if (_seqOutput)
          OVERRIDE_ACTIVATE(data, activateVector);
        else
          OVERRIDE_ACTIVATE(data, activateNumber);

        uint8_t indexTableLength = _seqOutput ? (uint8_t)_indices.payload.seqValue.len : 1;
        _vectorOutputType = VectorTypeLookup::getInstance().findCompatibleType(_vectorInputType->isInteger, indexTableLength);
        if (!_vectorOutputType) {
          std::stringstream errorMsgStream;
          errorMsgStream << "Take: No " << (_vectorInputType->isInteger ? "integer" : "float") << " vector type exists that fits "
                         << indexTableLength << "elements";
          std::string errorMsg = errorMsgStream.str();
          CBLOG_ERROR(errorMsg.c_str());
          throw ComposeError(errorMsg);
        }

        _vectorConversion =
            NumberTypeLookup::getInstance().getConversion(_vectorInputType->numberType, _vectorOutputType->numberType);
        if (!_vectorConversion) {
          std::stringstream errorMsgStream;
          errorMsgStream << "Take: No conversion from " << _vectorInputType->name << " to " << _vectorOutputType->name
                         << " exists";
          std::string errorMsg = errorMsgStream.str();
          CBLOG_ERROR(errorMsg.c_str());
          throw ComposeError(errorMsg);
        }

        _vectorOutput.valueType = _vectorOutputType->cbType;
        return _vectorOutputType->type;
      } else if (data.inputType.basicType == Bytes) {
        OVERRIDE_ACTIVATE(data, activateBytes);
        if (_seqOutput) {
          return CoreInfo::IntSeqType;
        } else {
          return CoreInfo::IntType;
        }
      } else if (data.inputType.basicType == String) {
        OVERRIDE_ACTIVATE(data, activateString);
        if (_seqOutput) {
          return CoreInfo::StringSeqType;
        } else {
          return CoreInfo::StringType;
        }
      } else if (data.inputType.basicType == Table) {
        OVERRIDE_ACTIVATE(data, activateTable);
        if (data.inputType.table.keys.len > 0 && (_indices.valueType == String || _indices.valueType == Seq)) {
          // we can fully reconstruct a type in this case
          if (data.inputType.table.keys.len != data.inputType.table.types.len) {
            CBLOG_ERROR("Table input type: {}", data.inputType);
            throw ComposeError("Take: Expected same number of types for numer of "
                               "keys in table input.");
          }

          if (_seqOutput) {
            _seqOutputTypes.clear();
            for (uint32_t j = 0; j < _indices.payload.seqValue.len; j++) {
              auto &record = _indices.payload.seqValue.elements[j];
              if (record.valueType != String) {
                CBLOG_ERROR("Expected a sequence of strings, but found: {}", _indices);
                throw ComposeError("Take: Expected a sequence of strings as keys.");
              }
              for (uint32_t i = 0; i < data.inputType.table.keys.len; i++) {
                if (strcmp(record.payload.stringValue, data.inputType.table.keys.elements[i]) == 0) {
                  _seqOutputTypes.emplace_back(data.inputType.table.types.elements[i]);
                }
              }
            }
            // if types is 0 we did not match any
            if (_seqOutputTypes.size() == 0) {
              CBLOG_ERROR("Table input type: {} missing keys: {}", data.inputType, _indices);
              throw ComposeError("Take: Failed to find a matching keys in the "
                                 "input type table");
            }
            _seqOutputType = Type::SeqOf(CBTypesInfo{_seqOutputTypes.data(), uint32_t(_seqOutputTypes.size()), 0});
            return _seqOutputType;
          } else {
            for (uint32_t i = 0; i < data.inputType.table.keys.len; i++) {
              if (strcmp(_indices.payload.stringValue, data.inputType.table.keys.elements[i]) == 0) {
                return data.inputType.table.types.elements[i];
              }
            }
            // we didn't match any key...
            // and so we just return any type to allow extra keys
            return CoreInfo::AnyType;
          }
        } else {
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
      }
    }

    throw CBException("Take, invalid input type or not implemented.");
  }

  CBExposedTypesInfo requiredVariables() {
    if (_indices.valueType == ContextVar) {
      if (_seqOutput)
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(_indices.payload.stringValue, CBCCSTR("The required variables."),
                                                         _tableOutput ? CoreInfo::StringSeqType : CoreInfo::IntSeqType));
      else
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(_indices.payload.stringValue, CBCCSTR("The required variable."),
                                                         _tableOutput ? CoreInfo::StringType : CoreInfo::IntType));
      return CBExposedTypesInfo(_exposedInfo);
    } else {
      return {};
    }
  }

  void setParam(int index, const CBVar &value) {
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
    OutOfRangeEx(int64_t len, int64_t index) : ActivationError("Take out of range!") {
      CBLOG_ERROR("Out of range! len: {} wanted index: {}", len, index);
    }
  };

  void warmup(CBContext *context) {
    if (_indices.valueType == ContextVar && !_indicesVar) {
      _indicesVar = referenceVariable(context, _indices.payload.stringValue);
    }
  }

#define ACTIVATE_INDEXABLE(__name__, __len__, __val__)                            \
  ALWAYS_INLINE CBVar __name__(CBContext *context, const CBVar &input) {          \
    const auto inputLen = size_t(__len__);                                       \
    const auto &indices = _indicesVar ? *_indicesVar : _indices;                  \
    if (likely(!_seqOutput)) {                                                    \
      const auto index = indices.payload.intValue;                                \
      if (index >= inputLen || index < 0) {                                       \
        throw OutOfRangeEx(inputLen, index);                                      \
      }                                                                           \
      return __val__;                                                             \
    } else {                                                                      \
      const uint32_t nindices = indices.payload.seqValue.len;                     \
      chainblocks::arrayResize(_cachedSeq, nindices);                             \
      for (uint32_t i = 0; nindices > i; i++) {                                   \
        const auto index = indices.payload.seqValue.elements[i].payload.intValue; \
        if (index >= inputLen || index < 0) {                                     \
          throw OutOfRangeEx(inputLen, index);                                    \
        }                                                                         \
        _cachedSeq.elements[i] = __val__;                                         \
      }                                                                           \
      return Var(_cachedSeq);                                                     \
    }                                                                             \
  }

  ACTIVATE_INDEXABLE(activateSeq, input.payload.seqValue.len, input.payload.seqValue.elements[index])
  ACTIVATE_INDEXABLE(activateString, CBSTRLEN(input), Var(input.payload.stringValue[index]))
  ACTIVATE_INDEXABLE(activateBytes, input.payload.bytesSize, Var(input.payload.bytesValue[index]))

  CBVar activateTable(CBContext *context, const CBVar &input) {
    // TODO, if the strings are static at compose time, make sure to cache the
    // return value
    const auto &indices = _indicesVar ? *_indicesVar : _indices;
    if (!_seqOutput) {
      const auto key = indices.payload.stringValue;
      const auto val = input.payload.tableValue.api->tableAt(input.payload.tableValue, key);
      return *val;
    } else {
      const uint32_t nkeys = indices.payload.seqValue.len;
      chainblocks::arrayResize(_cachedSeq, nkeys);
      for (uint32_t i = 0; nkeys > i; i++) {
        const auto key = indices.payload.seqValue.elements[i].payload.stringValue;
        const auto val = input.payload.tableValue.api->tableAt(input.payload.tableValue, key);
        _cachedSeq.elements[i] = *val;
      }
      return Var(_cachedSeq);
    }
  }

  CBVar activateVector(CBContext *context, const CBVar &input) {
    const auto &indices = _indices;

    try {
      _vectorConversion->convertMultipleSeq(&input.payload, &_vectorOutput.payload, _vectorInputType->dimension,
                                            indices.payload.seqValue);
    } catch (const NumberConversionOutOfRangeEx &ex) {
      throw OutOfRangeEx(_vectorInputType->dimension, ex.index);
    }

    return _vectorOutput;
  }

  CBVar activateNumber(CBContext *context, const CBVar &input) {
    CBInt index = _indices.payload.intValue;
    if (index < 0 || index >= (CBInt)_vectorInputType->dimension) {
      throw OutOfRangeEx(_vectorInputType->dimension, index);
    }

    const uint8_t *inputPtr = (uint8_t *)&input.payload + _vectorConversion->inStride * index;
    _vectorConversion->convertOne(inputPtr, &_vectorOutput.payload);

    return _vectorOutput;
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
      "Indices", CBCCSTR("One or more indices (counted backwards from the last element) to extract from a sequence."),
      CoreInfo::RTakeTypes));
  static CBOptionalString help() {
    return CBCCSTR("Works exactly like `Take` except that the selection indices are counted backwards from the last element in "
                   "the target sequence. Also, `RTake` works only on sequences, not on tables.");
  }

  static CBParametersInfo parameters() { return CBParametersInfo(indicesParamsInfo); }

  static CBTypesInfo inputTypes() { return CoreInfo::RIndexables; }
  static CBOptionalString inputHelp() { return CBCCSTR("The sequence from which elements have to be extracted."); }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static CBOptionalString outputHelp() { return CBCCSTR("The extracted elements."); }

  CBTypeInfo compose(const CBInstanceData &data) {
    CBTypeInfo result = Take::compose(data);
    if (data.inputType.basicType == Seq) {
      OVERRIDE_ACTIVATE(data, activate);
    } else {
      throw CBException("RTake is only supported on sequence types");
    }
    return result;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    const auto inputLen = input.payload.seqValue.len;
    const auto &indices = _indicesVar ? *_indicesVar : _indices;

    if (!_seqOutput) {
      const auto index = indices.payload.intValue;
      if (index >= inputLen || index < 0) {
        throw OutOfRangeEx(inputLen, index);
      }
      return input.payload.seqValue.elements[inputLen - 1 - index];
    } else {
      const uint32_t nindices = indices.payload.seqValue.len;
      chainblocks::arrayResize(_cachedSeq, nindices);
      for (uint32_t i = 0; nindices > i; i++) {
        const auto index = indices.payload.seqValue.elements[i].payload.intValue;
        if (index >= inputLen || index < 0) {
          throw OutOfRangeEx(inputLen, index);
        }
        _cachedSeq.elements[i] = input.payload.seqValue.elements[inputLen - 1 - index];
      }
      return Var(_cachedSeq);
    }
  }
};

struct Slice {
  static inline ParamsInfo indicesParamsInfo = ParamsInfo(
      ParamsInfo::Param("From",
                        CBCCSTR("The position/index of the first character or element that is to be extracted (including). "
                                "Negative position/indices simply loop over the target string/sequence counting backwards."),
                        CoreInfo::IntsVar),
      ParamsInfo::Param("To",
                        CBCCSTR("The position/index of the last character or element that is to be extracted (excluding). "
                                "Negative position/indices simply loop over the target string/sequence counting backwards."),
                        CoreInfo::IntsVarOrNone),
      ParamsInfo::Param("Step",
                        CBCCSTR("The increment between each position/index. Chooses every nth sample to extract, where n is the "
                                "increment. Value has to be greater than zero."),
                        CoreInfo::IntType));
  static CBOptionalString help() {
    return CBCCSTR("Extracts characters from a string or elements from a sequence based on the start and end positions/indices "
                   "and an increment parameter. Operation is non-destructive; the target string/sequence is not modified.");
  }

  CBSeq _cachedSeq{};
  std::vector<uint8_t> _cachedBytes{};
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
      if (_step > 1) {
        // we cloned in this case
        for (auto i = _cachedSeq.len; i > 0; i--) {
          destroyVar(_cachedSeq.elements[i - 1]);
        }
      }
      chainblocks::arrayFree(_cachedSeq);
    }
  }

  static inline Types InputTypes{{CoreInfo::AnySeqType, CoreInfo::BytesType, CoreInfo::StringType}};

  static CBTypesInfo inputTypes() { return InputTypes; }
  static CBOptionalString inputHelp() {
    return CBCCSTR("The string or sequence from which characters/elements have to be extracted.");
  }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static CBOptionalString outputHelp() { return CBCCSTR("The extracted characters/elements."); }

  static CBParametersInfo parameters() { return CBParametersInfo(indicesParamsInfo); }

  CBTypeInfo compose(const CBInstanceData &data) {
    bool valid = false;

    if (_from.valueType == Int) {
      valid = true;
    } else { // ContextVar
      for (auto &info : data.shared) {
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
      for (auto &info : data.shared) {
        if (strcmp(info.name, _to.payload.stringValue) == 0) {
          valid = true;
          break;
        }
      }
    }

    if (!valid)
      throw CBException("Slice, invalid To variable.");

    if (data.inputType.basicType == Seq) {
      OVERRIDE_ACTIVATE(data, activateSeq);
    } else if (data.inputType.basicType == Bytes) {
      OVERRIDE_ACTIVATE(data, activateBytes);
    } else if (data.inputType.basicType == String) {
      OVERRIDE_ACTIVATE(data, activateString);
    }

    return data.inputType;
  }

  CBExposedTypesInfo requiredVariables() {
    if (_from.valueType == ContextVar && _to.valueType == ContextVar) {
      _exposedInfo =
          ExposedInfo(ExposedInfo::Variable(_from.payload.stringValue, CBCCSTR("The required variable."), CoreInfo::IntType),
                      ExposedInfo::Variable(_to.payload.stringValue, CBCCSTR("The required variable."), CoreInfo::IntType));
      return CBExposedTypesInfo(_exposedInfo);
    } else if (_from.valueType == ContextVar) {
      _exposedInfo =
          ExposedInfo(ExposedInfo::Variable(_from.payload.stringValue, CBCCSTR("The required variable."), CoreInfo::IntType));
      return CBExposedTypesInfo(_exposedInfo);
    } else if (_to.valueType == ContextVar) {
      _exposedInfo =
          ExposedInfo(ExposedInfo::Variable(_to.payload.stringValue, CBCCSTR("The required variable."), CoreInfo::IntType));
      return CBExposedTypesInfo(_exposedInfo);
    } else {
      return {};
    }
  }

  void setParam(int index, const CBVar &value) {
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
    OutOfRangeEx(int64_t len, int64_t from, int64_t to) : ActivationError("Slice out of range!") {
      CBLOG_ERROR("Out of range! from: {} to: {} len: {}", from, to, len);
    }
  };

  CBVar activateBytes(CBContext *context, const CBVar &input) {
    if (_from.valueType == ContextVar && !_fromVar) {
      _fromVar = referenceVariable(context, _from.payload.stringValue);
    }
    if (_to.valueType == ContextVar && !_toVar) {
      _toVar = referenceVariable(context, _to.payload.stringValue);
    }

    const auto inputLen = input.payload.bytesSize;
    const auto &vfrom = _fromVar ? *_fromVar : _from;
    const auto &vto = _toVar ? *_toVar : _to;
    auto from = vfrom.payload.intValue;
    if (from < 0) {
      from = inputLen + from;
    }
    auto to = vto.valueType == None ? inputLen : vto.payload.intValue;
    if (to < 0) {
      to = inputLen + to;
    }

    if (from > to || to < 0 || to > inputLen) {
      throw OutOfRangeEx(inputLen, from, to);
    }

    const auto len = to - from;
    if (_step == 1) {
      // we don't need to copy anything in this case
      CBVar output{};
      output.valueType = Bytes;
      output.payload.bytesValue = &input.payload.bytesValue[from];
      output.payload.bytesSize = uint32_t(len);
      return output;
    } else if (_step > 1) {
      const auto actualLen = len / _step + (len % _step != 0);
      _cachedBytes.resize(actualLen);
      auto idx = 0;
      for (auto i = from; i < to; i += _step) {
        _cachedBytes[idx] = input.payload.bytesValue[i];
        idx++;
      }
      return Var(&_cachedBytes.front(), uint32_t(actualLen));
    } else {
      throw ActivationError("Slice's Step must be greater then 0");
    }
  }

  CBVar activateString(CBContext *context, const CBVar &input) {
    if (_from.valueType == ContextVar && !_fromVar) {
      _fromVar = referenceVariable(context, _from.payload.stringValue);
    }
    if (_to.valueType == ContextVar && !_toVar) {
      _toVar = referenceVariable(context, _to.payload.stringValue);
    }

    const auto inputLen = input.payload.stringLen > 0 || input.payload.stringValue == nullptr
                              ? input.payload.stringLen
                              : uint32_t(strlen(input.payload.stringValue));
    const auto &vfrom = _fromVar ? *_fromVar : _from;
    const auto &vto = _toVar ? *_toVar : _to;
    auto from = vfrom.payload.intValue;
    if (from < 0) {
      from = inputLen + from;
    }
    auto to = vto.valueType == None ? inputLen : vto.payload.intValue;
    if (to < 0) {
      to = inputLen + to;
    }

    if (from > to || to < 0 || to > inputLen) {
      throw OutOfRangeEx(inputLen, from, to);
    }

    const auto len = to - from;
    if (_step > 0) {
      const auto actualLen = len / _step + (len % _step != 0);
      _cachedBytes.resize(actualLen + 1);
      auto idx = 0;
      for (auto i = from; i < to; i += _step) {
        _cachedBytes[idx] = input.payload.stringValue[i];
        idx++;
      }
      _cachedBytes[idx] = '\0';
      return Var((const char *)_cachedBytes.data(), uint32_t(actualLen));
    } else {
      throw ActivationError("Slice's Step must be greater then 0");
    }
  }

  CBVar activateSeq(CBContext *context, const CBVar &input) {
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
    if (from < 0) {
      from = inputLen + from;
    }
    auto to = vto.valueType == None ? inputLen : vto.payload.intValue;
    if (to < 0) {
      to = inputLen + to;
    }

    if (from > to || to < 0 || to > inputLen) {
      throw OutOfRangeEx(inputLen, from, to);
    }

    const auto len = to - from;
    if (_step == 1) {
      // we don't need to copy anything in this case
      CBVar output{};
      output.valueType = Seq;
      output.payload.seqValue.elements = &input.payload.seqValue.elements[from];
      output.payload.seqValue.len = uint32_t(len);
      return output;
    } else if (_step > 1) {
      const auto actualLen = len / _step + (len % _step != 0);
      chainblocks::arrayResize(_cachedSeq, uint32_t(actualLen));
      auto idx = 0;
      for (auto i = from; i < to; i += _step) {
        cloneVar(_cachedSeq.elements[idx], input.payload.seqValue.elements[i]);
        idx++;
      }
      return Var(_cachedSeq);
    } else {
      throw ActivationError("Slice's Step must be greater then 0");
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) { throw ActivationError("Slice: unreachable code path"); }
};

struct Limit {
  static inline ParamsInfo indicesParamsInfo = ParamsInfo(
      ParamsInfo::Param("Max", CBCCSTR("How many maximum elements to take from the input sequence."), CoreInfo::IntType));

  CBSeq _cachedResult{};
  int64_t _max = 0;

  void destroy() {
    if (_cachedResult.elements) {
      chainblocks::arrayFree(_cachedResult);
    }
  }

  static CBTypesInfo inputTypes() { return CoreInfo::AnySeqType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static CBParametersInfo parameters() { return CBParametersInfo(indicesParamsInfo); }

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

  void setParam(int index, const CBVar &value) { _max = value.payload.intValue; }

  CBVar getParam(int index) { return Var(_max); }

  CBVar activate(CBContext *context, const CBVar &input) {
    int64_t inputLen = input.payload.seqValue.len;

    if (_max == 1) {
      auto index = 0;
      if (index >= inputLen) {
        CBLOG_ERROR("Limit out of range! len: {} wanted index: {}", inputLen, index);
        throw ActivationError("Limit out of range!");
      }
      return input.payload.seqValue.elements[index];
    } else {
      // Else it's a seq
      auto nindices = (uint64_t)std::max((int64_t)0, std::min(inputLen, _max));
      chainblocks::arrayResize(_cachedResult, uint32_t(nindices));
      for (uint64_t i = 0; i < nindices; i++) {
        _cachedResult.elements[i] = input.payload.seqValue.elements[i];
      }
      return Var(_cachedResult);
    }
  }
};

struct ForRangeBlock {
  static CBOptionalString help() { return CBCCSTR("Executes a block while an iteration value is within a range."); }

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBOptionalString inputHelp() { return CBCCSTR("The input value is not used and will pass through."); }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static CBOptionalString outputHelp() { return CBCCSTR("The output of this block will be its input."); }

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _from = value;
      break;
    case 1:
      _to = value;
      break;
    case 2:
      _blocks = value;
      break;
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
      return _blocks;
    default:
      throw InvalidParameterIndex();
    }
  }

  void cleanup() {
    _from.cleanup();
    _to.cleanup();
    _blocks.cleanup();
  }

  void warmup(CBContext *context) {
    _from.warmup(context);
    _to.warmup(context);
    _blocks.warmup(context);
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    auto dataCopy = data;
    dataCopy.inputType = CoreInfo::IntType;

    _blocks.compose(dataCopy);

    return data.inputType;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto from = _from.get().payload.intValue;
    auto to = _to.get().payload.intValue;

    CBVar output{};
    CBVar item{};
    if (from < to) {
      for (auto i = from; i <= to; i++) {
        item = Var(i);
        auto state = _blocks.activate<true>(context, item, output);
        if (state != CBChainState::Continue)
          break;
      }
    } else if (from > to) {
      for (auto i = from; i >= to; i--) {
        item = Var(i);
        auto state = _blocks.activate<true>(context, item, output);
        if (state != CBChainState::Continue)
          break;
      }
    }

    return input;
  }

private:
  static inline Parameters _params = {
      {"From", CBCCSTR("The initial iteration value (inclusive)."), {CoreInfo::IntType, CoreInfo::IntVarType}},
      {"To", CBCCSTR("The final iteration value (inclusive)."), {CoreInfo::IntType, CoreInfo::IntVarType}},
      {"Action",
       CBCCSTR("The action to perform at each iteration. The current iteration "
               "value will be passed as input."),
       {CoreInfo::BlocksOrNone}}};

  ParamVar _from{Var(0)};
  ParamVar _to{Var(1)};
  BlocksVar _blocks{};
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

  static inline Parameters _params{{"Action", CBCCSTR("The blocks to repeat."), CoreInfo::Blocks},
                                   {"Times", CBCCSTR("How many times we should repeat the action."), CoreInfo::IntsVar},
                                   {"Forever", CBCCSTR("If we should repeat the action forever."), {CoreInfo::BoolType}},
                                   {"Until",
                                    CBCCSTR("Optional blocks to use as predicate to continue repeating "
                                            "until it's true"),
                                    CoreInfo::BlocksOrNone}};

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
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
      throw ComposeError("Repeat block Until predicate should output a boolean!");
    }
    return data.inputType;
  }

  CBExposedTypesInfo requiredVariables() {
    if (_ctxVar.size() == 0) {
      return {};
    } else {
      _requiredInfo =
          ExposedInfo(ExposedInfo::Variable(_ctxVar.c_str(), CBCCSTR("The Int number of repeats variable."), CoreInfo::IntType));
      return CBExposedTypesInfo(_requiredInfo);
    }
  }

  FLATTEN ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    auto repeats = _forever ? 1 : *_repeats;
    while (repeats) {
      CBVar repeatOutput{};
      CBVar blks = _blks;
      auto state = activateBlocks2(blks.payload.seqValue, context, input, repeatOutput);
      if (state != CBChainState::Continue)
        break;

      if (!_forever)
        repeats--;

      if (_pred) {
        CBVar pres{};
        state = _pred.activate<true>(context, input, pres);
        // logic flow here!
        if (unlikely(state > CBChainState::Return || pres.payload.boolValue))
          break;
      }
    }
    return input;
  }
};

struct Once {
  struct ProcessClock {
    decltype(std::chrono::high_resolution_clock::now()) Start;
    ProcessClock() { Start = std::chrono::high_resolution_clock::now(); }
  } _clock;

  CBTime current;
  CBTimeDiff next;
  CBDuration dsleep;

  BlocksVar _blks;
  ExposedInfo _requiredInfo{};
  CBComposeResult _validation{};
  bool _done{false};
  bool _repeat{false};
  double _repeatTime{0.0};
  CBlock *self{nullptr};

  void cleanup() {
    _blks.cleanup();
    _done = false;
    if (self)
      self->inlineBlockId = NotInline;
  }

  void warmup(CBContext *ctx) {
    _blks.warmup(ctx);
    current = CBClock::now();
    dsleep = CBDuration(_repeatTime);
    next = current + CBDuration(0.0);
  }

  static inline Parameters params{{"Action", CBCCSTR("The block or sequence of blocks to execute."), {CoreInfo::Blocks}},
                                  {"Every",
                                   CBCCSTR("The number of seconds to wait until repeating the action, if 0 "
                                           "the action will happen only once per chain flow execution."),
                                   {CoreInfo::FloatType}}};

  static CBOptionalString help() {
    return CBCCSTR("Executes the block or sequence of blocks with the desired frequency in a chain flow execution.");
  }

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _blks = value;
      break;
    case 1:
      _repeatTime = value.payload.floatValue;
      _repeat = _repeatTime != 0.0;
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
      return Var(_repeatTime);
    default:
      break;
    }
    throw CBException("Parameter out of range.");
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    self = data.block;
    _validation = _blks.compose(data);
    return data.inputType;
  }

  CBExposedTypesInfo exposedVariables() { return _validation.exposedInfo; }

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    if (_repeat) {
      // monitor and reset timer if expired
      current = CBClock::now();
      if (current >= next) {
        _done = false;
      }
    }

    if (unlikely(!_done)) {
      _done = true;
      CBVar output{};
      _blks.activate(context, input, output);
      if (!_repeat) {
        // let's cheat in this case and stop triggering this call
        self->inlineBlockId = NoopBlock;
      } else {
        next = next + dsleep;
      }
    }

    return input;
  }
};

RUNTIME_CORE_BLOCK_TYPE(Const);
RUNTIME_CORE_BLOCK_TYPE(And);
RUNTIME_CORE_BLOCK_TYPE(Or);
RUNTIME_CORE_BLOCK_TYPE(Not);
RUNTIME_CORE_BLOCK_TYPE(Fail);
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
