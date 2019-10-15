/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#include "shared.hpp"

namespace chainblocks {
namespace Assert {

struct Base {
  static inline ParamsInfo assertParamsInfo = ParamsInfo(
      ParamsInfo::Param("Value", "The value to test against for equality.",
                        CBTypesInfo(SharedTypes::anyInfo)),
      ParamsInfo::Param("Abort", "If we should abort the process on failure.",
                        CBTypesInfo(SharedTypes::boolInfo)));

  CBVar value{};
  bool aborting;

  void destroy() { destroyVar(value); }

  CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }

  CBTypesInfo outputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }

  CBParametersInfo parameters() { return CBParametersInfo(assertParamsInfo); }

  void setParam(int index, CBVar inValue) {
    switch (index) {
    case 0:
      destroyVar(value);
      cloneVar(value, inValue);
      break;
    case 1:
      aborting = inValue.payload.boolValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    auto res = CBVar();
    switch (index) {
    case 0:
      res = value;
      break;
    case 1:
      res.valueType = Bool;
      res.payload.boolValue = aborting;
      break;
    default:
      break;
    }
    return res;
  }
};

struct Is : public Base {
  CBVar activate(CBContext *context, const CBVar &input) {
    if (input != value) {
      LOG(ERROR) << "Failed assertion Is, input: " << input
                 << " expected: " << value;
      if (aborting)
        abort();
      else
        throw CBException("Assert failed - Is");
    }

    return input;
  }
};

struct IsNot : public Base {
  CBVar activate(CBContext *context, const CBVar &input) {
    if (input == value) {
      LOG(ERROR) << "Failed assertion IsNot, input: " << input
                 << " not expected: " << value;
      if (aborting)
        abort();
      else
        throw CBException("Assert failed - IsNot");
    }

    return input;
  }
};

// Register Is
RUNTIME_BLOCK(Assert, Is);
RUNTIME_BLOCK_destroy(Is);
RUNTIME_BLOCK_inputTypes(Is);
RUNTIME_BLOCK_outputTypes(Is);
RUNTIME_BLOCK_parameters(Is);
RUNTIME_BLOCK_setParam(Is);
RUNTIME_BLOCK_getParam(Is);
RUNTIME_BLOCK_activate(Is);
RUNTIME_BLOCK_END(Is);

// Register IsNot
RUNTIME_BLOCK(Assert, IsNot);
RUNTIME_BLOCK_destroy(IsNot);
RUNTIME_BLOCK_inputTypes(IsNot);
RUNTIME_BLOCK_outputTypes(IsNot);
RUNTIME_BLOCK_parameters(IsNot);
RUNTIME_BLOCK_setParam(IsNot);
RUNTIME_BLOCK_getParam(IsNot);
RUNTIME_BLOCK_activate(IsNot);
RUNTIME_BLOCK_END(IsNot);
}; // namespace Assert

