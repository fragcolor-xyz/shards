/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#include "shared.hpp"
#include <type_traits>

namespace chainblocks {
struct FromImage {
  // TODO SIMD this
  template <CBType OF>
  void toSeq(std::vector<Var> &output, const CBVar &input) {
    if constexpr (OF == CBType::Float) {
      // assume we want 0-1 normalized values
      if (input.valueType != CBType::Image)
        throw ActivationError("Expected Image type.");

      const int w = int(input.payload.imageValue.width);
      const int h = int(input.payload.imageValue.height);
      const int c = int(input.payload.imageValue.channels);
      const int flatsize = w * h * c;

      output.resize(flatsize, Var(0.0));
      for (int i = 0; i < flatsize; i++) {
        const auto fval = double(input.payload.imageValue.data[i]) / 255.0;
        output[i].payload.floatValue = fval;
      }
    } else {
      throw ActivationError("Conversion pair not implemented yet.");
    }
  }
};

struct FromSeq {
  template <CBType OF>
  void toImage(std::vector<uint8_t> &buffer, int w, int h, int c,
               const CBVar &input) {
    // TODO SIMD this
    if (input.payload.seqValue.len == 0)
      throw ActivationError("Input sequence was empty.");

    const int flatsize = std::min(w * h * c, int(input.payload.seqValue.len));
    buffer.resize(flatsize);

    if constexpr (OF == CBType::Float) {
      // assuming it's scaled 0-1
      for (int i = 0; i < flatsize; i++) {
        buffer[i] = uint8_t(
            input.payload.seqValue.elements[i].payload.floatValue * 255.0);
      }
    } else {
      throw ActivationError("Conversion pair not implemented yet.");
    }
  }
};

template <CBType CBTYPE, CBType CBOTHER> struct ToSeq {
  static inline Type _outputElemType{{CBOTHER}};
  static inline Type _outputType{{CBType::Seq, {.seqTypes = _outputElemType}}};

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return _outputType; }

  std::vector<Var> _output;

  CBVar activate(CBContext *context, const CBVar &input) {
    // do not .clear here, for speed, let From manage that
    if constexpr (CBTYPE == CBType::Image) {
      FromImage c;
      c.toSeq<CBOTHER>(_output, input);
    } else {
      throw ActivationError("Conversion pair not implemented yet.");
    }
    return Var(_output);
  }
};

template <CBType FROMTYPE> struct ToImage {
  static inline Type _inputElemType{{FROMTYPE}};
  static inline Type _inputType{{CBType::Seq, {.seqTypes = _inputElemType}}};

  static CBTypesInfo inputTypes() { return _inputType; }
  static CBTypesInfo outputTypes() { return CoreInfo::ImageType; }

  static inline Parameters _params{
      {"Width", "The width of the output image.", {CoreInfo::IntType}},
      {"Height", "The height of the output image.", {CoreInfo::IntType}},
      {"Channels", "The channels of the output image.", {CoreInfo::IntType}}};

  static CBParametersInfo parameters() { return _params; }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(int(_width));
    case 1:
      return Var(int(_height));
    case 2:
      return Var(int(_channels));
    default:
      return {};
    }
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _width = uint16_t(value.payload.intValue);
      break;
    case 1:
      _height = uint16_t(value.payload.intValue);
      break;
    case 2:
      _channels = uint8_t(
          std::min(CBInt(4), std::max(CBInt(1), value.payload.intValue)));
      break;
    }
  }

  void warmup(CBContext *_) {}

  CBVar activate(CBContext *context, const CBVar &input) {
    FromSeq c;
    c.toImage<FROMTYPE>(_buffer, int(_width), int(_height), int(_channels),
                        input);
    return Var(&_buffer.front(), _width, _height, _channels, 0);
  }

private:
  uint8_t _channels = 1;
  uint16_t _width = 16;
  uint16_t _height = 16;
  std::vector<uint8_t> _buffer;
};

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
        throw ActivationError("Cannot cast given input type.");                \
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
        throw ActivationError("Cannot cast given input type.");                \
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
        throw ActivationError("Cannot cast given input type.");                \
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
        throw ActivationError("Cannot cast given input type.");                \
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
      throw ActivationError("BytesToX, Not enough bytes to convert to " +
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
      throw ActivationError("Expected type " + type2Name(ET) + " got instead " +
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
EXPECT_BLOCK(Table, Table);
EXPECT_BLOCK(Chain, CBType::Chain);

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
    case CBType::Array:
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
    case CBType::Array: {
      _buffer.resize(input.payload.arrayLen * sizeof(CBVarPayload));
      memcpy(&_buffer.front(), input.payload.arrayValue,
             input.payload.arrayLen * sizeof(CBVarPayload));
      break;
    }
    case CBType::Path:
    case CBType::String:
    case CBType::ContextVar: {
      auto len = input.payload.stringLen > 0
                     ? input.payload.stringLen
                     : strlen(input.payload.stringValue);
      // adding on purpose a 0 terminator
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
          throw ActivationError("ToBytes, unsupported Seq type, use Flatten to "
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
      throw ActivationError("ToBytes, unsupported type, likely TODO.");
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
  REGISTER_CORE_BLOCK(ExpectChain);
  REGISTER_CORE_BLOCK(ExpectTable);
  REGISTER_CORE_BLOCK(ToBytes);
  REGISTER_CBLOCK("BytesToString!!", BytesToStringUnsafe);
  using ImageToFloats = ToSeq<CBType::Image, CBType::Float>;
  using FloatsToImage = ToImage<CBType::Float>;
  REGISTER_CBLOCK("ImageToFloats", ImageToFloats);
  REGISTER_CBLOCK("FloatsToImage", FloatsToImage);
}
}; // namespace chainblocks
