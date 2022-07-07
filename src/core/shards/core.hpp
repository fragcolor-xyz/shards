/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#ifndef SH_CORE_SHARDS_CORE
#define SH_CORE_SHARDS_CORE

#include "../shards_macros.hpp"
// circular warning this is self inclusive on purpose
#include "../foundation.hpp"
#include "shards.h"
#include "shards.hpp"
#include "common_types.hpp"
#include "number_types.hpp"
#include <cassert>
#include <cmath>
#include <sstream>

namespace shards {
struct CoreInfo2 {
  // 'type' == 0x74797065 or 1954115685
  static inline EnumInfo<BasicTypes> BasicTypesEnum{"Type", CoreCC, 'type'};
  static inline Type BasicTypesType{{SHType::Enum, {.enumeration = {CoreCC, 'type'}}}};
  static inline Type BasicTypesSeqType{{SHType::Seq, {.seqTypes = BasicTypesType}}};
  static inline Types BasicTypesTypes{{BasicTypesType, BasicTypesSeqType}, true};
};

struct Const {
  static inline shards::ParamsInfo constParamsInfo = shards::ParamsInfo(
      shards::ParamsInfo::Param("Value", SHCCSTR("The constant value to insert in the wire."), CoreInfo::AnyType));

  OwnedVar _value{};
  OwnedVar _clone{};
  SHTypeInfo _innerInfo{};
  VariableResolver resolver;
  std::vector<SHExposedTypeInfo> _dependencies;

  void destroy() { freeDerivedInfo(_innerInfo); }

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static SHParametersInfo parameters() { return SHParametersInfo(constParamsInfo); }

  void setParam(int index, const SHVar &value) { _value = value; }

  SHVar getParam(int index) { return _value; }

  SHTypeInfo compose(const SHInstanceData &data) {
    freeDerivedInfo(_innerInfo);
    _dependencies.clear();
    _innerInfo = deriveTypeInfo(_value, data, &_dependencies);
    if (!_dependencies.empty()) {
      _clone = _value;
      const_cast<Shard *>(data.shard)->inlineShardId = SHInlineShards::NotInline;
    } else {
      _clone = shards::Var::Empty;
      const_cast<Shard *>(data.shard)->inlineShardId = SHInlineShards::CoreConst;
    }
    return _innerInfo;
  }

  SHExposedTypesInfo requiredVariables() { return SHExposedTypesInfo{_dependencies.data(), uint32_t(_dependencies.size())}; }

  void cleanup() { resolver.cleanup(); }

  void warmup(SHContext *context) {
    if (_clone != shards::Var::Empty)
      resolver.warmup(_value, _clone, context);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    // we need to reassign values every frame
    resolver.reassign();
    return _clone;
  }
};

static shards::ParamsInfo compareParamsInfo =
    shards::ParamsInfo(shards::ParamsInfo::Param("Value", SHCCSTR("The value to test against for equality."), CoreInfo::AnyType));

struct BaseOpsBin {
  SHVar _value{};
  SHVar *_target = nullptr;

  void destroy() { destroyVar(_value); }

  void cleanup() {
    if (_target && _value.valueType == ContextVar) {
      releaseVariable(_target);
    }
    _target = nullptr;
  }

  SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  SHTypesInfo outputTypes() { return CoreInfo::BoolType; }

  SHParametersInfo parameters() { return SHParametersInfo(compareParamsInfo); }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      cloneVar(_value, value);
      cleanup();
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _value;
    default:
      return shards::Var::Empty;
    }
  }

  void warmup(SHContext *context) {
    // TODO deep resolve variables like const
    _target = _value.valueType == ContextVar ? referenceVariable(context, _value.payload.stringValue) : &_value;
  }
};

#define LOGIC_OP(NAME, OP)                                                         \
  struct NAME : public BaseOpsBin {                                                \
    FLATTEN ALWAYS_INLINE SHVar activate(SHContext *context, const SHVar &input) { \
      const auto &value = *_target;                                                \
      if (input OP value) {                                                        \
        return shards::Var::True;                                                  \
      }                                                                            \
      return shards::Var::False;                                                   \
    }                                                                              \
  };                                                                               \
  RUNTIME_CORE_SHARD_TYPE(NAME);

LOGIC_OP(Is, ==);
LOGIC_OP(IsNot, !=);
LOGIC_OP(IsMore, >);
LOGIC_OP(IsLess, <);
LOGIC_OP(IsMoreEqual, >=);
LOGIC_OP(IsLessEqual, <=);

#define LOGIC_ANY_SEQ_OP(NAME, OP)                                                        \
  struct NAME : public BaseOpsBin {                                                       \
    SHVar activate(SHContext *context, const SHVar &input) {                              \
      const auto &value = *_target;                                                       \
      if (input.valueType == Seq && value.valueType == Seq) {                             \
        auto vlen = value.payload.seqValue.len;                                           \
        auto ilen = input.payload.seqValue.len;                                           \
        if (ilen > vlen)                                                                  \
          throw ActivationError("Failed to compare, input len > value len.");             \
        for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {                       \
          if (input.payload.seqValue.elements[i] OP value.payload.seqValue.elements[i]) { \
            return shards::Var::True;                                                     \
          }                                                                               \
        }                                                                                 \
        return shards::Var::False;                                                        \
      } else if (input.valueType == Seq && value.valueType != Seq) {                      \
        for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {                       \
          if (input.payload.seqValue.elements[i] OP value) {                              \
            return shards::Var::True;                                                     \
          }                                                                               \
        }                                                                                 \
        return shards::Var::False;                                                        \
      } else if (input.valueType != Seq && value.valueType == Seq) {                      \
        for (uint32_t i = 0; i < value.payload.seqValue.len; i++) {                       \
          if (input OP value.payload.seqValue.elements[i]) {                              \
            return shards::Var::True;                                                     \
          }                                                                               \
        }                                                                                 \
        return shards::Var::False;                                                        \
      } else if (input.valueType != Seq && value.valueType != Seq) {                      \
        if (input OP value) {                                                             \
          return shards::Var::True;                                                       \
        }                                                                                 \
        return shards::Var::False;                                                        \
      }                                                                                   \
      return shards::Var::False;                                                          \
    }                                                                                     \
  };                                                                                      \
  RUNTIME_CORE_SHARD_TYPE(NAME);

#define LOGIC_ALL_SEQ_OP(NAME, OP)                                                           \
  struct NAME : public BaseOpsBin {                                                          \
    SHVar activate(SHContext *context, const SHVar &input) {                                 \
      const auto &value = *_target;                                                          \
      if (input.valueType == Seq && value.valueType == Seq) {                                \
        auto vlen = value.payload.seqValue.len;                                              \
        auto ilen = input.payload.seqValue.len;                                              \
        if (ilen > vlen)                                                                     \
          throw ActivationError("Failed to compare, input len > value len.");                \
        for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {                          \
          if (!(input.payload.seqValue.elements[i] OP value.payload.seqValue.elements[i])) { \
            return shards::Var::False;                                                       \
          }                                                                                  \
        }                                                                                    \
        return shards::Var::True;                                                            \
      } else if (input.valueType == Seq && value.valueType != Seq) {                         \
        for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {                          \
          if (!(input.payload.seqValue.elements[i] OP value)) {                              \
            return shards::Var::False;                                                       \
          }                                                                                  \
        }                                                                                    \
        return shards::Var::True;                                                            \
      } else if (input.valueType != Seq && value.valueType == Seq) {                         \
        for (uint32_t i = 0; i < value.payload.seqValue.len; i++) {                          \
          if (!(input OP value.payload.seqValue.elements[i])) {                              \
            return shards::Var::False;                                                       \
          }                                                                                  \
        }                                                                                    \
        return shards::Var::True;                                                            \
      } else if (input.valueType != Seq && value.valueType != Seq) {                         \
        if (!(input OP value)) {                                                             \
          return shards::Var::False;                                                         \
        }                                                                                    \
        return shards::Var::True;                                                            \
      }                                                                                      \
      return shards::Var::False;                                                             \
    }                                                                                        \
  };                                                                                         \
  RUNTIME_CORE_SHARD_TYPE(NAME);

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
  RUNTIME_CORE_SHARD_FACTORY(NAME); \
  RUNTIME_SHARD_destroy(NAME);      \
  RUNTIME_SHARD_cleanup(NAME);      \
  RUNTIME_SHARD_warmup(NAME);       \
  RUNTIME_SHARD_inputTypes(NAME);   \
  RUNTIME_SHARD_outputTypes(NAME);  \
  RUNTIME_SHARD_parameters(NAME);   \
  RUNTIME_SHARD_setParam(NAME);     \
  RUNTIME_SHARD_getParam(NAME);     \
  RUNTIME_SHARD_activate(NAME);     \
  RUNTIME_SHARD_END(NAME);

struct Input {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  FLATTEN ALWAYS_INLINE SHVar activate(SHContext *context, const SHVar &input) { return context->wireStack.back()->currentInput; }
};

struct Pause {
  ExposedInfo reqs{};
  static inline Parameters params{
      {"Time",
       SHCCSTR("The amount of time in seconds (can be fractional) to pause "
               "this wire."),
       {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::IntType, CoreInfo::FloatVarType, CoreInfo::IntVarType}}};