void registerAssertBlocks() {
  REGISTER_BLOCK(Assert, Is);
  REGISTER_BLOCK(Assert, IsNot);
}
}; // namespace chainblocks
                     \
          return true;                                                         \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.float2Value[1]);                   \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        break;                                                                 \
      case Float3:                                                             \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.float3Value[0]);                   \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.float3Value[1]);                   \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.float3Value[2]);                   \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        break;                                                                 \
      case Float4:                                                             \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.float4Value[0]);                   \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.float4Value[1]);                   \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.float4Value[2]);                   \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.float4Value[3]);                   \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        break;                                                                 \
      case Int:                                                                \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.intValue);                         \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        break;                                                                 \
      case Int2:                                                               \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.int2Value[0]);                     \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.int2Value[1]);                     \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        break;                                                                 \
      case Int3:                                                               \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.int3Value[0]);                     \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.int3Value[1]);                     \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.int3Value[2]);                     \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        break;                                                                 \
      case Int4:                                                               \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.int4Value[0]);                     \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.int4Value[1]);                     \
        index++;                                                               \
        if (index == 4)                                                        \
          return true;                                                         \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.int4Value[2]);                     \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.int4Value[3]);                     \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        break;                                                                 \
      default:                                                                 \
        throw CBException("Cannot cast given input type.");                    \
      }                                                                        \
      return false;                                                            \
    }                                                                          \
                                                                               \
    CBVar activate(CBContext *context, const CBVar &input) {                   \
      int index = 0;                                                           \
      CBVar output{};                                                          \
      output.valueType = _varName_##_width_;                                   \
      switch (input.valueType) {                                               \
      case Seq: {                                                              \
        for (auto i = 0;                                                       \
             i < _width_ && i < stbds_arrlen(input.payload.seqValue); i++) {   \
          if (convert(output, index, input.payload.seqValue[i]))               \
            return output;                                                     \
        }                                                                      \
        break;                                                                 \
      }                                                                        \
      case Int:                                                                \
      case Int2:                                                               \
      case Int3:                                                               \
      case Int4:                                                               \
      case Float:                                                              \
      case Float2:                                                             \
      case Float3:                                                             \
      case Float4:                                                             \
      case String:                                                             \
        if (convert(output, index, input))                                     \
          return output;                                                       \
        break;                                                                 \
      default:                                                                 \
        throw CBException("Cannot cast given input type.");                    \
      }                                                                        \
      return output;                                                           \
    }                                                                          \
  };                                                                           \
  RUNTIME_CORE_BLOCK(To##_varName_##_width_);                                  \
  RUNTIME_BLOCK_inputTypes(To##_varName_##_width_);                            \
  RUNTIME_BLOCK_outputTypes(To##_varName_##_width_);                           \
  RUNTIME_BLOCK_inferTypes(To##_varName_##_width_);                            \
  RUNTIME_BLOCK_activate(To##_varName_##_width_);                              \
  RUNTIME_BLOCK_END(To##_varName_##_width_);

// TODO improve this
#define TO_SOMETHING_SIMPLE(_varName_, _type_, _payload_, _strOp_, _info_)     \
  struct To##_varName_ {                                                       \
    static inline TypesInfo &singleOutput = SharedTypes::_info_;               \
    CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }     \
    CBTypesInfo outputTypes() { return CBTypesInfo(singleOutput); }            \
                                                                               \
    CBTypeInfo inferTypes(CBTypeInfo inputType,                                \
                          CBExposedTypesInfo consumableVariables) {            \
      return CBTypeInfo(singleOutput);                                         \
    }                                                                          \
                                                                               \
    bool convert(CBVar &dst, const CBVar &src) {                               \
      switch (src.valueType) {                                                 \
      case String: {                                                           \
        dst.payload._payload_ =                                                \
            static_cast<_type_>(std::_strOp_(src.payload.stringValue));        \
        break;                                                                 \
      }                                                                        \
      case Float:                                                              \
        dst.payload._payload_ = static_cast<_type_>(src.payload.floatValue);   \
        break;                                                                 \
      case Float2:                                                             \
        dst.payload._payload_ =                                                \
            static_cast<_type_>(src.payload.float2Value[0]);                   \
        break;                                                                 \
      case Float3:                                                             \
        dst.payload._payload_ =                                                \
            static_cast<_type_>(src.payload.float3Value[0]);                   \
        break;                                                                 \
      case Float4:                                                             \
        dst.payload._payload_ =                                                \
            static_cast<_type_>(src.payload.float4Value[0]);                   \
        break;                                                                 \
      case Int:                                                                \
        dst.payload._payload_ = static_cast<_type_>(src.payload.intValue);     \
        break;                                                                 \
      case Int2:                                                               \
        dst.payload._payload_ = static_cast<_type_>(src.payload.int2Value[0]); \
        break;                                                                 \
      case Int3:                                                               \
        dst.payload._payload_ = static_cast<_type_>(src.payload.int3Value[0]); \
        break;                                                                 \
      case Int4:                                                               \
        dst.payload._payload_ = static_cast<_type_>(src.payload.int4Value[0]); \
        break;                                                                 \
      default:                                                                 \
        throw CBException("Cannot cast given input type.");                    \
      }                                                                        \
      return false;                                                            \
    }                                                                          \
                                                                               \
    CBVar activate(CBContext *context, const CBVar &input) {                   \
      CBVar output{};                                                          \
      output.valueType = _varName_;                                            \
      switch (input.valueType) {                                               \
      case Seq: {                                                              \
        for (auto i = 0; i < 1 && i < stbds_arrlen(input.payload.seqValue);    \
             i++) {                                                            \
          if (convert(output, input.payload.seqValue[i]))                      \
            return output;                                                     \
        }                                                                      \
        break;                                                                 \
      }                                                                        \
      case Int:                                                                \
      case Int2:                                                               \
      case Int3:                                                               \
      case Int4:                                                               \
      case Float:                                                              \
      case Float2:                                                             \
      case Float3:                                                             \
      case Float4:                                                             \
      case String:                                                             \
        if (convert(output, input))                                            \
          return output;                                                       \
        break;                                                                 \
      default:                                                                 \
        throw CBException("Cannot cast given input type.");                    \
      }                                                                        \
      return output;                                                           \
    }                                                                          \
  };                                                                           \
  RUNTIME_CORE_BLOCK(To##_varName_);                                           \
  RUNTIME_BLOCK_inputTypes(To##_varName_);                                     \
  RUNTIME_BLOCK_outputTypes(To##_varName_);                                    \
  RUNTIME_BLOCK_inferTypes(To##_varName_);                                     \
  RUNTIME_BLOCK_activate(To##_varName_);                                       \
  RUNTIME_BLOCK_END(To##_varName_);

TO_SOMETHING_SIMPLE(Int, int64_t, intValue, stoi, intInfo);
TO_SOMETHING(Int, 2, int64_t, int2Value, stoi, int2Info);
TO_SOMETHING(Int, 3, int32_t, int3Value, stoi, int3Info);
TO_SOMETHING(Int, 4, int32_t, int4Value, stoi, int4Info);
TO_SOMETHING_SIMPLE(Float, double, floatValue, stod, floatInfo);
TO_SOMETHING(Float, 2, double, float2Value, stod, float2Info);
TO_SOMETHING(Float, 3, float, float3Value, stof, float3Info);
TO_SOMETHING(Float, 4, float, float4Value, stof, float4Info);

struct ToString {
  VarStringStream stream;
  static CBTypesInfo inputTypes() { return CBTypesInfo(CoreInfo::anyInfo); }
  static CBTypesInfo outputTypes() { return CBTypesInfo(CoreInfo::strInfo); }
  CBVar activate(CBContext *context, const CBVar &input) {
    stream.write(input);
    return Var(stream.str());
  }
};

RUNTIME_CORE_BLOCK(ToString);
RUNTIME_BLOCK_inputTypes(ToString);
RUNTIME_BLOCK_outputTypes(ToString);
RUNTIME_BLOCK_activate(ToString);
RUNTIME_BLOCK_END(ToString);

struct ToHex {
  VarStringStream stream;
  static CBTypesInfo inputTypes() { return CBTypesInfo(CoreInfo::intInfo); }
  static CBTypesInfo outputTypes() { return CBTypesInfo(CoreInfo::strInfo); }
  CBVar activate(CBContext *context, const CBVar &input) {
    stream.tryWriteHex(input);
    return Var(stream.str());
  }
};

RUNTIME_CORE_BLOCK(ToHex);
RUNTIME_BLOCK_inputTypes(ToHex);
RUNTIME_BLOCK_outputTypes(ToHex);
RUNTIME_BLOCK_activate(ToHex);
RUNTIME_BLOCK_END(ToHex);

struct VarAddr {
  VarStringStream stream;
  static CBTypesInfo inputTypes() { return CBTypesInfo(CoreInfo::strInfo); }
  static CBTypesInfo outputTypes() { return CBTypesInfo(CoreInfo::intInfo); }
  CBVar activate(CBContext *context, const CBVar &input) {
    auto v = contextVariable(context, input.payload.stringValue);
    return Var(reinterpret_cast<int64_t>(v));
  }
};

RUNTIME_CORE_BLOCK(VarAddr);
RUNTIME_BLOCK_inputTypes(VarAddr);
RUNTIME_BLOCK_outputTypes(VarAddr);
RUNTIME_BLOCK_activate(VarAddr);
RUNTIME_BLOCK_END(VarAddr);

struct BitSwap32 {
  static CBTypesInfo inputTypes() { return CBTypesInfo(CoreInfo::intInfo); }
  static CBTypesInfo outputTypes() { return CBTypesInfo(CoreInfo::intInfo); }
  CBVar activate(CBContext *context, const CBVar &input) {
    auto i32 = static_cast<uint32_t>(input.payload.intValue);
    i32 = __builtin_bswap32(i32);
    return Var(static_cast<int64_t>(i32));
  }
};

RUNTIME_CORE_BLOCK(BitSwap32);
RUNTIME_BLOCK_inputTypes(BitSwap32);
RUNTIME_BLOCK_outputTypes(BitSwap32);
RUNTIME_BLOCK_activate(BitSwap32);
RUNTIME_BLOCK_END(BitSwap32);

struct BitSwap64 {
  static CBTypesInfo inputTypes() { return CBTypesInfo(CoreInfo::intInfo); }
  static CBTypesInfo outputTypes() { return CBTypesInfo(CoreInfo::intInfo); }
  CBVar activate(CBContext *context, const CBVar &input) {
    auto i64 = static_cast<uint64_t>(input.payload.intValue);
    i64 = __builtin_bswap64(i64);
    return Var(static_cast<int64_t>(i64));
  }
};

RUNTIME_CORE_BLOCK(BitSwap64);
RUNTIME_BLOCK_inputTypes(BitSwap64);
RUNTIME_BLOCK_outputTypes(BitSwap64);
RUNTIME_BLOCK_activate(BitSwap64);
RUNTIME_BLOCK_END(BitSwap64);

// As, reinterpret
#define AS_SOMETHING_SIMPLE(_varName_, _varName_2, _type_, _payload_, _strOp_, \
                            _info_)                                            \
  struct As##_varName_2 {                                                      \
    static inline TypesInfo &singleOutput = SharedTypes::_info_;               \
    CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }     \
    CBTypesInfo outputTypes() { return CBTypesInfo(singleOutput); }            \
                                                                               \
    CBTypeInfo inferTypes(CBTypeInfo inputType,                                \
                          CBExposedTypesInfo consumableVariables) {            \
      return CBTypeInfo(singleOutput);                                         \
    }                                                                          \
                                                                               \
    bool convert(CBVar &dst, CBVar &src) {                                     \
      switch (src.valueType) {                                                 \
      case String: {                                                           \
        auto val = std::_strOp_(src.payload.stringValue);                      \
        dst.payload._payload_ = reinterpret_cast<_type_ &>(val);               \
        break;                                                                 \
      }                                                                        \
      case Float:                                                              \
        dst.payload._payload_ =                                                \
            reinterpret_cast<_type_ &>(src.payload.floatValue);                \
        break;                                                                 \
      case Float2: {                                                           \
        auto val = src.payload.float2Value[0];                                 \
        dst.payload._payload_ = reinterpret_cast<_type_ &>(val);               \
        break;                                                                 \
      }                                                                        \
      case Float3: {                                                           \
        auto val = src.payload.float3Value[0];                                 \
        dst.payload._payload_ = reinterpret_cast<_type_ &>(val);               \
        break;                                                                 \
      }                                                                        \
      case Float4: {                                                           \
        auto val = src.payload.float4Value[0];                                 \
        dst.payload._payload_ = reinterpret_cast<_type_ &>(val);               \
        break;                                                                 \
      }                                                                        \
      case Int:                                                                \
        dst.payload._payload_ =                                                \
            reinterpret_cast<_type_ &>(src.payload.intValue);                  \
        break;                                                                 \
      case Int2: {                                                             \
        auto val = src.payload.int2Value[0];                                   \
        dst.payload._payload_ = reinterpret_cast<_type_ &>(val);               \
        break;                                                                 \
      }                                                                        \
      case Int3: {                                                             \
        auto val = src.payload.int3Value[0];                                   \
        dst.payload._payload_ = reinterpret_cast<_type_ &>(val);               \
        break;                                                                 \
      }                                                                        \
      case Int4: {                                                             \
        auto val = src.payload.int4Value[0];                                   \
        dst.payload._payload_ = reinterpret_cast<_type_ &>(val);               \
        break;                                                                 \
      }                                                                        \
      default:                                                                 \
        throw CBException("Cannot cast given input type.");                    \
      }                                                                        \
      return false;                                                            \
    }                                                                          \
                                                                               \
    CBVar activate(CBContext *context, const CBVar &input) {                   \
      CBVar output{};                                                          \
      output.valueType = _varName_;                                            \
      switch (input.valueType) {                                               \
      case Seq: {                                                              \
        for (auto i = 0; i < 1 && i < stbds_arrlen(input.payload.seqValue);    \
             i++) {                                                            \
          if (convert(output, input.payload.seqValue[i]))                      \
            return output;                                                     \
        }                                                                      \
        break;                                                                 \
      }                                                                        \
      case Int:                                                                \
      case Int2:                                                               \
      case Int3:                                                               \
      case Int4:                                                               \
      case Float:                                                              \
      case Float2:                                                             \
      case Float3:                                                             \
      case Float4:                                                             \
      case String:                                                             \
        if (convert(output, const_cast<CBVar &>(input)))                       \
          return output;                                                       \
        break;                                                                 \
      default:                                                                 \
        throw CBException("Cannot cast given input type.");                    \
      }                                                                        \
      return output;                                                           \
    }                                                                          \
  };                                                                           \
  RUNTIME_CORE_BLOCK(As##_varName_2);                                          \
  RUNTIME_BLOCK_inputTypes(As##_varName_2);                                    \
  RUNTIME_BLOCK_outputTypes(As##_varName_2);                                   \
  RUNTIME_BLOCK_inferTypes(As##_varName_2);                                    \
  RUNTIME_BLOCK_activate(As##_varName_2);                                      \
  RUNTIME_BLOCK_END(As##_varName_2);

AS_SOMETHING_SIMPLE(Int, Int32, int32_t, intValue, stoi, intInfo);
AS_SOMETHING_SIMPLE(Int, Int64, int64_t, intValue, stoi, intInfo);
AS_SOMETHING_SIMPLE(Float, Float32, float, floatValue, stof, floatInfo);
AS_SOMETHING_SIMPLE(Float, Float64, double, floatValue, stod, floatInfo);

// actual type = AT = the c type
template <CBType OT, typename AT> struct BytesToX {
  static inline TypeInfo outputType = TypeInfo(OT);
  static inline TypesInfo outputInfo =
      TypesInfo(TypeInfo::Sequence(outputType));

  CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::bytesInfo); }
  CBTypesInfo outputTypes() { return CBTypesInfo(outputInfo); }

  CBSeq _outputCache = nullptr;

  void destroy() {
    if (_outputCache) {
      stbds_arrfree(_outputCache);
      _outputCache = nullptr;
    }
  }

  void convert(CBVar &dst, uint8_t *addr) {
    // compile time magic
    if constexpr (std::is_same<AT, int8_t>::value ||
                  std::is_same<AT, int16_t>::value ||
                  std::is_same<AT, int32_t>::value ||
                  std::is_same<AT, int64_t>::value) {
      dst.valueType = Int;
      auto p = reinterpret_cast<AT *>(addr);
      dst.payload.intValue = static_cast<int64_t>(*p);
    } else if constexpr (std::is_same<AT, float>::value ||
                         std::is_same<AT, double>::value) {
      dst.valueType = Float;
      auto p = reinterpret_cast<AT *>(addr);
      dst.payload.floatValue = static_cast<double>(*p);
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    const auto tsize = sizeof(AT);
    if (unlikely(input.payload.bytesSize < tsize)) {
      throw CBException("BytesToX, Not enough bytes to convert to " +
                        type2Name(OT));
    } else if (input.payload.bytesSize == tsize) {
      // exact size, 1 value
      stbds_arrsetlen(_outputCache, 1);
      convert(_outputCache[0], input.payload.bytesValue);
    } else {
      // many values
      auto len = input.payload.bytesSize / tsize; // int division, fine
      stbds_arrsetlen(_outputCache, len);
      for (auto i = 0; i < len; i++) {
        convert(_outputCache[i], input.payload.bytesValue + (sizeof(AT) * i));
      }
    }
    return Var(_outputCache);
  }
};

#define BYTES_TO_BLOCK(_name_, _cbtype_, _ctype_)                              \
  struct BytesTo##_name_ : public BytesToX<_cbtype_, _ctype_> {};              \
  RUNTIME_CORE_BLOCK(BytesTo##_name_);                                         \
  RUNTIME_BLOCK_destroy(BytesTo##_name_);                                      \
  RUNTIME_BLOCK_inputTypes(BytesTo##_name_);                                   \
  RUNTIME_BLOCK_outputTypes(BytesTo##_name_);                                  \
  RUNTIME_BLOCK_activate(BytesTo##_name_);                                     \
  RUNTIME_BLOCK_END(BytesTo##_name_);

BYTES_TO_BLOCK(Float32, Float, float);
BYTES_TO_BLOCK(Float64, Float, double);
BYTES_TO_BLOCK(Int8, Int, int8_t);
BYTES_TO_BLOCK(Int16, Int, int16_t);
BYTES_TO_BLOCK(Int32, Int, int32_t);
BYTES_TO_BLOCK(Int64, Int, int64_t);

template <CBType ET> struct ExpectX {
  static inline TypeInfo outputType = TypeInfo(ET);
  static inline TypesInfo outputInfo = TypesInfo(outputType);
  CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }
  CBTypesInfo outputTypes() { return CBTypesInfo(outputInfo); }
  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(input.valueType != ET)) {
      throw CBException("Expected type " + type2Name(ET) + " got instead " +
                        type2Name(input.valueType));
    }
    return input;
  }
};

#define EXPECT_BLOCK(_name_, _cbtype_)                                         \
  struct Expect##_name_ : public ExpectX<_cbtype_> {};                         \
  RUNTIME_CORE_BLOCK(Expect##_name_);                                          \
  RUNTIME_BLOCK_inputTypes(Expect##_name_);                                    \
  RUNTIME_BLOCK_outputTypes(Expect##_name_);                                   \
  RUNTIME_BLOCK_activate(Expect##_name_);                                      \
  RUNTIME_BLOCK_END(Expect##_name_);

EXPECT_BLOCK(Float, Float);
EXPECT_BLOCK(Float2, Float2);
EXPECT_BLOCK(Float3, Float3);
EXPECT_BLOCK(Float4, Float4);
EXPECT_BLOCK(Int, Int);
EXPECT_BLOCK(Int2, Int2);
EXPECT_BLOCK(Int3, Int3);
EXPECT_BLOCK(Int4, Int4);
EXPECT_BLOCK(Bytes, Bytes);
EXPECT_BLOCK(String, String);

struct ToBytes {
  struct StreamBuffer : std::streambuf {
    std::vector<uint8_t> &data;
    StreamBuffer(std::vector<uint8_t> &buffer) : data(buffer) {}
    int overflow(int c) override {
      data.push_back(static_cast<char>(c));
      return 0;
    }
  };

  std::vector<uint8_t> _buffer;

  CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }
  CBTypesInfo outputTypes() { return CBTypesInfo(SharedTypes::bytesInfo); }

  size_t getSize(const CBVar &input) {
    switch (input.valueType) {
    case CBType::Enum: {
      return sizeof(CBEnum);
    }
    case CBType::Bool: {
      return sizeof(CBBool);
    }
    case CBType::Int: {
      return sizeof(CBInt);
    }
    case CBType::Int2: {
      return sizeof(CBInt2);
    }
    case CBType::Int3: {
      return sizeof(CBInt3);
    }
    case CBType::Int4: {
      return sizeof(CBInt4);
    }
    case CBType::Int8: {
      return sizeof(CBInt8);
    }
    case CBType::Int16: {
      return sizeof(CBInt16);
    }
    case CBType::Float: {
      return sizeof(CBFloat);
    }
    case CBType::Float2: {
      return sizeof(CBFloat2);
    }
    case CBType::Float3: {
      return sizeof(CBFloat3);
    }
    case CBType::Float4: {
      return sizeof(CBFloat4);
    }
    case CBType::Color: {
      return sizeof(CBColor);
    }
    case CBType::EndOfBlittableTypes:
    case CBType::None:
    case CBType::Object:
    case CBType::Any:
    case CBType::String:
    case CBType::ContextVar:
    case CBType::Image:
    case CBType::Chain:
    case CBType::Block:
    case CBType::Table:
    case CBType::Seq:
    case CBType::Bytes: {
      return -1; // unsupported for now
    }
    }
    return -1;
  }

  void convert(const CBVar &input) {
    switch (input.valueType) {
    case CBType::EndOfBlittableTypes:
    case CBType::None:
    case CBType::Object:
    case CBType::Any: {
      break;
    }
    case CBType::Enum: {
      _buffer.resize(sizeof(CBEnum));
      memcpy(&_buffer.front(), &input.payload.enumValue, sizeof(CBEnum));
      break;
    }
    case CBType::Bool: {
      _buffer.resize(sizeof(CBBool));
      memcpy(&_buffer.front(), &input.payload.boolValue, sizeof(CBBool));
      break;
    }
    case CBType::Int: {
      _buffer.resize(sizeof(CBInt));
      memcpy(&_buffer.front(), &input.payload.intValue, sizeof(CBInt));
      break;
    }
    case CBType::Int2: {
      _buffer.resize(sizeof(CBInt2));
      memcpy(&_buffer.front(), &input.payload.int2Value, sizeof(CBInt2));
      break;
    }
    case CBType::Int3: {
      _buffer.resize(sizeof(CBInt3));
      memcpy(&_buffer.front(), &input.payload.int3Value, sizeof(CBInt3));
      break;
    }
    case CBType::Int4: {
      _buffer.resize(sizeof(CBInt4));
      memcpy(&_buffer.front(), &input.payload.int4Value, sizeof(CBInt4));
      break;
    }
    case CBType::Int8: {
      _buffer.resize(sizeof(CBInt8));
      memcpy(&_buffer.front(), &input.payload.int8Value, sizeof(CBInt8));
      break;
    }
    case CBType::Int16: {
      _buffer.resize(sizeof(CBInt16));
      memcpy(&_buffer.front(), &input.payload.int16Value, sizeof(CBInt16));
      break;
    }
    case CBType::Float: {
      _buffer.resize(sizeof(CBFloat));
      memcpy(&_buffer.front(), &input.payload.floatValue, sizeof(CBFloat));
      break;
    }
    case CBType::Float2: {
      _buffer.resize(sizeof(CBFloat2));
      memcpy(&_buffer.front(), &input.payload.float2Value, sizeof(CBFloat2));
      break;
    }
    case CBType::Float3: {
      _buffer.resize(sizeof(CBFloat3));
      memcpy(&_buffer.front(), &input.payload.float3Value, sizeof(CBFloat3));
      break;
    }
    case CBType::Float4: {
      _buffer.resize(sizeof(CBFloat4));
      memcpy(&_buffer.front(), &input.payload.float4Value, sizeof(CBFloat4));
      break;
    }
    case CBType::Color: {
      _buffer.resize(sizeof(CBColor));
      memcpy(&_buffer.front(), &input.payload.colorValue, sizeof(CBColor));
      break;
    }
    case CBType::Bytes: {
      _buffer.resize(input.payload.bytesSize);
      memcpy(&_buffer.front(), input.payload.bytesValue,
             input.payload.bytesSize);
      break;
    }
    case CBType::String:
    case CBType::ContextVar: {
      auto len = strlen(input.payload.stringValue);
      _buffer.resize(len + 1);
      memcpy(&_buffer.front(), input.payload.stringValue, len + 1);
      break;
    }
    case CBType::Image: {
      auto size = input.payload.imageValue.width *
                  input.payload.imageValue.height *
                  input.payload.imageValue.channels;
      _buffer.resize(size);
      memcpy(&_buffer.front(), input.payload.imageValue.data, size);
      break;
    }
    case CBType::Seq: {
      // For performance reasons we enforce this to be a flat sequence,
      // if not the case run (Flatten) before
      auto len = stbds_arrlen(input.payload.seqValue);
      if (len > 0) {
        auto itemSize = getSize(input.payload.seqValue[0]);
        if (itemSize == -1) {
          throw CBException("ToBytes, unsupported Seq type.");
        }
        _buffer.resize(itemSize * len);
        for (auto i = 0; i < len; i++) {
          // we cheat using any pointer in the union
          // since only blittables are allowed
          memcpy((&_buffer.front()) + (itemSize * i),
                 &input.payload.seqValue[i].payload.intValue, itemSize);
        }
      }
      break;
    }
    case CBType::Chain:
    case CBType::Block:
    case CBType::Table: {
      throw CBException("ToBytes, unsupported type, likely TODO.");
    }
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    convert(input);
    return Var(&_buffer.front(), _buffer.size());
  }
};

RUNTIME_CORE_BLOCK(ToBytes);
RUNTIME_BLOCK_inputTypes(ToBytes);
RUNTIME_BLOCK_outputTypes(ToBytes);
RUNTIME_BLOCK_activate(ToBytes);
RUNTIME_BLOCK_END(ToBytes);

void registerCastingBlocks() {
  REGISTER_CORE_BLOCK(ToInt);
  REGISTER_CORE_BLOCK(ToInt2);
  REGISTER_CORE_BLOCK(ToInt3);
  REGISTER_CORE_BLOCK(ToInt4);
  REGISTER_CORE_BLOCK(ToFloat);
  REGISTER_CORE_BLOCK(ToFloat2);
  REGISTER_CORE_BLOCK(ToFloat3);
  REGISTER_CORE_BLOCK(ToFloat4);
  REGISTER_CORE_BLOCK(ToString);
  REGISTER_CORE_BLOCK(ToHex);
  REGISTER_CORE_BLOCK(VarAddr);
  REGISTER_CORE_BLOCK(BitSwap32);
  REGISTER_CORE_BLOCK(BitSwap64);
  REGISTER_CORE_BLOCK(AsInt32);
  REGISTER_CORE_BLOCK(AsInt64);
  REGISTER_CORE_BLOCK(AsFloat32);
  REGISTER_CORE_BLOCK(AsFloat64);
  REGISTER_CORE_BLOCK(BytesToFloat32);
  REGISTER_CORE_BLOCK(BytesToFloat64);
  REGISTER_CORE_BLOCK(BytesToInt8);
  REGISTER_CORE_BLOCK(BytesToInt16);
  REGISTER_CORE_BLOCK(BytesToInt32);
  REGISTER_CORE_BLOCK(BytesToInt64);
  REGISTER_CORE_BLOCK(ExpectInt);
  REGISTER_CORE_BLOCK(ExpectInt2);
  REGISTER_CORE_BLOCK(ExpectInt3);
  REGISTER_CORE_BLOCK(ExpectInt4);
  REGISTER_CORE_BLOCK(ExpectFloat);
  REGISTER_CORE_BLOCK(ExpectFloat2);
  REGISTER_CORE_BLOCK(ExpectFloat3);
  REGISTER_CORE_BLOCK(ExpectFloat4);
  REGISTER_CORE_BLOCK(ExpectBytes);
  REGISTER_CORE_BLOCK(ExpectString);
  REGISTER_CORE_BLOCK(ToBytes);
}
}; // namespace chainblocks
esVar = nullptr;
  ExposedInfo _exposedInfo{};

  void destroy() {
    if (_cachedResult) {
      stbds_arrfree(_cachedResult);
    }
    destroyVar(_indices);
  }

  void cleanup() { _indicesVar = nullptr; }

  static CBTypesInfo inputTypes() { return CBTypesInfo(CoreInfo::anySeqInfo); }

  static CBTypesInfo outputTypes() { return CBTypesInfo(CoreInfo::anyInfo); }

  static CBParametersInfo parameters() {
    return CBParametersInfo(indicesParamsInfo);
  }

  CBTypeInfo inferTypes(CBTypeInfo inputType,
                        CBExposedTypesInfo consumableVariables) {
    // Figure if we output a sequence or not
    if (_indices.valueType == Seq) {
      if (inputType.basicType == Seq) {
        return inputType; // multiple values
      }
    } else if (_indices.valueType == Int) {
      if (inputType.basicType == Seq && inputType.seqType) {
        return *inputType.seqType; // single value
      }
    } else { // ContextVar
      IterableExposedInfo infos(consumableVariables);
      for (auto &info : infos) {
        if (strcmp(info.name, _indices.payload.stringValue) == 0) {
          if (info.exposedType.basicType == Seq && info.exposedType.seqType &&
              info.exposedType.seqType->basicType == Int) {
            if (inputType.basicType == Seq) {
              return inputType; // multiple values
            }
          } else if (info.exposedType.basicType == Int) {
            if (inputType.basicType == Seq && inputType.seqType) {
              return *inputType.seqType; // single value
            }
          } else {
            auto msg = "Take indices variable " + std::string(info.name) +
                       " expected to be either a Seq or a Int";
            throw CBException(msg);
          }
        }
      }
    }
    throw CBException("Take expected a sequence as input.");
  }

  CBExposedTypesInfo consumedVariables() {
    if (_indices.valueType == ContextVar) {
      _exposedInfo =
          ExposedInfo(ExposedInfo::Variable(_indices.payload.stringValue,
                                            "The consumed variable.",
                                            CBTypeInfo(CoreInfo::intInfo)),
                      ExposedInfo::Variable(_indices.payload.stringValue,
                                            "The consumed variables.",
                                            CBTypeInfo(CoreInfo::intSeqInfo)));
      return CBExposedTypesInfo(_exposedInfo);
    } else {
      return nullptr;
    }
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      cloneVar(_indices, value);
      _indicesVar = nullptr;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) { return _indices; }

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    auto inputLen = stbds_arrlen(input.payload.seqValue);
    auto indices = _indices;

    if (_indices.valueType == ContextVar && !_indicesVar) {
      _indicesVar = contextVariable(context, _indices.payload.stringValue);
    }

    if (_indicesVar) {
      indices = *_indicesVar;
    }

    if (indices.valueType == Int) {
      auto index = indices.payload.intValue;
      if (index >= inputLen) {
        LOG(ERROR) << "Take out of range! len:" << inputLen
                   << " wanted index: " << index;
        throw CBException("Take out of range!");
      }
      return input.payload.seqValue[index];
    } else {
      // Else it's a seq
      auto nindices = stbds_arrlen(indices.payload.seqValue);
      stbds_arrsetlen(_cachedResult, nindices);
      for (auto i = 0; nindices > i; i++) {
        auto index = indices.payload.seqValue[i].payload.intValue;
        if (index >= inputLen) {
          LOG(ERROR) << "Take out of range! len:" << inputLen
                     << " wanted index: " << index;
          throw CBException("Take out of range!");
        }
        _cachedResult[i] = input.payload.seqValue[index];
      }
      return Var(_cachedResult);
    }
  }
};

struct Limit {
  static inline ParamsInfo indicesParamsInfo = ParamsInfo(ParamsInfo::Param(
      "Max", "How many maximum elements to take from the input sequence.",
      CBTypesInfo(CoreInfo::intInfo)));

  CBSeq _cachedResult = nullptr;
  int64_t _max = 0;

  void destroy() {
    if (_cachedResult) {
      stbds_arrfree(_cachedResult);
    }
  }

  static CBTypesInfo inputTypes() { return CBTypesInfo(CoreInfo::anySeqInfo); }

  static CBTypesInfo outputTypes() { return CBTypesInfo(CoreInfo::anyInfo); }

  static CBParametersInfo parameters() {
    return CBParametersInfo(indicesParamsInfo);
  }

  CBTypeInfo inferTypes(CBTypeInfo inputType,
                        CBExposedTypesInfo consumableVariables) {
    // Figure if we output a sequence or not
    if (_max > 1) {
      if (inputType.basicType == Seq) {
        return inputType; // multiple values
      }
    } else {
      if (inputType.basicType == Seq && inputType.seqType) {
        return *inputType.seqType; // single value
      }
    }
    throw CBException("Limit expected a sequence as input.");
  }

  void setParam(int index, CBVar value) { _max = value.payload.intValue; }

  CBVar getParam(int index) { return Var(_max); }

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    int64_t inputLen = stbds_arrlen(input.payload.seqValue);

    if (_max == 1) {
      auto index = 0;
      if (index >= inputLen) {
        LOG(ERROR) << "Limit out of range! len:" << inputLen
                   << " wanted index: " << index;
        throw CBException("Limit out of range!");
      }
      return input.payload.seqValue[index];
    } else {
      // Else it's a seq
      auto nindices = std::min(inputLen, _max);
      stbds_arrsetlen(_cachedResult, nindices);
      for (auto i = 0; i < nindices; i++) {
        _cachedResult[i] = input.payload.seqValue[i];
      }
      return Var(_cachedResult);
    }
  }
};

struct BlocksUser {
  CBVar _blocks{};
  CBValidationResult _chainValidation{};

  void destroy() {
    if (_blocks.valueType == Seq) {
      for (auto i = 0; i < stbds_arrlen(_blocks.payload.seqValue); i++) {
        auto &blk = _blocks.payload.seqValue[i].payload.blockValue;
        blk->cleanup(blk);
        blk->destroy(blk);
      }
    } else if (_blocks.valueType == Block) {
      _blocks.payload.blockValue->cleanup(_blocks.payload.blockValue);
      _blocks.payload.blockValue->destroy(_blocks.payload.blockValue);
    }
    destroyVar(_blocks);
    stbds_arrfree(_chainValidation.exposedInfo);
  }

  void cleanup() {
    if (_blocks.valueType == Seq) {
      for (auto i = 0; i < stbds_arrlen(_blocks.payload.seqValue); i++) {
        auto &blk = _blocks.payload.seqValue[i].payload.blockValue;
        blk->cleanup(blk);
      }
    } else if (_blocks.valueType == Block) {
      _blocks.payload.blockValue->cleanup(_blocks.payload.blockValue);
    }
  }

  CBTypeInfo inferTypes(CBTypeInfo inputType, CBExposedTypesInfo consumables) {
    // Free any previous result!
    stbds_arrfree(_chainValidation.exposedInfo);
    _chainValidation.exposedInfo = nullptr;

    std::vector<CBlock *> blocks;
    if (_blocks.valueType == Block) {
      blocks.push_back(_blocks.payload.blockValue);
    } else {
      for (auto i = 0; i < stbds_arrlen(_blocks.payload.seqValue); i++) {
        blocks.push_back(_blocks.payload.seqValue[i].payload.blockValue);
      }
    }
    _chainValidation = validateConnections(
        blocks,
        [](const CBlock *errorBlock, const char *errorTxt, bool nonfatalWarning,
           void *userData) {
          if (!nonfatalWarning) {
            LOG(ERROR) << "Failed inner chain validation, error: " << errorTxt;
            throw CBException("Failed inner chain validation.");
          } else {
            LOG(INFO) << "Warning during inner chain validation: " << errorTxt;
          }
        },
        this, inputType, consumables);

    return inputType;
  }

  CBExposedTypesInfo exposedVariables() { return _chainValidation.exposedInfo; }
};

struct Repeat : public BlocksUser {
  std::string _ctxVar;
  CBVar *_ctxTimes = nullptr;
  int64_t _times = 0;
  bool _forever = false;
  ExposedInfo _consumedInfo{};

  void cleanup() {
    BlocksUser::cleanup();
    _ctxTimes = nullptr;
  }

  static inline ParamsInfo repeatParamsInfo = ParamsInfo(
      ParamsInfo::Param("Action", "The blocks to repeat.",
                        CBTypesInfo(CoreInfo::blocksInfo)),
      ParamsInfo::Param("Times", "How many times we should repeat the action.",
                        CBTypesInfo(CoreInfo::intVarInfo)),
      ParamsInfo::Param("Forever", "If we should repeat the action forever.",
                        CBTypesInfo(CoreInfo::boolInfo)));

  static CBTypesInfo inputTypes() { return CBTypesInfo(CoreInfo::anyInfo); }

  static CBTypesInfo outputTypes() { return CBTypesInfo(CoreInfo::anyInfo); }

  static CBParametersInfo parameters() {
    return CBParametersInfo(repeatParamsInfo);
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      cloneVar(_blocks, value);
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
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _blocks;
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
    default:
      break;
    }
    throw CBException("Parameter out of range.");
  }

  CBExposedTypesInfo consumedVariables() {
    if (_ctxVar.size() == 0) {
      return nullptr;
    } else {
      _consumedInfo = ExposedInfo(ExposedInfo::Variable(
          _ctxVar.c_str(), "The Int number of repeats variable.",
          CBTypeInfo(CoreInfo::intInfo)));
      return CBExposedTypesInfo(_consumedInfo);
    }
  }

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    auto repeats = _forever ? 1 : _times;

    if (_ctxVar.size()) {
      if (!_ctxTimes)
        _ctxTimes = contextVariable(context, _ctxVar.c_str());
      repeats = _ctxTimes->payload.intValue;
    }

    while (repeats) {
      CBVar repeatOutput{};
      repeatOutput.valueType = None;
      repeatOutput.payload.chainState = CBChainState::Continue;
      auto state = activateBlocks(_blocks.payload.seqValue, context, input,
                                  repeatOutput);
      if (unlikely(state == FlowState::Stopping)) {
        return StopChain;
      } else if (unlikely(state == FlowState::Returning)) {
        break;
      }

      if (!_forever)
        repeats--;
    }
    return input;
  }
};

RUNTIME_CORE_BLOCK_TYPE(Const);
RUNTIME_CORE_BLOCK_TYPE(Input);
RUNTIME_CORE_BLOCK_TYPE(Sleep);
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
RUNTIME_CORE_BLOCK_TYPE(Limit);
RUNTIME_CORE_BLOCK_TYPE(Push);
RUNTIME_CORE_BLOCK_TYPE(Pop);
RUNTIME_CORE_BLOCK_TYPE(Clear);
RUNTIME_CORE_BLOCK_TYPE(Count);
RUNTIME_CORE_BLOCK_TYPE(Repeat);
}; // namespace chainblocks