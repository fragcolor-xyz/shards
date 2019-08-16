#pragma once

#include "core.hpp"

namespace chainblocks {
namespace Math {
struct BinaryBase {
  enum OpType { Normal, Seq1, SeqSeq };

  static inline TypesInfo mathBaseTypesInfo = TypesInfo::FromMany(
      CBType::Int, CBType::Int2, CBType::Int3, CBType::Int4, CBType::Int8,
      CBType::Int16, CBType::Float, CBType::Float2, CBType::Float3,
      CBType::Float4, CBType::Color);
  static inline TypesInfo mathTypesInfo =
      TypesInfo(CBTypesInfo(mathBaseTypesInfo), true);
  static inline TypesInfo mathBaseTypesInfo1 = TypesInfo::FromMany(
      CBType::Int, CBType::Int2, CBType::Int3, CBType::Int4, CBType::Int8,
      CBType::Int16, CBType::Float, CBType::Float2, CBType::Float3,
      CBType::Float4, CBType::Color, CBType::ContextVar);
  static inline TypesInfo mathTypesInfo1 =
      TypesInfo(CBTypesInfo(mathBaseTypesInfo1), true);
  static inline ParamsInfo mathParamsInfo = ParamsInfo(ParamsInfo::Param(
      "Operand", "The operand.", CBTypesInfo(mathTypesInfo1)));

  CBVar _operand{};
  CBVar *_ctxOperand{};
  CBVar _cachedSeq{};
  ExposedInfo _consumedInfo{};
  OpType _opType = Normal;

  void cleanup() { _ctxOperand = nullptr; }

  void destroy() {
    if (_cachedSeq.valueType == Seq) {
      stbds_arrfree(_cachedSeq.payload.seqValue);
    }
    destroyVar(_operand);
  }

  void setup() {
    _cachedSeq.valueType = Seq;
    _cachedSeq.payload.seqValue = nullptr;
    _cachedSeq.payload.seqLen = -1;
  }

  static CBTypesInfo inputTypes() { return CBTypesInfo(mathTypesInfo); }

  static CBTypesInfo outputTypes() { return CBTypesInfo(mathTypesInfo); }

  static CBParametersInfo parameters() {
    return CBParametersInfo(mathParamsInfo);
  }

  CBTypeInfo inferTypes(CBTypeInfo inputType,
                        CBExposedTypesInfo consumableVariables) {
    if (_operand.valueType == ContextVar) {
      for (auto i = 0; i < stbds_arrlen(consumableVariables); i++) {
        if (consumableVariables[i].name == _operand.payload.stringValue) {
          if (consumableVariables[i].exposedType.basicType != Seq &&
              inputType.basicType != Seq) {
            _opType = Normal;
          } else if (consumableVariables[i].exposedType.basicType != Seq &&
                     inputType.basicType == Seq) {
            _opType = Seq1;
          } else if (consumableVariables[i].exposedType.basicType == Seq &&
                     inputType.basicType == Seq) {
            _opType = SeqSeq;
          } else {
            throw CBException(
                "Math broadcasting not supported between given types!");
          }
        }
      }
    } else {
      if (_operand.valueType != Seq && inputType.basicType != Seq) {
        _opType = Normal;
      } else if (_operand.valueType != Seq && inputType.basicType == Seq) {
        _opType = Seq1;
      } else if (_operand.valueType == Seq && inputType.basicType == Seq) {
        _opType = SeqSeq;
      } else {
        throw CBException(
            "Math broadcasting not supported between given types!");
      }
    }
    return inputType;
  }

  CBExposedTypesInfo consumedVariables() {
    if (_operand.valueType == ContextVar) {
      _consumedInfo = ExposedInfo(ExposedInfo::Variable(
          _operand.payload.stringValue, "The consumed operand.",
          CBTypeInfo(mathTypesInfo)));
      return CBExposedTypesInfo(_consumedInfo);
    }
    return nullptr;
  }

  void setParam(int index, CBVar value) {
    cloneVar(_operand, value);
    cleanup();
  }

