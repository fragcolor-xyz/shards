/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#include "shared.hpp"
#include <boost/beast/core/detail/base64.hpp>
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

      auto pixsize = 1;
      if ((input.payload.imageValue.flags & CBIMAGE_FLAGS_16BITS_INT) ==
          CBIMAGE_FLAGS_16BITS_INT)
        pixsize = 2;
      else if ((input.payload.imageValue.flags & CBIMAGE_FLAGS_32BITS_FLOAT) ==
               CBIMAGE_FLAGS_32BITS_FLOAT)
        pixsize = 4;

      const int w = int(input.payload.imageValue.width);
      const int h = int(input.payload.imageValue.height);
      const int c = int(input.payload.imageValue.channels);
      const int flatsize = w * h * c * pixsize;

      output.resize(flatsize, Var(0.0));

      if (pixsize == 1) {
        for (int i = 0; i < flatsize; i++) {
          const auto fval = double(input.payload.imageValue.data[i]) / 255.0;
          output[i].payload.floatValue = fval;
        }
      } else if (pixsize == 2) {
        const auto u16 =
            reinterpret_cast<uint16_t *>(input.payload.imageValue.data);
        for (int i = 0; i < flatsize; i++) {
          const auto fval = double(u16[i]) / 65535.0;
          output[i].payload.floatValue = fval;
        }
      } else if (pixsize == 4) {
        const auto f32 =
            reinterpret_cast<float *>(input.payload.imageValue.data);
        for (int i = 0; i < flatsize; i++) {
          output[i].payload.floatValue = f32[i];
        }
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

  void setParam(int index, const CBVar &value) {
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

TO_SOMETHING_SIMPLE(Int, int64_t, intValue, stoll, IntType);
TO_SOMETHING(Int, 2, int64_t, int2Value, stoll, Int2Type);
TO_SOMETHING(Int, 3, int32_t, int3Value, stol, Int3Type);
TO_SOMETHING(Int, 4, int32_t, int4Value, stol, Int4Type);
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
  static inline Types toHexTypes{CoreInfo::IntType, CoreInfo::BytesType};
  static CBTypesInfo inputTypes() { return toHexTypes; }
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

template <CBType ET> struct ExpectX {
  static inline Type outputType{{ET}};
  CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  CBTypesInfo outputTypes() { return outputType; }
  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(input.valueType != ET)) {
      LOG(ERROR) << "Unexpected value: " << input;
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

template <Type &ET> struct ExpectXComplex {
  CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  CBTypesInfo outputTypes() { return ET; }
  CBVar activate(CBContext *context, const CBVar &input) {
    // TODO make this check stricter?
    const static CBTypeInfo info = ET;
    if (unlikely(input.valueType != info.basicType)) {
      LOG(ERROR) << "Unexpected value: " << input;
      throw ActivationError("Expected type " + type2Name(info.basicType) +
                            " got instead " + type2Name(input.valueType));
    }
    return input;
  }
};

struct ToBase64 {
  std::string output;
  static inline Types _inputTypes{{CoreInfo::BytesType, CoreInfo::StringType}};
  static CBTypesInfo inputTypes() { return _inputTypes; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }
  CBVar activate(CBContext *context, const CBVar &input) {
    output.clear();
    if (input.valueType == Bytes) {
      auto req =
          boost::beast::detail::base64::encoded_size(input.payload.bytesSize);
      output.resize(req);
      auto written = boost::beast::detail::base64::encode(
          output.data(), input.payload.bytesValue, input.payload.bytesSize);
      output.resize(written);
    } else {
      const auto len = input.payload.stringLen > 0
                           ? input.payload.stringLen
                           : strlen(input.payload.stringValue);
      auto req = boost::beast::detail::base64::encoded_size(len);
      output.resize(req);
      auto written = boost::beast::detail::base64::encode(
          output.data(), input.payload.stringValue, len);
      output.resize(written);
    }
    return Var(output);
  }
};

struct FromBase64 {
  std::vector<uint8_t> output;
  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BytesType; }
  CBVar activate(CBContext *context, const CBVar &input) {
    output.clear();
    auto len = input.payload.stringLen > 0 ? input.payload.stringLen
                                           : strlen(input.payload.stringValue);
    auto req = boost::beast::detail::base64::decoded_size(len);
    output.resize(req);
    auto [written, _] = boost::beast::detail::base64::decode(
        output.data(), input.payload.stringValue, len);
    output.resize(written);
    return Var(output.data(), output.size());
  }
};

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

  using ImageToFloats = ToSeq<CBType::Image, CBType::Float>;
  using FloatsToImage = ToImage<CBType::Float>;
  REGISTER_CBLOCK("ImageToFloats", ImageToFloats);
  REGISTER_CBLOCK("FloatsToImage", FloatsToImage);

  using ExpectFloatSeq = ExpectXComplex<CoreInfo::FloatSeqType>;
  REGISTER_CBLOCK("ExpectFloatSeq", ExpectFloatSeq);

  REGISTER_CBLOCK("ToBase64", ToBase64);
  REGISTER_CBLOCK("FromBase64", FromBase64);
}
}; // namespace chainblocks