  ParamVar time{};

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) { time = value; }

  SHVar getParam(int index) { return time; }

  void warmup(SHContext *context) { time.warmup(context); }

  void cleanup() { time.cleanup(); }

  SHExposedTypesInfo requiredVariables() {
    if (time.isVariable()) {
      reqs = ExposedInfo(ExposedInfo::Variable(time.variableName(), SHCCSTR("The required variable"), CoreInfo::FloatType),
                         ExposedInfo::Variable(time.variableName(), SHCCSTR("The required variable"), CoreInfo::IntType));
    } else {
      reqs = {};
    }
    return SHExposedTypesInfo(reqs);
  }

  FLATTEN ALWAYS_INLINE SHVar activate(SHContext *context, const SHVar &input) {
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
                                   SHCCSTR("The amount of time in milliseconds to pause this wire."),
                                   {CoreInfo::NoneType, CoreInfo::IntType, CoreInfo::IntVarType}}};

  static SHParametersInfo parameters() { return params; }

  SHExposedTypesInfo requiredVariables() {
    if (time.isVariable()) {
      reqs = ExposedInfo(ExposedInfo::Variable(time.variableName(), SHCCSTR("The required variable"), CoreInfo::IntType));
    } else {
      reqs = {};
    }
    return SHExposedTypesInfo(reqs);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
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

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static inline Parameters params{{"Text", SHCCSTR("The comment's text."), {CoreInfo::StringType}}};

  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) { _comment = value.payload.stringValue; }

  SHVar getParam(int index) { return shards::Var(_comment); }

  SHVar activate(SHContext *context, const SHVar &input) {
    // We are a NOOP shard
    SHLOG_FATAL("invalid state");
    return input;
  }
};

struct OnCleanup {
  ShardsVar _shards{};
  SHContext *_context{nullptr};

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static inline Parameters params{{"Shards",
                                   SHCCSTR("The shards to execute on wire's cleanup. Notice that shards "
                                           "that suspend execution are not allowed."),
                                   {CoreInfo::ShardsOrNone}}};

  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) { _shards = value; }

  SHVar getParam(int index) { return _shards; }

  SHTypeInfo compose(const SHInstanceData &data) {
    _shards.compose(data);
    return data.inputType;
  }

  void warmup(SHContext *ctx) {
    _context = ctx;
    _shards.warmup(ctx);
  }

  void cleanup() {
    // also run the shards here!
    if (_context) {
      // cleanup might be called multiple times
      SHVar output{};
      std::string error;
      if (_context->failed()) {
        error = _context->getErrorMessage();
      }
      // we need to reset the state or only the first shard will run
      _context->continueFlow();
      _context->onCleanup = true; // this is kind of a hack
      _shards.activate(_context, shards::Var(error), output);
      // restore the terminal state
      if (error.size() > 0) {
        _context->cancelFlow(error);
      } else {
        // var should not matter at this point
        _context->stopFlow(shards::Var::Empty);
      }
      _context = nullptr;
    }
    // and cleanup after
    _shards.cleanup();
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    // We are a NOOP shard
    SHLOG_FATAL("invalid state");
    return input;
  }
};

struct And {
  static SHOptionalString help() {
    return SHCCSTR("Computes the logical AND between the input of this shard and the output of the next shard.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::BoolType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The first operand to be evaluated."); }

  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The output of this shard will be its input."); }

  SHVar activate(SHContext *context, const SHVar &input) {
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
  static SHOptionalString help() {
    return SHCCSTR("Computes the logical OR between the input of this shard and the output of the next shard.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::BoolType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The first operand to be evaluated."); }

  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The output of this shard will be its input."); }

  SHVar activate(SHContext *context, const SHVar &input) {
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
  static SHOptionalString help() { return SHCCSTR("Computes the logical negation of the input."); }

  static SHTypesInfo inputTypes() { return CoreInfo::BoolType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The value to be negated."); }

  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The negation of the input."); }

  SHVar activate(SHContext *context, const SHVar &input) { return shards::Var(!input.payload.boolValue); }
};

struct IsNone {
  static SHOptionalString help() { return SHCCSTR("Gets whether the type of the input is `None`."); }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The value which type to check against."); }

  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }
  static SHOptionalString outputHelp() { return SHCCSTR("`true` is the type of input is `None`; otherwise, `false`."); }

  SHVar activate(SHContext *context, const SHVar &input) { return shards::Var(input.valueType == None); }
};

struct IsNotNone {
  static SHOptionalString help() { return SHCCSTR("Gets whether the type of the input is different from `None`."); }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The value which type to check against."); }

  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("`true` is the type of input different from `None`; otherwise, `false`.");
  }

  SHVar activate(SHContext *context, const SHVar &input) { return shards::Var(input.valueType != None); }
};

struct Restart {
  // Must ensure input is the same kind of wire root input
  SHTypeInfo compose(const SHInstanceData &data) {
    if (data.wire->inputType->basicType != SHType::None && data.inputType != data.wire->inputType)
      throw ComposeError("Restart input and wire input type mismatch, Restart "
                         "feeds back to the wire input, wire: " +
                         data.wire->name + " expected: " + type2Name(data.wire->inputType->basicType));
    return data.inputType; // actually we are flow stopper
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  SHVar activate(SHContext *context, const SHVar &input) {
    context->restartFlow(input);
    return input;
  }
};

struct Return {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  SHVar activate(SHContext *context, const SHVar &input) {
    context->returnFlow(input);
    return input;
  }
};

struct Fail {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  SHVar activate(SHContext *context, const SHVar &input) {
    context->cancelFlow(input.payload.stringValue);
    return input;
  }
};

struct IsValidNumber {
  static SHTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }
  SHVar activate(SHContext *context, const SHVar &input) { return shards::Var(std::isnormal(input.payload.floatValue)); }
};

struct NaNTo0 {
  static SHTypesInfo inputTypes() { return CoreInfo::FloatOrFloatSeq; }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatOrFloatSeq; }

  SHTypeInfo compose(const SHInstanceData &data) {
    if (data.inputType.basicType == Float) {
      OVERRIDE_ACTIVATE(data, activateSingle);
    } else {
      OVERRIDE_ACTIVATE(data, activateSeq);
    }
    return data.inputType;
  }

  SHVar activateSingle(SHContext *context, const SHVar &input) {
    if (std::isnan(input.payload.floatValue)) {
      return shards::Var(0);
    } else {
      return input;
    }
  }

  SHVar activateSeq(SHContext *context, const SHVar &input) {
    // violate const.. edit in place
    auto violatedInput = const_cast<SHVar &>(input);
    for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {
      auto val = violatedInput.payload.seqValue.elements[i];
      if (std::isnan(val.payload.floatValue)) {
        violatedInput.payload.seqValue.elements[i].payload.floatValue = 0.0;
      }
    }
    return input;
  }

  SHVar activate(SHContext *context, const SHVar &input) { throw ActivationError("Unexpected code path"); }
};

struct VariableBase {
  SHVar *_target{nullptr};
  SHVar *_cell{nullptr};
  std::string _name;
  ParamVar _key{};
  ExposedInfo _exposedInfo{};
  bool _isTable{false};
  bool _global{false};

  static inline ParamsInfo variableParamsInfo =
      ParamsInfo(ParamsInfo::Param("Name", SHCCSTR("The name of the variable."), CoreInfo::StringOrAnyVar),
                 ParamsInfo::Param("Key",
                                   SHCCSTR("The key of the value to read/write from/in the table (this "
                                           "variable will become a table)."),
                                   CoreInfo::StringStringVarOrNone),
                 ParamsInfo::Param("Global",
                                   SHCCSTR("If the variable is or should be available to "
                                           "all of the wires in the same mesh."),
                                   CoreInfo::BoolType));

  static constexpr int variableParamsInfoLen = 3;

  static SHParametersInfo parameters() { return SHParametersInfo(variableParamsInfo); }

  void cleanup() {
    if (_target) {
      releaseVariable(_target);
    }
    _key.cleanup();
    _target = nullptr;
    _cell = nullptr;
  }

  void setParam(int index, const SHVar &value) {
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

  SHVar getParam(int index) {
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
  SHString _tableContentKey{};
  SHTypeInfo _tableContentInfo{};

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  void sanityChecks(const SHInstanceData &data, bool warnIfExists) {
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
          SHLOG_ERROR("Error with variable: {}", _name);
          throw ComposeError("Set/Ref/Update, attempted to write an immutable variable.");
        }
        if (!_isTable && reference.isProtected) {
          SHLOG_ERROR("Error with variable: {}", _name);
          throw ComposeError("Set/Ref/Update, attempted to write a protected variable.");
        }
        if (!_isTable && warnIfExists) {
          SHLOG_INFO("Set - Warning: setting an already exposed variable, use "
                     "Update to avoid this warning, variable: {}",
                     _name);
        }
      }
    }
  }

  void warmup(SHContext *context) {
    if (_global)
      _target = referenceGlobalVariable(context, _name.c_str());
    else
      _target = referenceVariable(context, _name.c_str());
    _key.warmup(context);
  }

  ALWAYS_INLINE SHVar activate(SHContext *context, const SHVar &input) {
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
        _target->payload.tableValue.opaque = new SHMap();
      }

      auto &kv = _key.get();
      SHVar *vptr = _target->payload.tableValue.api->tableAt(_target->payload.tableValue, kv.payload.stringValue);

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
  SHTypeInfo compose(const SHInstanceData &data) {
    sanityChecks(data, true);

    // bake exposed types
    if (_isTable) {
      // we are a table!
      _tableContentInfo = data.inputType;
      if (_key.isVariable()) {
        // only add types info, we don't know keys
        _tableTypeInfo = SHTypeInfo{SHType::Table, {.table = {.types = {&_tableContentInfo, 1, 0}}}};
      } else {
        SHVar kv = _key;
        _tableContentKey = kv.payload.stringValue;
        _tableTypeInfo =
            SHTypeInfo{SHType::Table, {.table = {.keys = {&_tableContentKey, 1, 0}, .types = {&_tableContentInfo, 1, 0}}}};
      }
      if (_global) {
        _exposedInfo =
            ExposedInfo(ExposedInfo::GlobalVariable(_name.c_str(), SHCCSTR("The exposed table."), _tableTypeInfo, true, true));
      } else {
        _exposedInfo =
            ExposedInfo(ExposedInfo::Variable(_name.c_str(), SHCCSTR("The exposed table."), _tableTypeInfo, true, true));
      }
    } else {
      // just a variable!
      if (_global) {
        _exposedInfo = ExposedInfo(
            ExposedInfo::GlobalVariable(_name.c_str(), SHCCSTR("The exposed variable."), SHTypeInfo(data.inputType), true));
      } else {
        _exposedInfo =
            ExposedInfo(ExposedInfo::Variable(_name.c_str(), SHCCSTR("The exposed variable."), SHTypeInfo(data.inputType), true));
      }
    }
    return data.inputType;
  }

  SHExposedTypesInfo exposedVariables() { return SHExposedTypesInfo(_exposedInfo); }
};