  CBVar getParam(int index) { return _operand; }
};

#define MATH_OPERATION(NAME, OPERATOR, OPERATOR_STR)                           \
  struct NAME : public BinaryBase {                                            \
    void operate(CBVar &output, const CBVar &input, const CBVar &operand) {    \
      if (input.valueType != operand.valueType)                                \
        throw CBException("Operation not supported between different "         \
                          "types.");                                           \
      switch (input.valueType) {                                               \
      case Int:                                                                \
        output.valueType = Int;                                                \
        output.payload.intValue =                                              \
            input.payload.intValue OPERATOR operand.payload.intValue;          \
        break;                                                                 \
      case Int2:                                                               \
        output.valueType = Int2;                                               \
        output.payload.int2Value =                                             \
            input.payload.int2Value OPERATOR operand.payload.int2Value;        \
        break;                                                                 \
      case Int3:                                                               \
        output.valueType = Int3;                                               \
        output.payload.int3Value =                                             \
            input.payload.int3Value OPERATOR operand.payload.int3Value;        \
        break;                                                                 \
      case Int4:                                                               \
        output.valueType = Int4;                                               \
        output.payload.int4Value =                                             \
            input.payload.int4Value OPERATOR operand.payload.int4Value;        \
        break;                                                                 \
      case Int8:                                                               \
        output.valueType = Int8;                                               \
        output.payload.int8Value =                                             \
            input.payload.int8Value OPERATOR operand.payload.int8Value;        \
        break;                                                                 \
      case Int16:                                                              \
        output.valueType = Int16;                                              \
        output.payload.int16Value =                                            \
            input.payload.int16Value OPERATOR operand.payload.int16Value;      \
        break;                                                                 \
      case Float:                                                              \
        output.valueType = Float;                                              \
        output.payload.floatValue =                                            \
            input.payload.floatValue OPERATOR operand.payload.floatValue;      \
        break;                                                                 \
      case Float2:                                                             \
        output.valueType = Float2;                                             \
        output.payload.float2Value =                                           \
            input.payload.float2Value OPERATOR operand.payload.float2Value;    \
        break;                                                                 \
      case Float3:                                                             \
        output.valueType = Float3;                                             \
        output.payload.float3Value =                                           \
            input.payload.float3Value OPERATOR operand.payload.float3Value;    \
        break;                                                                 \
      case Float4:                                                             \
        output.valueType = Float4;                                             \
        output.payload.float4Value =                                           \
            input.payload.float4Value OPERATOR operand.payload.float4Value;    \
        break;                                                                 \
      case Color:                                                              \
        output.valueType = Color;                                              \
        output.payload.colorValue.r =                                          \
            input.payload.colorValue.r OPERATOR operand.payload.colorValue.r;  \
        output.payload.colorValue.g =                                          \
            input.payload.colorValue.g OPERATOR operand.payload.colorValue.g;  \
        output.payload.colorValue.b =                                          \
            input.payload.colorValue.b OPERATOR operand.payload.colorValue.b;  \
        output.payload.colorValue.a =                                          \
            input.payload.colorValue.a OPERATOR operand.payload.colorValue.a;  \
        break;                                                                 \
      default:                                                                 \
        throw CBException(OPERATOR_STR                                         \
                          " operation not supported between given types!");    \
      }                                                                        \
    }                                                                          \
                                                                               \
    CBVar activate(CBContext *context, const CBVar &input) {                   \
      if (_operand.valueType == ContextVar && _ctxOperand == nullptr) {        \
        _ctxOperand = contextVariable(context, _operand.payload.stringValue);  \
      }                                                                        \
      auto &operand = _ctxOperand ? *_ctxOperand : _operand;                   \
      CBVar output{};                                                          \
      if (_opType == Normal) {                                                 \
        operate(output, input, operand);                                       \
        return output;                                                         \
      } else if (_opType == Seq1) {                                            \
        stbds_arrsetlen(_cachedSeq.payload.seqValue, 0);                       \
        for (auto i = 0; i < stbds_arrlen(input.payload.seqValue); i++) {      \
          operate(output, input.payload.seqValue[i], operand);                 \
          stbds_arrpush(_cachedSeq.payload.seqValue, output);                  \
        }                                                                      \
        return _cachedSeq;                                                     \
      } else {                                                                 \
        stbds_arrsetlen(_cachedSeq.payload.seqValue, 0);                       \
        for (auto i = 0; i < stbds_arrlen(input.payload.seqValue),             \
                  i < stbds_arrlen(operand.payload.seqValue);                  \
             i++) {                                                            \
          operate(output, input.payload.seqValue[i],                           \
                  operand.payload.seqValue[i]);                                \
          stbds_arrpush(_cachedSeq.payload.seqValue, output);                  \
        }                                                                      \
        return _cachedSeq;                                                     \
      }                                                                        \
    }                                                                          \
  };                                                                           \
  RUNTIME_BLOCK_TYPE(Math, NAME);

#define MATH_INT_OPERATION(NAME, OPERATOR, OPERATOR_STR)                       \
  struct NAME : public BinaryBase {                                            \
    void operate(CBVar &output, const CBVar &input, const CBVar &operand) {    \
      if (input.valueType != operand.valueType)                                \
        throw CBException("Operation not supported between different "         \
                          "types.");                                           \
      switch (input.valueType) {                                               \
      case Int:                                                                \
        output.valueType = Int;                                                \
        output.payload.intValue =                                              \
            input.payload.intValue OPERATOR operand.payload.intValue;          \
        break;                                                                 \
      case Int2:                                                               \
        output.valueType = Int2;                                               \
        output.payload.int2Value =                                             \
            input.payload.int2Value OPERATOR operand.payload.int2Value;        \
        break;                                                                 \
      case Int3:                                                               \
        output.valueType = Int3;                                               \
        output.payload.int3Value =                                             \
            input.payload.int3Value OPERATOR operand.payload.int3Value;        \
        break;                                                                 \
      case Int4:                                                               \
        output.valueType = Int4;                                               \
        output.payload.int4Value =                                             \
            input.payload.int4Value OPERATOR operand.payload.int4Value;        \
        break;                                                                 \
      case Int8:                                                               \
        output.valueType = Int8;                                               \
        output.payload.int8Value =                                             \
            input.payload.int8Value OPERATOR operand.payload.int8Value;        \
        break;                                                                 \
      case Int16:                                                              \
        output.valueType = Int16;                                              \
        output.payload.int16Value =                                            \
            input.payload.int16Value OPERATOR operand.payload.int16Value;      \
        break;                                                                 \
      case Color:                                                              \
        output.valueType = Color;                                              \
        output.payload.colorValue.r =                                          \
            input.payload.colorValue.r OPERATOR operand.payload.colorValue.r;  \
        output.payload.colorValue.g =                                          \
            input.payload.colorValue.g OPERATOR operand.payload.colorValue.g;  \
        output.payload.colorValue.b =                                          \
            input.payload.colorValue.b OPERATOR operand.payload.colorValue.b;  \
        output.payload.colorValue.a =                                          \
            input.payload.colorValue.a OPERATOR operand.payload.colorValue.a;  \
        break;                                                                 \
      default:                                                                 \
        throw CBException(OPERATOR_STR                                         \
                          " operation not supported between given types!");    \
      }                                                                        \
    }                                                                          \
                                                                               \
    CBVar activate(CBContext *context, const CBVar &input) {                   \
      if (_operand.valueType == ContextVar && _ctxOperand == nullptr) {        \
        _ctxOperand = contextVariable(context, _operand.payload.stringValue);  \
      }                                                                        \
      auto &operand = _ctxOperand ? *_ctxOperand : _operand;                   \
      CBVar output{};                                                          \
      if (_opType == Normal) {                                                 \
        operate(output, input, operand);                                       \
        return output;                                                         \
      } else if (_opType == Seq1) {                                            \
        stbds_arrsetlen(_cachedSeq.payload.seqValue, 0);                       \
        for (auto i = 0; i < stbds_arrlen(input.payload.seqValue); i++) {      \
          operate(output, input, operand);                                     \
          stbds_arrpush(_cachedSeq.payload.seqValue, output);                  \
        }                                                                      \
        return _cachedSeq;                                                     \
      } else {                                                                 \
        stbds_arrsetlen(_cachedSeq.payload.seqValue, 0);                       \
        for (auto i = 0; i < stbds_arrlen(input.payload.seqValue),             \
                  i < stbds_arrlen(operand.payload.seqValue);                  \
             i++) {                                                            \
          operate(output, input, operand.payload.seqValue[i]);                 \
          stbds_arrpush(_cachedSeq.payload.seqValue, output);                  \
        }                                                                      \
        return _cachedSeq;                                                     \
      }                                                                        \
    }                                                                          \
  };                                                                           \
  RUNTIME_BLOCK_TYPE(Math, NAME);

MATH_OPERATION(Add, +, "Add");
MATH_OPERATION(Subtract, -, "Subtract");
MATH_OPERATION(Multiply, *, "Multiply");
MATH_OPERATION(Divide, /, "Divide");
MATH_INT_OPERATION(Xor, ^, "Xor");
MATH_INT_OPERATION(And, &, "And");
MATH_INT_OPERATION(Or, |, "Or");
MATH_INT_OPERATION(Mod, %, "Mod");
MATH_INT_OPERATION(LShift, <<, "LShift");
MATH_INT_OPERATION(RShift, >>, "RShift");

#define MATH_BLOCK(NAME)                                                       \
  RUNTIME_BLOCK_FACTORY(Math, NAME);                                           \
  RUNTIME_BLOCK_destroy(NAME);                                                 \
  RUNTIME_BLOCK_cleanup(NAME);                                                 \
  RUNTIME_BLOCK_setup(NAME);                                                   \
  RUNTIME_BLOCK_inputTypes(NAME);                                              \
  RUNTIME_BLOCK_outputTypes(NAME);                                             \
  RUNTIME_BLOCK_parameters(NAME);                                              \
  RUNTIME_BLOCK_inferTypes(NAME);                                              \
  RUNTIME_BLOCK_consumedVariables(NAME);                                       \
  RUNTIME_BLOCK_setParam(NAME);                                                \
  RUNTIME_BLOCK_getParam(NAME);                                                \
  RUNTIME_BLOCK_activate(NAME);                                                \
  RUNTIME_BLOCK_END(NAME);
}; // namespace Math
}; // namespace chainblocks