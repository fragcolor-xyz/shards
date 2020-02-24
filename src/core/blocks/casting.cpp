/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#include "shared.hpp"

namespace chainblocks {
// TODO Write proper inputTypes Info
#define TO_SOMETHING(_varName_, _width_, _type_, _payload_, _strOp_, _info_)   \
  struct To##_varName_##_width_ {                                              \
    static inline Type &singleOutput = CoreInfo::_info_;                       \
    CBTypesInfo inputTypes() { return CoreInfo::AnyType; }                     \
    CBTypesInfo outputTypes() { return singleOutput; }                         \
                                                                               \
    CBTypeInfo compose(const CBInstanceData &data) { return singleOutput; }    \
                                                                               \
    bool convert(CBVar &dst, int &index, const CBVar &src) {                   \
      switch (src.valueType) {                                                 \
      case String: {                                                           \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(std::_strOp_(src.payload.stringValue));        \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        break;                                                                 \
      }                                                                        \
      case Float:                                                              \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.floatValue);                       \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        break;                                                                 \
      case Float2:                                                             \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.float2Value[0]);                   \
        index++;                                                               \
        if (index == (_width_))                                                \
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
        for (uint32_t i = 0; i < _width_ && i < input.payload.seqValue.len;    \
             i++) {                                                            \
          if (convert(output, index, input.payload.seqValue.elements[i]))      \
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
  RUNTIME_BLOCK_compose(To##_varName_##_width_);                               \
  RUNTIME_BLOCK_activate(To##_varName_##_width_);                              \
  RUNTIME_BLOCK_END(To##_varName_##_width_);

// TODO improve this
#define TO_SOMETHING_SIMPLE(_varName_, _type_, _payload_, _strOp_, _info_)     \
  struct To##_varName_ {                                                       \
    static inline Type &singleOutput = CoreInfo::_info_;                       \
    CBTypesInfo inputTypes() { return CoreInfo::AnyType; }                     \
    CBTypesInfo outputTypes() { return singleOutput; }                         \
                                                                               \
    CBTypeInfo compose(const CBInstanceData &data) { return singleOutput; }    \
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
        for (uint32_t i = 0; i < 1 && i < input.payload.seqValue.len; i++) {   \
          if (convert(output, input.payload.seqValue.elements[i]))             \
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
  RUNTIME_BLOCK_compose(To##_varName_);                                        \
  RUNTIME_BLOCK_activate(To##_varName_);                                       \
  RUNTIME_BLOCK_END(To##_varName_);

TO_SOMETHING_SIMPLE(Int, int64_t, intValue, stoi, IntType);
TO_SOMETHING(Int, 2, int64_t, int2Value, stoi, Int2Type);
TO_SOMETHING(Int, 3, int32_t, int3Value, stoi, Int3Type);
TO_SOMETHING(Int, 4, int32_t, int4Value, stoi, Int4Type);
TO_SOMETHING_SIMPLE(Float, double, floatValue, stod, FloatType);
TO_SOMETHING(Float, 2, double, float2Value, stod, Float2Type);
TO_SOMETHING(Float, 3, float, float3Value, stof, Float3Type);
TO_SOMETHING(Float, 4, float, float4Value, stof, Float4Type);

struct ToString {
  VarStringStream stream;
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }
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
  static CBTypesInfo inputTypes() { return CoreInfo::IntType; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }
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
  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return CoreInfo::IntType; }
  CBVar activate(CBContext *context, const CBVar &input) {
    auto v = referenceVariable(context, input.payload.stringValue);
    auto res = Var(reinterpret_cast<int64_t>(v));
    releaseVariable(v);
    return res;
  }
};

RUNTIME_CORE_BLOCK(VarAddr);
RUNTIME_BLOCK_inputTypes(VarAddr);
RUNTIME_BLOCK_outputTypes(VarAddr);
RUNTIME_BLOCK_activate(VarAddr);
RUNTIME_BLOCK_END(VarAddr);

struct BitSwap32 {
  static CBTypesInfo inputTypes() { return CoreInfo::IntType; }
  static CBTypesInfo outputTypes() { return CoreInfo::IntType; }
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
  static CBTypesInfo inputTypes() { return CoreInfo::IntType; }
  static CBTypesInfo outputTypes() { return CoreInfo::IntType; }
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

// actual type = AT = the c type
template <Type(&OT), typename AT> struct BytesToX {
  CBTypesInfo inputTypes() { return CoreInfo::BytesType; }
  CBTypesInfo outputTypes() { return OT; }

  CBSeq _outputCache = {};

  void destroy() {
    if (_outputCache.elements) {
      chainblocks::arrayFree(_outputCache);
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
    } else if constexpr (std::is_same<AT, char *>::value) {
      dst.valueType = String;
      dst.payload.stringValue = (char *)addr;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    const auto tsize = sizeof(AT);
    if (unlikely(input.payload.bytesSize < tsize)) {
      throw CBException("BytesToX, Not enough bytes to convert to " +
                        type2Name(CBTypeInfo(OT).basicType));
    } else if constexpr (std::is_same<AT, char *>::value) {
      CBVar output{};
      convert(output, input.payload.bytesValue);
      return output;
    } else if (input.payload.bytesSize == tsize) {
      // exact size, 1 value
      chainblocks::arrayResize(_outputCache, 1);
      convert(_outputCache.elements[0], input.payload.bytesValue);
    } else {
      // many values
      auto len = input.payload.bytesSize / tsize; // int division, fine
      chainblocks::arrayResize(_outputCache, len);
      for (size_t i = 0; i < len; i++) {
        convert(_outputCache.elements[i],
                input.payload.bytesValue + (sizeof(AT) * i));
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

BYTES_TO_BLOCK(Float32, CoreInfo::FloatSeqType, float);
BYTES_TO_BLOCK(Float64, CoreInfo::FloatSeqType, double);
BYTES_TO_BLOCK(Int8, CoreInfo::IntSeqType, int8_t);
BYTES_TO_BLOCK(Int16, CoreInfo::IntSeqType, int16_t);
BYTES_TO_BLOCK(Int32, CoreInfo::IntSeqType, int32_t);
BYTES_TO_BLOCK(Int64, CoreInfo::IntSeqType, int64_t);
BYTES_TO_BLOCK(String, CoreInfo::StringType, char *);

struct BytesToStringUnsafe {
  CBTypesInfo inputTypes() { return CoreInfo::BytesType; }
  CBTypesInfo outputTypes() { return CoreInfo::StringType; }
  CBVar activate(CBContext *context, const CBVar &input) {
    const char *str = (const char *)input.payload.bytesValue;
    return Var(str);
  }
};

template <CBType ET> struct ExpectX {
  static inline Type outputType{{ET}};
  CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  CBTypesInfo outputTypes() { return outputType; }
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
EXPECT_BLOCK(Bool, Bool);
EXPECT_BLOCK(Int, Int);
EXPECT_BLOCK(Int2, Int2);
EXPECT_BLOCK(Int3, Int3);
EXPECT_BLOCK(Int4, Int4);
EXPECT_BLOCK(Bytes, Bytes);
EXPECT_BLOCK(String, String);
EXPECT_BLOCK(Image, Image);
EXPECT_BLOCK(Seq, Seq);

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

  CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  CBTypesInfo outputTypes() { return CoreInfo::BytesType; }

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
    case CBType::Path:
    case CBType::ContextVar:
    case CBType::StackIndex:
    case CBType::Image:
    case CBType::Chain:
    case CBType::Block:
    case CBType::Table:
    case CBType::Seq:
    case CBType::Bytes: {
      return 0; // unsupported for now
    }
    }
    return 0;
  }

  void convert(const CBVar &input) {
    switch (input.valueType) {
    case CBType::EndOfBlittableTypes:
    case CBType::None:
    case CBType::Object:
    case CBType::StackIndex:
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
    case CBType::Path:
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
      auto len = input.payload.seqValue.len;
      if (len > 0) {
        auto itemSize = getSize(input.payload.seqValue.elements[0]);
        if (itemSize == 0) {
          throw CBException("ToBytes, unsupported Seq type, use Flatten to "
                            "make a flat sequence.");
        }
        _buffer.resize(itemSize * len);
        for (uint32_t i = 0; i < len; i++) {
          // we cheat using any pointer in the union
          // since only blittables are allowed
          memcpy((&_buffer.front()) + (itemSize * i),
                 &input.payload.seqValue.elements[i].payload.intValue,
                 itemSize);
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
  REGISTER_CORE_BLOCK(BytesToFloat32);
  REGISTER_CORE_BLOCK(BytesToFloat64);
  REGISTER_CORE_BLOCK(BytesToInt8);
  REGISTER_CORE_BLOCK(BytesToInt16);
  REGISTER_CORE_BLOCK(BytesToInt32);
  REGISTER_CORE_BLOCK(BytesToInt64);
  REGISTER_CORE_BLOCK(BytesToString);
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
  REGISTER_CORE_BLOCK(ExpectImage);
  REGISTER_CORE_BLOCK(ExpectBool);
  REGISTER_CORE_BLOCK(ExpectSeq);
  REGISTER_CORE_BLOCK(ToBytes);
  REGISTER_CBLOCK("BytesToString!!", BytesToStringUnsafe);
}
}; // namespace chainblocks