struct Ref : public SetBase {
  SHTypeInfo compose(const SHInstanceData &data) {
    sanityChecks(data, true);

    // bake exposed types
    if (_isTable) {
      // we are a table!
      _tableContentInfo = data.inputType;
      if (_key.isVariable()) {
        // only add types info, we don't know keys
        _tableTypeInfo = SHTypeInfo{SHType::Table, {.table = {.types = {&_tableContentInfo, 1, 0}}}};
      } else {
        SHVar kv = _key;
        _tableContentKey = kv.payload.stringValue;
        _tableTypeInfo =
            SHTypeInfo{SHType::Table, {.table = {.keys = {&_tableContentKey, 1, 0}, .types = {&_tableContentInfo, 1, 0}}}};
      }
      _exposedInfo =
          ExposedInfo(ExposedInfo::Variable(_name.c_str(), SHCCSTR("The exposed table."), _tableTypeInfo, false, true));

      // properly link the right call
      const_cast<Shard *>(data.shard)->inlineShardId = SHInlineShards::CoreRefTable;
    } else {
      // just a variable!
      _exposedInfo =
          ExposedInfo(ExposedInfo::Variable(_name.c_str(), SHCCSTR("The exposed variable."), SHTypeInfo(data.inputType), false));

      const_cast<Shard *>(data.shard)->inlineShardId = SHInlineShards::CoreRefRegular;
    }
    return data.inputType;
  }

  SHExposedTypesInfo exposedVariables() { return SHExposedTypesInfo(_exposedInfo); }

  void cleanup() {
    if (_target) {
      // this is a special case
      // Ref will reference previous shard result..
      // Let's cleanup our storage so that release, if calls destroy
      // won't mangle the shard's variable memory
      const auto rc = _target->refcount;
      const auto rcflag = _target->flags & SHVAR_FLAGS_REF_COUNTED;
      memset(_target, 0x0, sizeof(SHVar));
      _target->refcount = rc;
      _target->flags |= rcflag;
      releaseVariable(_target);
    }
    _target = nullptr;

    _cell = nullptr;
    _key.cleanup();
  }

  ALWAYS_INLINE SHVar activate(SHContext *context, const SHVar &input) {
    throw ActivationError("Invalid Ref activation function.");
  }

  ALWAYS_INLINE SHVar activateTable(SHContext *context, const SHVar &input) {
    if (likely(_cell != nullptr)) {
      memcpy(_cell, &input, sizeof(SHVar));
      return input;
    } else {
      if (_target->valueType != Table) {
        // Not initialized yet
        _target->valueType = Table;
        _target->payload.tableValue.api = &GetGlobals().TableInterface;
        _target->payload.tableValue.opaque = new SHMap();
      }

      auto &kv = _key.get();
      SHVar *vptr = _target->payload.tableValue.api->tableAt(_target->payload.tableValue, kv.payload.stringValue);

      // Notice, NO Cloning!
      memcpy(vptr, &input, sizeof(SHVar));

      // use fast cell from now
      if (!_key.isVariable())
        _cell = vptr;

      return input;
    }
  }

  ALWAYS_INLINE SHVar activateRegular(SHContext *context, const SHVar &input) {
    // must keep refcount!
    const auto rc = _target->refcount;
    const auto rcflag = _target->flags & SHVAR_FLAGS_REF_COUNTED;
    memcpy(_target, &input, sizeof(SHVar));
    _target->refcount = rc;
    _target->flags |= rcflag;
    return input;
  }
};

struct Update : public SetBase {
  SHTypeInfo compose(const SHInstanceData &data) {
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
            SHVar kv = _key;
            auto &key = tableKeys.elements[y];
            if (strcmp(kv.payload.stringValue, key) == 0) {
              if (data.inputType != tableTypes.elements[y]) {
                throw SHException("Update: error, update is changing the variable type.");
              }
            }
          }
        }
      }
    } else {
      for (uint32_t i = 0; i < data.shared.len; i++) {
        auto &cv = data.shared.elements[i];
        if (_name == cv.name) {
          if (cv.exposedType.basicType != SHType::Table && data.inputType != cv.exposedType) {
            throw SHException("Update: error, update is changing the variable type.");
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
        _tableTypeInfo = SHTypeInfo{SHType::Table, {.table = {.types = {&_tableContentInfo, 1, 0}}}};
      } else {
        SHVar kv = _key;
        _tableContentKey = kv.payload.stringValue;
        _tableTypeInfo =
            SHTypeInfo{SHType::Table, {.table = {.keys = {&_tableContentKey, 1, 0}, .types = {&_tableContentInfo, 1, 0}}}};
      }
      _exposedInfo = ExposedInfo(ExposedInfo::Variable(_name.c_str(), SHCCSTR("The exposed table."), _tableTypeInfo, true, true));
    } else {
      // just a variable!
      _exposedInfo =
          ExposedInfo(ExposedInfo::Variable(_name.c_str(), SHCCSTR("The exposed variable."), SHTypeInfo(data.inputType), true));
    }

    return data.inputType;
  }

  SHExposedTypesInfo requiredVariables() { return SHExposedTypesInfo(_exposedInfo); }
};

struct Get : public VariableBase {
  SHVar _defaultValue{};
  SHTypeInfo _defaultType{};
  std::vector<SHTypeInfo> _tableTypes{};
  std::vector<SHString> _tableKeys{};
  Shard *_shard{nullptr};

  static inline shards::ParamsInfo getParamsInfo = shards::ParamsInfo(
      variableParamsInfo, shards::ParamsInfo::Param("Default",
                                                    SHCCSTR("The default value to use to infer types and output if the "
                                                            "variable is not set, key is not there and/or type "
                                                            "mismatches."),
                                                    CoreInfo::AnyType));

  static SHParametersInfo parameters() { return SHParametersInfo(getParamsInfo); }

  void setParam(int index, const SHVar &value) {
    if (index < variableParamsInfoLen)
      VariableBase::setParam(index, value);
    else if (index == variableParamsInfoLen + 0) {
      cloneVar(_defaultValue, value);
    }
  }

  SHVar getParam(int index) {
    if (index < variableParamsInfoLen)
      return VariableBase::getParam(index);
    else if (index == variableParamsInfoLen + 0)
      return _defaultValue;
    throw SHException("Param index out of range.");
  }

  void destroy() { freeDerivedInfo(_defaultType); }

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  SHTypeInfo compose(const SHInstanceData &data) {
    _shard = const_cast<Shard *>(data.shard);
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
              SHVar kv = _key;
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
          SHVar kv = _key;
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
          if (cv.exposedType.basicType == SHType::Table && cv.isTableEntry && cv.exposedType.table.types.len == 1) {
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
        auto outputTableType = SHTypeInfo(CoreInfo::AnyTableType);
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

  SHExposedTypesInfo requiredVariables() {
    if (_defaultValue.valueType != None) {
      return {};
    } else {
      if (_isTable) {
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(_name.c_str(), SHCCSTR("The required table."), CoreInfo::AnyTableType));
      } else {
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(_name.c_str(), SHCCSTR("The required variable."), CoreInfo::AnyType));
      }
      return SHExposedTypesInfo(_exposedInfo);
    }
  }

  bool defaultTypeCheck(const SHVar &value) {
    auto warn = false;
    DEFER({
      if (warn) {
        SHLOG_INFO("Get found a variable but it's using the default value because the "
                   "type found did not match with the default type.");
      }
    });
    if (value.valueType != _defaultValue.valueType)
      return false;

    if (value.valueType == SHType::Object && (value.payload.objectVendorId != _defaultValue.payload.objectVendorId ||
                                              value.payload.objectTypeId != _defaultValue.payload.objectTypeId))
      return false;

    if (value.valueType == SHType::Enum && (value.payload.enumVendorId != _defaultValue.payload.enumVendorId ||
                                            value.payload.enumTypeId != _defaultValue.payload.enumTypeId))
      return false;

    return true;
  }

  void warmup(SHContext *context) {
    if (_global)
      _target = referenceGlobalVariable(context, _name.c_str());
    else
      _target = referenceVariable(context, _name.c_str());

    _key.warmup(context);
  }

  void cleanup() {
    // reset shard id
    if (_shard) {
      _shard->inlineShardId = SHInlineShards::NotInline;
    }
    VariableBase::cleanup();
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (unlikely(_cell != nullptr)) {
      // we override shard id, this should not happen
      assert(false);
      return shards::Var::Empty;
    } else {
      if (_isTable) {
        if (_target->valueType == Table) {
          auto &kv = _key.get();
          if (_target->payload.tableValue.api->tableContains(_target->payload.tableValue, kv.payload.stringValue)) {
            // Has it
            SHVar *vptr = _target->payload.tableValue.api->tableAt(_target->payload.tableValue, kv.payload.stringValue);

            if (unlikely(_defaultValue.valueType != None && !defaultTypeCheck(*vptr))) {
              return _defaultValue;
            } else {
              // Pin fast cell
              // skip if variable
              if (!_key.isVariable()) {
                _cell = vptr;
                // override shard internal id
                _shard->inlineShardId = SHInlineShards::CoreGet;
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
          // override shard internal id
          _shard->inlineShardId = SHInlineShards::CoreGet;
          return value;
        }
      }
    }
  }
};

struct Swap {
  static inline shards::ParamsInfo swapParamsInfo =
      shards::ParamsInfo(shards::ParamsInfo::Param("NameA", SHCCSTR("The name of first variable."), CoreInfo::StringOrAnyVar),
                         shards::ParamsInfo::Param("NameB", SHCCSTR("The name of second variable."), CoreInfo::StringOrAnyVar));

  std::string _nameA;
  std::string _nameB;
  SHVar *_targetA{};
  SHVar *_targetB{};
  ExposedInfo _exposedInfo;

  void cleanup() {
    if (_targetA) {
      releaseVariable(_targetA);
      releaseVariable(_targetB);
    }
    _targetA = nullptr;
    _targetB = nullptr;
  }

  void warmup(SHContext *ctx) {
    assert(!_targetA);
    assert(!_targetB);
    _targetA = referenceVariable(ctx, _nameA.c_str());
    _targetB = referenceVariable(ctx, _nameB.c_str());
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  SHExposedTypesInfo requiredVariables() {
    _exposedInfo = ExposedInfo(ExposedInfo::Variable(_nameA.c_str(), SHCCSTR("The required variable."), CoreInfo::AnyType),
                               ExposedInfo::Variable(_nameB.c_str(), SHCCSTR("The required variable."), CoreInfo::AnyType));
    return SHExposedTypesInfo(_exposedInfo);
  }

  static SHParametersInfo parameters() { return SHParametersInfo(swapParamsInfo); }

  void setParam(int index, const SHVar &value) {
    if (index == 0)
      _nameA = value.payload.stringValue;
    else if (index == 1) {
      _nameB = value.payload.stringValue;
    }
  }

  SHVar getParam(int index) {
    if (index == 0)
      return shards::Var(_nameA.c_str());
    else if (index == 1)
      return shards::Var(_nameB.c_str());
    throw SHException("Param index out of range.");
  }

  ALWAYS_INLINE SHVar activate(SHContext *context, const SHVar &input) {
    auto tmp = *_targetA;
    *_targetA = *_targetB;
    *_targetB = tmp;
    return input;
  }
};

struct SeqBase : public VariableBase {
  bool _clear = true;
  SHTypeInfo _tableInfo{};

  void initSeq() {
    if (_isTable) {
      if (_target->valueType != Table) {
        // Not initialized yet
        _target->valueType = Table;
        _target->payload.tableValue.api = &GetGlobals().TableInterface;
        _target->payload.tableValue.opaque = new SHMap();
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

  void warmup(SHContext *context) {
    if (_global)
      _target = referenceGlobalVariable(context, _name.c_str());
    else
      _target = referenceVariable(context, _name.c_str());
    _key.warmup(context);
    initSeq();
  }

  SHExposedTypesInfo exposedVariables() { return SHExposedTypesInfo(_exposedInfo); }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  void destroy() {
    if (_tableInfo.table.keys.elements)
      shards::arrayFree(_tableInfo.table.keys);
    if (_tableInfo.table.types.elements)
      shards::arrayFree(_tableInfo.table.types);
  }
};

struct Push : public SeqBase {
  bool _firstPush = false;
  SHTypeInfo _seqInfo{};
  SHTypeInfo _seqInnerInfo{};

  static inline shards::ParamsInfo pushParams = shards::ParamsInfo(
      variableParamsInfo, shards::ParamsInfo::Param("Clear",
                                                    SHCCSTR("If we should clear this sequence at every wire iteration; "
                                                            "works only if this is the first push; default: true."),
                                                    CoreInfo::BoolType));

  static SHParametersInfo parameters() { return SHParametersInfo(pushParams); }

  void setParam(int index, const SHVar &value) {
    if (index < variableParamsInfoLen)
      VariableBase::setParam(index, value);
    else if (index == variableParamsInfoLen + 0) {
      _clear = value.payload.boolValue;
    }
  }

  SHVar getParam(int index) {
    if (index < variableParamsInfoLen)
      return VariableBase::getParam(index);
    else if (index == variableParamsInfoLen + 0)
      return shards::Var(_clear);
    throw SHException("Param index out of range.");
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    const auto updateSeqInfo = [this, &data] {
      _seqInfo.basicType = Seq;
      _seqInnerInfo = data.inputType;
      _seqInfo.seqTypes = {&_seqInnerInfo, 1, 0};
      if (_global) {
        _exposedInfo = ExposedInfo(ExposedInfo::GlobalVariable(_name.c_str(), SHCCSTR("The exposed sequence."), _seqInfo, true));
      } else {
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(_name.c_str(), SHCCSTR("The exposed sequence."), _seqInfo, true));
      }
    };

    const auto updateTableInfo = [this, &data] {
      _tableInfo.basicType = Table;
      if (_tableInfo.table.types.elements) {
        shards::arrayFree(_tableInfo.table.types);
      }
      if (_tableInfo.table.keys.elements) {
        shards::arrayFree(_tableInfo.table.keys);
      }
      _seqInfo.basicType = Seq;
      _seqInnerInfo = data.inputType;
      _seqInfo.seqTypes = {&_seqInnerInfo, 1, 0};
      shards::arrayPush(_tableInfo.table.types, _seqInfo);
      if (!_key.isVariable()) {
        SHVar kv = _key;
        shards::arrayPush(_tableInfo.table.keys, kv.payload.stringValue);
      }
      if (_global) {
        _exposedInfo =
            ExposedInfo(ExposedInfo::GlobalVariable(_name.c_str(), SHCCSTR("The exposed table."), SHTypeInfo(_tableInfo), true));
      } else {
        _exposedInfo =
            ExposedInfo(ExposedInfo::Variable(_name.c_str(), SHCCSTR("The exposed table."), SHTypeInfo(_tableInfo), true));
      }
    };

    if (_isTable) {
      for (uint32_t i = 0; data.shared.len > i; i++) {
        if (data.shared.elements[i].name == _name && data.shared.elements[i].exposedType.table.types.elements) {
          auto &tableKeys = data.shared.elements[i].exposedType.table.keys;
          auto &tableTypes = data.shared.elements[i].exposedType.table.types;
          for (uint32_t y = 0; y < tableKeys.len; y++) {
            // if we got key it's not a variable
            SHVar kv = _key;
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

  ALWAYS_INLINE SHVar activate(SHContext *context, const SHVar &input) {
    if (unlikely(_isTable && _key.isVariable())) {
      fillVariableCell();
    }

    if (_clear && _firstPush) {
      shards::arrayResize(_cell->payload.seqValue, 0);
    }
    const auto len = _cell->payload.seqValue.len;
    shards::arrayResize(_cell->payload.seqValue, len + 1);
    cloneVar(_cell->payload.seqValue.elements[len], input);

    return input;
  }
};

struct Sequence : public SeqBase {
  ParamVar _types{shards::Var::Enum(BasicTypes::Any, CoreCC, 'type')};
  Types _seqTypes{};
  std::deque<Types> _innerTypes;

  static inline shards::ParamsInfo pushParams = shards::ParamsInfo(
      variableParamsInfo,
      shards::ParamsInfo::Param("Clear",
                                SHCCSTR("If we should clear this sequence at every wire iteration; "
                                        "works only if this is the first push; default: true."),
                                CoreInfo::BoolType),
      shards::ParamsInfo::Param("Types", SHCCSTR("The sequence inner types to forward declare."), CoreInfo2::BasicTypesTypes));

  static SHParametersInfo parameters() { return SHParametersInfo(pushParams); }

  void setParam(int index, const SHVar &value) {
    if (index < variableParamsInfoLen)
      VariableBase::setParam(index, value);
    else if (index == variableParamsInfoLen + 0) {
      _clear = value.payload.boolValue;
    } else if (index == variableParamsInfoLen + 1) {
      _types = value;
    }
  }

  SHVar getParam(int index) {
    if (index < variableParamsInfoLen)
      return VariableBase::getParam(index);
    else if (index == variableParamsInfoLen + 0)
      return shards::Var(_clear);
    else if (index == variableParamsInfoLen + 1)
      return _types;
    throw SHException("Param index out of range.");
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
    case BasicTypes::Wire:
      inner._types.emplace_back(CoreInfo::WireType);
      break;
    case BasicTypes::Shard:
      inner._types.emplace_back(CoreInfo::ShardRefType);
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
        SHTypeInfo stype{SHType::Seq, {.seqTypes = sinner}};
        inner._types.emplace_back(stype);
      } else {
        const auto type = BasicTypes(v.payload.enumValue);
        // assume enum
        addType(inner, type);
      }
    }
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    const auto updateTableInfo = [this] {
      _tableInfo.basicType = Table;
      if (_tableInfo.table.types.elements) {
        shards::arrayFree(_tableInfo.table.types);
      }
      if (_tableInfo.table.keys.elements) {
        shards::arrayFree(_tableInfo.table.keys);
      }
      SHTypeInfo stype{SHType::Seq, {.seqTypes = _seqTypes}};
      shards::arrayPush(_tableInfo.table.types, stype);
      if (!_key.isVariable()) {
        SHVar kv = _key;
        shards::arrayPush(_tableInfo.table.keys, kv.payload.stringValue);
      }
      if (_global) {
        _exposedInfo =
            ExposedInfo(ExposedInfo::GlobalVariable(_name.c_str(), SHCCSTR("The exposed table."), SHTypeInfo(_tableInfo), true));
      } else {
        _exposedInfo =
            ExposedInfo(ExposedInfo::Variable(_name.c_str(), SHCCSTR("The exposed table."), SHTypeInfo(_tableInfo), true));
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
            SHVar kv = _key;
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
      SHTypeInfo stype{SHType::Seq, {.seqTypes = _seqTypes}};
      if (_global) {
        _exposedInfo = ExposedInfo(ExposedInfo::GlobalVariable(_name.c_str(), SHCCSTR("The exposed sequence."), stype, true));
      } else {
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(_name.c_str(), SHCCSTR("The exposed sequence."), stype, true));
      }
    } else {
      updateTableInfo();
    }

    return data.inputType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (unlikely(_isTable && _key.isVariable())) {
      fillVariableCell();
    }

    if (_clear) {
      auto &seq = *_cell;
      shards::arrayResize(seq.payload.seqValue, 0);
    }

    return input;
  }
};

struct TableDecl : public VariableBase {
  SHTypeInfo _tableInfo{};

  void initTable() {
    if (_isTable) {
      if (_target->valueType != Table) {
        // Not initialized yet
        _target->valueType = Table;
        _target->payload.tableValue.api = &GetGlobals().TableInterface;
        _target->payload.tableValue.opaque = new SHMap();
      }

      if (!_key.isVariable()) {
        auto &kv = _key.get();
        _cell = _target->payload.tableValue.api->tableAt(_target->payload.tableValue, kv.payload.stringValue);

        auto table = _cell;
        if (table->valueType != Table) {
          // Not initialized yet
          table->valueType = Table;
          table->payload.tableValue.api = &GetGlobals().TableInterface;
          table->payload.tableValue.opaque = new SHMap();
        }
      } else {
        return; // we will check during activate
      }
    } else {
      if (_target->valueType != Table) {
        _target->valueType = Table;
        _target->payload.tableValue.api = &GetGlobals().TableInterface;
        _target->payload.tableValue.opaque = new SHMap();
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
      table->payload.tableValue.opaque = new SHMap();
    }
  }

  void warmup(SHContext *context) {
    if (_global)
      _target = referenceGlobalVariable(context, _name.c_str());
    else
      _target = referenceVariable(context, _name.c_str());
    _key.warmup(context);
    initTable();
  }

  SHExposedTypesInfo exposedVariables() { return SHExposedTypesInfo(_exposedInfo); }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  void destroy() {
    if (_tableInfo.table.keys.elements)
      shards::arrayFree(_tableInfo.table.keys);
    if (_tableInfo.table.types.elements)
      shards::arrayFree(_tableInfo.table.types);
  }

  ParamVar _types{shards::Var::Enum(BasicTypes::Any, CoreCC, 'type')};
  Types _seqTypes{};
  std::deque<Types> _innerTypes;

  static inline shards::ParamsInfo pushParams = shards::ParamsInfo(
      variableParamsInfo,
      shards::ParamsInfo::Param("Types", SHCCSTR("The table inner types to forward declare."), CoreInfo2::BasicTypesTypes));

  static SHParametersInfo parameters() { return SHParametersInfo(pushParams); }

  void setParam(int index, const SHVar &value) {
    if (index < variableParamsInfoLen)
      VariableBase::setParam(index, value);
    else if (index == variableParamsInfoLen + 0) {
      _types = value;
    }
  }

  SHVar getParam(int index) {
    if (index < variableParamsInfoLen)
      return VariableBase::getParam(index);
    else if (index == variableParamsInfoLen + 0)
      return _types;
    throw SHException("Param index out of range.");
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
    case BasicTypes::Wire:
      inner._types.emplace_back(CoreInfo::WireType);
      break;
    case BasicTypes::Shard:
      inner._types.emplace_back(CoreInfo::ShardRefType);
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
        SHTypeInfo stype{SHType::Seq, {.seqTypes = sinner}};
        inner._types.emplace_back(stype);
      } else {
        const auto type = BasicTypes(v.payload.enumValue);
        // assume enum
        addType(inner, type);
      }
    }
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    const auto updateTableInfo = [this] {
      _tableInfo.basicType = Table;
      if (_tableInfo.table.types.elements) {
        shards::arrayFree(_tableInfo.table.types);
      }
      if (_tableInfo.table.keys.elements) {
        shards::arrayFree(_tableInfo.table.keys);
      }
      auto stype = SHTypeInfo{SHType::Table, {.table = {.keys = {nullptr, 0, 0}, .types = _seqTypes}}};
      shards::arrayPush(_tableInfo.table.types, stype);
      if (!_key.isVariable()) {
        SHVar kv = _key;
        shards::arrayPush(_tableInfo.table.keys, kv.payload.stringValue);
      }
      if (_global) {
        _exposedInfo =
            ExposedInfo(ExposedInfo::GlobalVariable(_name.c_str(), SHCCSTR("The exposed table."), SHTypeInfo(_tableInfo), true));
      } else {
        _exposedInfo =
            ExposedInfo(ExposedInfo::Variable(_name.c_str(), SHCCSTR("The exposed table."), SHTypeInfo(_tableInfo), true));
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
            SHVar kv = _key;
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
      auto stype = SHTypeInfo{SHType::Table, {.table = {.keys = {nullptr, 0, 0}, .types = _seqTypes}}};
      if (_global) {
        _exposedInfo = ExposedInfo(ExposedInfo::GlobalVariable(_name.c_str(), SHCCSTR("The exposed table."), stype, true));
      } else {
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(_name.c_str(), SHCCSTR("The exposed table."), stype, true));
      }
    } else {
      updateTableInfo();
    }

    return data.inputType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (unlikely(_isTable && _key.isVariable())) {
      fillVariableCell();
    }

    return input;
  }
};

struct SeqUser : VariableBase {
  bool _blittable = false;

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  void initSeq() {
    if (_isTable) {
      if (_target->valueType != Table) {
        // We need to init this in order to fetch cell addr
        // Not initialized yet
        _target->valueType = Table;
        _target->payload.tableValue.api = &GetGlobals().TableInterface;
        _target->payload.tableValue.opaque = new SHMap();
      }

      if (!_key.isVariable()) {
        SHVar kv = _key;
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

  void warmup(SHContext *context) {
    if (_global)
      _target = referenceGlobalVariable(context, _name.c_str());
    else
      _target = referenceVariable(context, _name.c_str());
    _key.warmup(context);
    initSeq();
  }

  SHExposedTypesInfo requiredVariables() {
    if (_name.size() > 0) {
      if (_isTable) {
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(_name.c_str(), SHCCSTR("The required table."), CoreInfo::AnyTableType));
      } else {
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(_name.c_str(), SHCCSTR("The required variable."), CoreInfo::AnyType));
      }
      return SHExposedTypesInfo(_exposedInfo);
    } else {
      return {};
    }
  }
};

struct Count : SeqUser {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::IntType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (unlikely(_isTable && _key.isVariable())) {
      fillVariableCell();
    }

    if (likely(_cell->valueType == Seq)) {
      return shards::Var(int64_t(_cell->payload.seqValue.len));
    } else if (_cell->valueType == Table) {
      return shards::Var(int64_t(_cell->payload.tableValue.api->tableSize(_cell->payload.tableValue)));
    } else if (_cell->valueType == Bytes) {
      return shards::Var(int64_t(_cell->payload.bytesSize));
    } else if (_cell->valueType == String) {
      return shards::Var(int64_t(_cell->payload.stringLen > 0 || _cell->payload.stringValue == nullptr
                                     ? _cell->payload.stringLen
                                     : strlen(_cell->payload.stringValue)));
    } else {
      return shards::Var(0);
    }
  }
};

struct Clear : SeqUser {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (unlikely(_isTable && _key.isVariable())) {
      fillVariableCell();
    }

    if (likely(_cell->valueType == Seq)) {
      // notice this is fine because destroyVar will destroy .cap later
      // so we make sure we are not leaking Vars
      shards::arrayResize(_cell->payload.seqValue, 0);

      // sometimes we might have as input the same _cell!
      // this is kind of a hack but helps UX
      // we in that case output the same _cell with adjusted len!
      if (input.payload.seqValue.elements == _cell->payload.seqValue.elements)
        const_cast<SHVar &>(input).payload.seqValue.len = 0;
    }

    return input;
  }
};

struct Drop : SeqUser {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (unlikely(_isTable && _key.isVariable())) {
      fillVariableCell();
    }

    if (likely(_cell->valueType == Seq)) {
      auto len = _cell->payload.seqValue.len;
      // notice this is fine because destroyVar will destroy .cap later
      // so we make sure we are not leaking Vars
      if (len > 0) {
        shards::arrayResize(_cell->payload.seqValue, len - 1);
      }

      // sometimes we might have as input the same _cell!
      // this is kind of a hack but helps UX
      // we in that case output the same _cell with adjusted len!
      if (input.payload.seqValue.elements == _cell->payload.seqValue.elements)
        const_cast<SHVar &>(input).payload.seqValue.len = len - 1;
    } else {
      throw ActivationError("Variable is not a sequence, failed to Drop.");
    }

    return input;
  }
};

struct DropFront : SeqUser {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (unlikely(_isTable && _key.isVariable())) {
      fillVariableCell();
    }

    if (likely(_cell->valueType == Seq) && _cell->payload.seqValue.len > 0) {
      auto &arr = _cell->payload.seqValue;
      shards::arrayDel(arr, 0);
      // sometimes we might have as input the same _cell!
      // this is kind of a hack but helps UX
      // we in that case output the same _cell with adjusted len!
      if (input.payload.seqValue.elements == arr.elements)
        const_cast<SHVar &>(input).payload.seqValue.len = arr.len;
    } else {
      throw ActivationError("Variable is not a sequence, failed to DropFront.");
    }

    return input;
  }
};

struct Pop : SeqUser {
  // TODO refactor like Push

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }

  SHVar _output{};

  void destroy() { destroyVar(_output); }

  SHTypeInfo compose(const SHInstanceData &data) {
    if (_isTable) {
      for (uint32_t i = 0; data.shared.len > i; i++) {
        if (data.shared.elements[i].name == _name && data.shared.elements[i].exposedType.table.types.elements) {
          auto &tableKeys = data.shared.elements[i].exposedType.table.keys;
          auto &tableTypes = data.shared.elements[i].exposedType.table.types;
          for (uint32_t y = 0; y < tableKeys.len; y++) {
            // if here _key is not variable
            SHVar kv = _key;
            if (strcmp(kv.payload.stringValue, tableKeys.elements[y]) == 0 && tableTypes.elements[y].basicType == Seq) {
              // if we have 1 type we can predict the output
              // with more just make us a any seq, will need ExpectX shards
              // likely
              if (tableTypes.elements[y].seqTypes.len == 1)
                return tableTypes.elements[y].seqTypes.elements[0];
              else
                return CoreInfo::AnySeqType;
            }
          }
        }
      }
      throw SHException("Pop: key not found or key value is not a sequence!.");
    } else {
      for (uint32_t i = 0; i < data.shared.len; i++) {
        auto &cv = data.shared.elements[i];
        if (_name == cv.name && cv.exposedType.basicType == Seq) {
          // if we have 1 type we can predict the output
          // with more just make us a any seq, will need ExpectX shards likely
          if (cv.exposedType.seqTypes.len == 1)
            return cv.exposedType.seqTypes.elements[0];
          else
            return CoreInfo::AnySeqType;
        }
      }
    }
    throw SHException("Variable is not a sequence.");
  }

  SHVar activate(SHContext *context, const SHVar &input) {
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
    auto pops = shards::arrayPop<SHSeq, SHVar>(_cell->payload.seqValue);
    cloneVar(_output, pops);
    return _output;
  }
};

struct PopFront : SeqUser {
  // TODO refactor like push

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }

  SHVar _output{};

  void destroy() { destroyVar(_output); }

  SHTypeInfo compose(const SHInstanceData &data) {
    if (_isTable) {
      for (uint32_t i = 0; data.shared.len > i; i++) {
        if (data.shared.elements[i].name == _name && data.shared.elements[i].exposedType.table.types.elements) {
          auto &tableKeys = data.shared.elements[i].exposedType.table.keys;
          auto &tableTypes = data.shared.elements[i].exposedType.table.types;
          for (uint32_t y = 0; y < tableKeys.len; y++) {
            // if here _key is not variable
            SHVar kv = _key;
            if (strcmp(kv.payload.stringValue, tableKeys.elements[y]) == 0 && tableTypes.elements[y].basicType == Seq) {
              // if we have 1 type we can predict the output
              // with more just make us a any seq, will need ExpectX shards
              // likely
              if (tableTypes.elements[y].seqTypes.len == 1)
                return tableTypes.elements[y].seqTypes.elements[0];
              else
                return CoreInfo::AnySeqType;
            }
          }
        }
      }
      throw SHException("Pop: key not found or key value is not a sequence!.");
    } else {
      for (uint32_t i = 0; i < data.shared.len; i++) {
        auto &cv = data.shared.elements[i];
        if (_name == cv.name && cv.exposedType.basicType == Seq) {
          // if we have 1 type we can predict the output
          // with more just make us a any seq, will need ExpectX shards likely
          if (cv.exposedType.seqTypes.len == 1)
            return cv.exposedType.seqTypes.elements[0];
          else
            return CoreInfo::AnySeqType;
        }
      }
    }
    throw SHException("Variable is not a sequence.");
  }

  SHVar activate(SHContext *context, const SHVar &input) {
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
    static_assert(sizeof(*arr.elements) == sizeof(SHVar), "Wrong seq elements size!");
    // shift backward current elements
    memmove(&arr.elements[0], &arr.elements[1], sizeof(*arr.elements) * len);
    // put first at end
    arr.elements[len] = first;
    // resize, will cut first out too
    shards::arrayResize(arr, len);

    cloneVar(_output, first);
    return _output;
  }
};

struct Take {
  static inline ParamsInfo indicesParamsInfo = ParamsInfo(ParamsInfo::Param(
      "Indices/Keys", SHCCSTR("One or more indices/keys to extract from a sequence/table."), CoreInfo::TakeTypes));
  static SHOptionalString help() {
    return SHCCSTR("Extracts one or more elements/key-values from a sequence or a table by using the provided sequence "
                   "index/indices or table key(s). Operation is non-destructive; doesn't modify target sequence/table.");
  }

  SHSeq _cachedSeq{};
  SHVar _output{};
  SHVar _indices{};
  SHVar *_indicesVar{nullptr};
  ExposedInfo _exposedInfo{};
  bool _seqOutput{false};
  bool _tableOutput{false};

  SHVar _vectorOutput{};
  const VectorTypeTraits *_vectorInputType{nullptr};
  const VectorTypeTraits *_vectorOutputType{nullptr};
  const NumberConversion *_vectorConversion{nullptr};

  Type _seqOutputType{};
  std::vector<SHTypeInfo> _seqOutputTypes;

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
      shards::arrayFree(_cachedSeq);
    }
  }

  static SHTypesInfo inputTypes() { return CoreInfo::Indexables; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("The sequence or table from which elements/key-values have to be extracted.");
  }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The extracted elements/key-values."); }

  static SHParametersInfo parameters() { return SHParametersInfo(indicesParamsInfo); }

  SHTypeInfo compose(const SHInstanceData &data) {
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
            throw SHException(msg);
          }
        }
      }
    }

    _tableOutput = isTable;

    if (!valid)
      throw SHException("Take, invalid indices or malformed input.");

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
          SHLOG_ERROR(errorMsg.c_str());
          throw ComposeError(errorMsg);
        }

        _vectorConversion =
            NumberTypeLookup::getInstance().getConversion(_vectorInputType->numberType, _vectorOutputType->numberType);
        if (!_vectorConversion) {
          std::stringstream errorMsgStream;
          errorMsgStream << "Take: No conversion from " << _vectorInputType->name << " to " << _vectorOutputType->name
                         << " exists";
          std::string errorMsg = errorMsgStream.str();
          SHLOG_ERROR(errorMsg.c_str());
          throw ComposeError(errorMsg);
        }

        _vectorOutput.valueType = _vectorOutputType->shType;
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
            SHLOG_ERROR("Table input type: {}", data.inputType);
            throw ComposeError("Take: Expected same number of types for numer of "
                               "keys in table input.");
          }

          if (_seqOutput) {
            _seqOutputTypes.clear();
            for (uint32_t j = 0; j < _indices.payload.seqValue.len; j++) {
              auto &record = _indices.payload.seqValue.elements[j];
              if (record.valueType != String) {
                SHLOG_ERROR("Expected a sequence of strings, but found: {}", _indices);
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
              SHLOG_ERROR("Table input type: {} missing keys: {}", data.inputType, _indices);
              throw ComposeError("Take: Failed to find a matching keys in the "
                                 "input type table");
            }
            _seqOutputType = Type::SeqOf(SHTypesInfo{_seqOutputTypes.data(), uint32_t(_seqOutputTypes.size()), 0});
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

    throw SHException("Take, invalid input type or not implemented.");
  }

  SHExposedTypesInfo requiredVariables() {
    if (_indices.valueType == ContextVar) {
      if (_seqOutput)
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(_indices.payload.stringValue, SHCCSTR("The required variables."),
                                                         _tableOutput ? CoreInfo::StringSeqType : CoreInfo::IntSeqType));
      else
        _exposedInfo = ExposedInfo(ExposedInfo::Variable(_indices.payload.stringValue, SHCCSTR("The required variable."),
                                                         _tableOutput ? CoreInfo::StringType : CoreInfo::IntType));
      return SHExposedTypesInfo(_exposedInfo);
    } else {
      return {};
    }
  }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      cloneVar(_indices, value);
      cleanup();
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _indices;
    default:
      break;
    }
    return shards::Var::Empty;
  }

  struct OutOfRangeEx : public ActivationError {
    OutOfRangeEx(int64_t len, int64_t index) : ActivationError("Take out of range!") {
      SHLOG_ERROR("Out of range! len: {} wanted index: {}", len, index);
    }
  };

  void warmup(SHContext *context) {
    if (_indices.valueType == ContextVar && !_indicesVar) {
      _indicesVar = referenceVariable(context, _indices.payload.stringValue);
    }
  }

#define ACTIVATE_INDEXABLE(__name__, __len__, __val__)                            \
  ALWAYS_INLINE SHVar __name__(SHContext *context, const SHVar &input) {          \
    const auto inputLen = size_t(__len__);                                        \
    const auto &indices = _indicesVar ? *_indicesVar : _indices;                  \
    if (likely(!_seqOutput)) {                                                    \
      const auto index = indices.payload.intValue;                                \
      if (index < 0 || size_t(index) >= inputLen) {                               \
        throw OutOfRangeEx(inputLen, index);                                      \
      }                                                                           \
      return __val__;                                                             \
    } else {                                                                      \
      const uint32_t nindices = indices.payload.seqValue.len;                     \
      shards::arrayResize(_cachedSeq, nindices);                                  \
      for (uint32_t i = 0; nindices > i; i++) {                                   \
        const auto index = indices.payload.seqValue.elements[i].payload.intValue; \
        if (index < 0 || size_t(index) >= inputLen) {                             \
          throw OutOfRangeEx(inputLen, index);                                    \
        }                                                                         \
        _cachedSeq.elements[i] = __val__;                                         \
      }                                                                           \
      return shards::Var(_cachedSeq);                                             \
    }                                                                             \
  }

  ACTIVATE_INDEXABLE(activateSeq, input.payload.seqValue.len, input.payload.seqValue.elements[index])
  ACTIVATE_INDEXABLE(activateString, SHSTRLEN(input), shards::Var(input.payload.stringValue[index]))
  ACTIVATE_INDEXABLE(activateBytes, input.payload.bytesSize, shards::Var(input.payload.bytesValue[index]))

  SHVar activateTable(SHContext *context, const SHVar &input) {
    // TODO, if the strings are static at compose time, make sure to cache the
    // return value
    const auto &indices = _indicesVar ? *_indicesVar : _indices;
    if (!_seqOutput) {
      const auto key = indices.payload.stringValue;
      const auto val = input.payload.tableValue.api->tableAt(input.payload.tableValue, key);
      return *val;
    } else {
      const uint32_t nkeys = indices.payload.seqValue.len;
      shards::arrayResize(_cachedSeq, nkeys);
      for (uint32_t i = 0; nkeys > i; i++) {
        const auto key = indices.payload.seqValue.elements[i].payload.stringValue;
        const auto val = input.payload.tableValue.api->tableAt(input.payload.tableValue, key);
        _cachedSeq.elements[i] = *val;
      }
      return Var(_cachedSeq);
    }
  }

  SHVar activateVector(SHContext *context, const SHVar &input) {
    const auto &indices = _indices;

    try {
      _vectorConversion->convertMultipleSeq(&input.payload, &_vectorOutput.payload, _vectorInputType->dimension,
                                            indices.payload.seqValue);
    } catch (const NumberConversionOutOfRangeEx &ex) {
      throw OutOfRangeEx(_vectorInputType->dimension, ex.index);
    }

    return _vectorOutput;
  }

  SHVar activateNumber(SHContext *context, const SHVar &input) {
    SHInt index = _indices.payload.intValue;
    if (index < 0 || index >= (SHInt)_vectorInputType->dimension) {
      throw OutOfRangeEx(_vectorInputType->dimension, index);
    }

    const uint8_t *inputPtr = (uint8_t *)&input.payload + _vectorConversion->inStride * index;
    _vectorConversion->convertOne(inputPtr, &_vectorOutput.payload);

    return _vectorOutput;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    // Take branches during validation into different inlined shards
    // If we hit this, maybe that type of input is not yet implemented
    throw ActivationError("Take path not implemented for this type.");
  }
};

struct RTake : public Take {
  // works only for seqs tho
  // TODO need to add string and bytes
  static inline ParamsInfo indicesParamsInfo = ParamsInfo(ParamsInfo::Param(
      "Indices", SHCCSTR("One or more indices (counted backwards from the last element) to extract from a sequence."),
      CoreInfo::RTakeTypes));
  static SHOptionalString help() {
    return SHCCSTR("Works exactly like `Take` except that the selection indices are counted backwards from the last element in "
                   "the target sequence. Also, `RTake` works only on sequences, not on tables.");
  }

  static SHParametersInfo parameters() { return SHParametersInfo(indicesParamsInfo); }

  static SHTypesInfo inputTypes() { return CoreInfo::RIndexables; }
  static SHOptionalString inputHelp() { return SHCCSTR("The sequence from which elements have to be extracted."); }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The extracted elements."); }

  SHTypeInfo compose(const SHInstanceData &data) {
    SHTypeInfo result = Take::compose(data);
    if (data.inputType.basicType == Seq) {
      OVERRIDE_ACTIVATE(data, activate);
    } else {
      throw SHException("RTake is only supported on sequence types");
    }
    return result;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
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
      shards::arrayResize(_cachedSeq, nindices);
      for (uint32_t i = 0; nindices > i; i++) {
        const auto index = indices.payload.seqValue.elements[i].payload.intValue;
        if (index >= inputLen || index < 0) {
          throw OutOfRangeEx(inputLen, index);
        }
        _cachedSeq.elements[i] = input.payload.seqValue.elements[inputLen - 1 - index];
      }
      return shards::Var(_cachedSeq);
    }
  }
};

struct Slice {
  static inline ParamsInfo indicesParamsInfo = ParamsInfo(
      ParamsInfo::Param("From",
                        SHCCSTR("The position/index of the first character or element that is to be extracted (including). "
                                "Negative position/indices simply loop over the target string/sequence counting backwards."),
                        CoreInfo::IntsVar),
      ParamsInfo::Param("To",
                        SHCCSTR("The position/index of the last character or element that is to be extracted (excluding). "
                                "Negative position/indices simply loop over the target string/sequence counting backwards."),
                        CoreInfo::IntsVarOrNone),
      ParamsInfo::Param("Step",
                        SHCCSTR("The increment between each position/index. Chooses every nth sample to extract, where n is the "
                                "increment. Value has to be greater than zero."),
                        CoreInfo::IntType));
  static SHOptionalString help() {
    return SHCCSTR("Extracts characters from a string or elements from a sequence based on the start and end positions/indices "
                   "and an increment parameter. Operation is non-destructive; the target string/sequence is not modified.");
  }

  SHSeq _cachedSeq{};
  std::vector<uint8_t> _cachedBytes{};
  SHVar _from{shards::Var(0)};
  SHVar *_fromVar = nullptr;
  SHVar _to{};
  SHVar *_toVar = nullptr;
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
      shards::arrayFree(_cachedSeq);
    }
  }

  static inline Types InputTypes{{CoreInfo::AnySeqType, CoreInfo::BytesType, CoreInfo::StringType}};

  static SHTypesInfo inputTypes() { return InputTypes; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("The string or sequence from which characters/elements have to be extracted.");
  }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The extracted characters/elements."); }

  static SHParametersInfo parameters() { return SHParametersInfo(indicesParamsInfo); }

  SHTypeInfo compose(const SHInstanceData &data) {
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
      throw SHException("Slice, invalid From variable.");

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
      throw SHException("Slice, invalid To variable.");

    if (data.inputType.basicType == Seq) {
      OVERRIDE_ACTIVATE(data, activateSeq);
    } else if (data.inputType.basicType == Bytes) {
      OVERRIDE_ACTIVATE(data, activateBytes);
    } else if (data.inputType.basicType == String) {
      OVERRIDE_ACTIVATE(data, activateString);
    }

    return data.inputType;
  }

  SHExposedTypesInfo requiredVariables() {
    if (_from.valueType == ContextVar && _to.valueType == ContextVar) {
      _exposedInfo =
          ExposedInfo(ExposedInfo::Variable(_from.payload.stringValue, SHCCSTR("The required variable."), CoreInfo::IntType),
                      ExposedInfo::Variable(_to.payload.stringValue, SHCCSTR("The required variable."), CoreInfo::IntType));
      return SHExposedTypesInfo(_exposedInfo);
    } else if (_from.valueType == ContextVar) {
      _exposedInfo =
          ExposedInfo(ExposedInfo::Variable(_from.payload.stringValue, SHCCSTR("The required variable."), CoreInfo::IntType));
      return SHExposedTypesInfo(_exposedInfo);
    } else if (_to.valueType == ContextVar) {
      _exposedInfo =
          ExposedInfo(ExposedInfo::Variable(_to.payload.stringValue, SHCCSTR("The required variable."), CoreInfo::IntType));
      return SHExposedTypesInfo(_exposedInfo);
    } else {
      return {};
    }
  }

  void setParam(int index, const SHVar &value) {
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

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _from;
    case 1:
      return _to;
    case 2:
      return shards::Var(_step);
    default:
      break;
    }
    return shards::Var::Empty;
  }

  struct OutOfRangeEx : public ActivationError {
    OutOfRangeEx(int64_t len, int64_t from, int64_t to) : ActivationError("Slice out of range!") {
      SHLOG_ERROR("Out of range! from: {} to: {} len: {}", from, to, len);
    }
  };

  SHVar activateBytes(SHContext *context, const SHVar &input) {
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
      SHVar output{};
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
      return shards::Var(&_cachedBytes.front(), uint32_t(actualLen));
    } else {
      throw ActivationError("Slice's Step must be greater then 0");
    }
  }

  SHVar activateString(SHContext *context, const SHVar &input) {
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
      return shards::Var((const char *)_cachedBytes.data(), uint32_t(actualLen));
    } else {
      throw ActivationError("Slice's Step must be greater then 0");
    }
  }

  SHVar activateSeq(SHContext *context, const SHVar &input) {
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
      SHVar output{};
      output.valueType = Seq;
      output.payload.seqValue.elements = &input.payload.seqValue.elements[from];
      output.payload.seqValue.len = uint32_t(len);
      return output;
    } else if (_step > 1) {
      const auto actualLen = len / _step + (len % _step != 0);
      shards::arrayResize(_cachedSeq, uint32_t(actualLen));
      auto idx = 0;
      for (auto i = from; i < to; i += _step) {
        cloneVar(_cachedSeq.elements[idx], input.payload.seqValue.elements[i]);
        idx++;
      }
      return shards::Var(_cachedSeq);
    } else {
      throw ActivationError("Slice's Step must be greater then 0");
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) { throw ActivationError("Slice: unreachable code path"); }
};

struct Limit {
  static inline shards::ParamsInfo indicesParamsInfo = shards::ParamsInfo(
      shards::ParamsInfo::Param("Max", SHCCSTR("How many maximum elements to take from the input sequence."), CoreInfo::IntType));

  SHSeq _cachedResult{};
  int64_t _max = 0;

  void destroy() {
    if (_cachedResult.elements) {
      shards::arrayFree(_cachedResult);
    }
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnySeqType; }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static SHParametersInfo parameters() { return SHParametersInfo(indicesParamsInfo); }

  SHTypeInfo compose(const SHInstanceData &data) {
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
    throw SHException("Limit expected a sequence as input.");
  }

  void setParam(int index, const SHVar &value) { _max = value.payload.intValue; }

  SHVar getParam(int index) { return shards::Var(_max); }

  SHVar activate(SHContext *context, const SHVar &input) {
    int64_t inputLen = input.payload.seqValue.len;

    if (_max == 1) {
      auto index = 0;
      if (index >= inputLen) {
        SHLOG_ERROR("Limit out of range! len: {} wanted index: {}", inputLen, index);
        throw ActivationError("Limit out of range!");
      }
      return input.payload.seqValue.elements[index];
    } else {
      // Else it's a seq
      auto nindices = (uint64_t)std::max((int64_t)0, std::min(inputLen, _max));
      shards::arrayResize(_cachedResult, uint32_t(nindices));
      for (uint64_t i = 0; i < nindices; i++) {
        _cachedResult.elements[i] = input.payload.seqValue.elements[i];
      }
      return shards::Var(_cachedResult);
    }
  }
};

struct ForRangeShard {
  static SHOptionalString help() { return SHCCSTR("Executes a shard while an iteration value is within a range."); }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The input value is not used and will pass through."); }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The output of this shard will be its input."); }

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _from = value;
      break;
    case 1:
      _to = value;
      break;
    case 2:
      _shards = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _from;
    case 1:
      return _to;
    case 2:
      return _shards;
    default:
      throw InvalidParameterIndex();
    }
  }

  void cleanup() {
    _from.cleanup();
    _to.cleanup();
    _shards.cleanup();
  }

  void warmup(SHContext *context) {
    _from.warmup(context);
    _to.warmup(context);
    _shards.warmup(context);
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    auto dataCopy = data;
    dataCopy.inputType = CoreInfo::IntType;

    _shards.compose(dataCopy);

    return data.inputType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto from = _from.get().payload.intValue;
    auto to = _to.get().payload.intValue;

    SHVar output{};
    SHVar item{};
    if (from < to) {
      for (auto i = from; i <= to; i++) {
        item = Var(i);
        auto state = _shards.activate<true>(context, item, output);
        if (state != SHWireState::Continue)
          break;
      }
    } else if (from > to) {
      for (auto i = from; i >= to; i--) {
        item = Var(i);
        auto state = _shards.activate<true>(context, item, output);
        if (state != SHWireState::Continue)
          break;
      }
    }

    return input;
  }

private:
  static inline Parameters _params = {
      {"From", SHCCSTR("The initial iteration value (inclusive)."), {CoreInfo::IntType, CoreInfo::IntVarType}},
      {"To", SHCCSTR("The final iteration value (inclusive)."), {CoreInfo::IntType, CoreInfo::IntVarType}},
      {"Action",
       SHCCSTR("The action to perform at each iteration. The current iteration "
               "value will be passed as input."),
       {CoreInfo::ShardsOrNone}}};

  ParamVar _from{Var(0)};
  ParamVar _to{Var(1)};
  ShardsVar _shards{};
};

struct Repeat {
  ShardsVar _blks{};
  ShardsVar _pred{};
  std::string _ctxVar;
  SHVar *_ctxTimes = nullptr;
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

  void warmup(SHContext *ctx) {
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

  static inline Parameters _params{{"Action", SHCCSTR("The shards to repeat."), CoreInfo::Shards},
                                   {"Times", SHCCSTR("How many times we should repeat the action."), CoreInfo::IntsVar},
                                   {"Forever", SHCCSTR("If we should repeat the action forever."), {CoreInfo::BoolType}},
                                   {"Until",
                                    SHCCSTR("Optional shards to use as predicate to continue repeating "
                                            "until it's true"),
                                    CoreInfo::ShardsOrNone}};

  static SHOptionalString help() {
    return SHCCSTR("Repeat an action a given number of times or until a condition is no longer `true`.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("The input will be passed to both the action and the `:Until` condition if used.");
  }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The output of this shard will be its input."); }

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
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

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _blks;
    case 1:
      if (_ctxVar.size() == 0) {
        return shards::Var(_times);
      } else {
        auto ctxTimes = shards::Var(_ctxVar);
        ctxTimes.valueType = ContextVar;
        return ctxTimes;
      }
    case 2:
      return shards::Var(_forever);
    case 3:
      return _pred;
    default:
      break;
    }
    throw SHException("Parameter out of range.");
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    _blks.compose(data);
    const auto predres = _pred.compose(data);
    if (_pred && predres.outputType.basicType != Bool) {
      throw ComposeError("Repeat shard Until predicate should output a boolean!");
    }
    return data.inputType;
  }

  SHExposedTypesInfo requiredVariables() {
    if (_ctxVar.size() == 0) {
      return {};
    } else {
      _requiredInfo =
          ExposedInfo(ExposedInfo::Variable(_ctxVar.c_str(), SHCCSTR("The Int number of repeats variable."), CoreInfo::IntType));
      return SHExposedTypesInfo(_requiredInfo);
    }
  }

  FLATTEN ALWAYS_INLINE SHVar activate(SHContext *context, const SHVar &input) {
    auto repeats = _forever ? 1 : *_repeats;
    while (repeats) {
      SHVar repeatOutput{};
      SHVar blks = _blks;
      auto state = activateShards2(blks.payload.seqValue, context, input, repeatOutput);
      if (state != SHWireState::Continue)
        break;

      if (!_forever)
        repeats--;

      if (_pred) {
        SHVar pres{};
        state = _pred.activate<true>(context, input, pres);
        // logic flow here!
        if (unlikely(state > SHWireState::Return || pres.payload.boolValue))
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

  SHTime current;
  SHTimeDiff next;
  SHDuration dsleep;

  ShardsVar _blks;
  ExposedInfo _requiredInfo{};
  SHComposeResult _validation{};
  bool _done{false};
  bool _repeat{false};
  double _repeatTime{0.0};
  Shard *self{nullptr};

  void cleanup() {
    _blks.cleanup();
    _done = false;
    if (self)
      self->inlineShardId = NotInline;
  }

  void warmup(SHContext *ctx) {
    _blks.warmup(ctx);
    current = SHClock::now();
    dsleep = SHDuration(_repeatTime);
    next = current + SHDuration(0.0);
  }

  static inline Parameters params{{"Action", SHCCSTR("The shard or sequence of shards to execute."), {CoreInfo::Shards}},
                                  {"Every",
                                   SHCCSTR("The number of seconds to wait until repeating the action, if 0 "
                                           "the action will happen only once per wire flow execution."),
                                   {CoreInfo::FloatType}}};

  static SHOptionalString help() {
    return SHCCSTR("Executes the shard or sequence of shards with the desired frequency in a wire flow execution.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) {
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

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _blks;
    case 1:
      return Var(_repeatTime);
    default:
      break;
    }
    throw SHException("Parameter out of range.");
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    self = data.shard;
    _validation = _blks.compose(data);
    return data.inputType;
  }

  SHExposedTypesInfo exposedVariables() { return _validation.exposedInfo; }

  ALWAYS_INLINE SHVar activate(SHContext *context, const SHVar &input) {
    if (_repeat) {
      // monitor and reset timer if expired
      current = SHClock::now();
      if (current >= next) {
        _done = false;
      }
    }

    if (unlikely(!_done)) {
      _done = true;
      SHVar output{};
      _blks.activate(context, input, output);
      if (!_repeat) {
        // let's cheat in this case and stop triggering this call
        self->inlineShardId = NoopShard;
      } else {
        next = next + dsleep;
      }
    }

    return input;
  }
};

RUNTIME_CORE_SHARD_TYPE(Const);
RUNTIME_CORE_SHARD_TYPE(And);
RUNTIME_CORE_SHARD_TYPE(Or);
RUNTIME_CORE_SHARD_TYPE(Not);
RUNTIME_CORE_SHARD_TYPE(Fail);
RUNTIME_CORE_SHARD_TYPE(Restart);
RUNTIME_CORE_SHARD_TYPE(Return);
RUNTIME_CORE_SHARD_TYPE(IsValidNumber);
RUNTIME_CORE_SHARD_TYPE(Set);
RUNTIME_CORE_SHARD_TYPE(Ref);
RUNTIME_CORE_SHARD_TYPE(Update);
RUNTIME_CORE_SHARD_TYPE(Get);
RUNTIME_CORE_SHARD_TYPE(Swap);
RUNTIME_CORE_SHARD_TYPE(Take);
RUNTIME_CORE_SHARD_TYPE(RTake);
RUNTIME_CORE_SHARD_TYPE(Slice);
RUNTIME_CORE_SHARD_TYPE(Limit);
RUNTIME_CORE_SHARD_TYPE(Push);
RUNTIME_CORE_SHARD_TYPE(Sequence);
RUNTIME_CORE_SHARD_TYPE(Pop);
RUNTIME_CORE_SHARD_TYPE(PopFront);
RUNTIME_CORE_SHARD_TYPE(Clear);
RUNTIME_CORE_SHARD_TYPE(Drop);
RUNTIME_CORE_SHARD_TYPE(DropFront);
RUNTIME_CORE_SHARD_TYPE(Count);
RUNTIME_CORE_SHARD_TYPE(Repeat);
}; // namespace shards

#endif // SH_CORE_SHARDS_CORE
