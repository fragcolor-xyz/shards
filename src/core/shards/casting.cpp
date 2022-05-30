/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "shardwrapper.hpp"
#include "shards.h"
#include "number_types.hpp"
#include "shared.hpp"
#include <boost/beast/core/detail/base64.hpp>
#include <type_traits>

namespace shards {
struct FromImage {
  // TODO SIMD this
  template <SHType OF> void toSeq(std::vector<Var> &output, const SHVar &input) {
    if constexpr (OF == SHType::Float) {
      // assume we want 0-1 normalized values
      if (input.valueType != SHType::Image)
        throw ActivationError("Expected Image type.");

      auto pixsize = 1;
      if ((input.payload.imageValue.flags & SHIMAGE_FLAGS_16BITS_INT) == SHIMAGE_FLAGS_16BITS_INT)
        pixsize = 2;
      else if ((input.payload.imageValue.flags & SHIMAGE_FLAGS_32BITS_FLOAT) == SHIMAGE_FLAGS_32BITS_FLOAT)
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
        const auto u16 = reinterpret_cast<uint16_t *>(input.payload.imageValue.data);
        for (int i = 0; i < flatsize; i++) {
          const auto fval = double(u16[i]) / 65535.0;
          output[i].payload.floatValue = fval;
        }
      } else if (pixsize == 4) {
        const auto f32 = reinterpret_cast<float *>(input.payload.imageValue.data);
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
  template <SHType OF> void toImage(std::vector<uint8_t> &buffer, int w, int h, int c, const SHVar &input) {
    // TODO SIMD this
    if (input.payload.seqValue.len == 0)
      throw ActivationError("Input sequence was empty.");

    const int flatsize = std::min(w * h * c, int(input.payload.seqValue.len));
    buffer.resize(flatsize);

    if constexpr (OF == SHType::Float) {
      // assuming it's scaled 0-1
      for (int i = 0; i < flatsize; i++) {
        buffer[i] = uint8_t(input.payload.seqValue.elements[i].payload.floatValue * 255.0);
      }
    } else {
      throw ActivationError("Conversion pair not implemented yet.");
    }
  }

  template <SHType OF> void toBytes(std::vector<uint8_t> &buffer, const SHVar &input) {
    // TODO SIMD this
    if (input.payload.seqValue.len == 0)
      throw ActivationError("Input sequence was empty.");

    buffer.resize(size_t(input.payload.seqValue.len));

    if constexpr (OF == SHType::Int) {
      for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {
        const auto val = input.payload.seqValue.elements[i].payload.intValue;
        if (val > UINT8_MAX || val < 0)
          throw ActivationError("Value out of byte range (0~255)");

        buffer[i] = uint8_t(val);
      }
    } else {
      throw ActivationError("Conversion pair not implemented yet.");
    }
  }
};

struct FromBytes {
  template <SHType OF> void toSeq(std::vector<Var> &output, const SHVar &input) {
    if constexpr (OF == SHType::Int) {
      // assume we want 0-1 normalized values
      if (input.valueType != SHType::Bytes)
        throw ActivationError("Expected Bytes type.");

      output.resize(input.payload.bytesSize);

      for (uint32_t i = 0; i < input.payload.bytesSize; i++) {
        const auto b = int64_t(input.payload.bytesValue[i]);
        output[i] = Var(b);
      }
    } else {
      throw ActivationError("Conversion pair not implemented yet.");
    }
  }
};

template <SHType SHTYPE, SHType SHOTHER> struct ToSeq {
  static inline Type _inputType{{SHTYPE}};
  static inline Type _outputElemType{{SHOTHER}};
  static inline Type _outputType{{SHType::Seq, {.seqTypes = _outputElemType}}};

  static SHTypesInfo inputTypes() { return _inputType; }
  static SHTypesInfo outputTypes() { return _outputType; }

  std::vector<Var> _output;

  SHVar activate(SHContext *context, const SHVar &input) {
    // do not .clear here, for speed, let From manage that
    if constexpr (SHTYPE == SHType::Image) {
      FromImage c;
      c.toSeq<SHOTHER>(_output, input);
    } else if constexpr (SHTYPE == SHType::Bytes) {
      FromBytes c;
      c.toSeq<SHOTHER>(_output, input);
    } else {
      throw ActivationError("Conversion pair not implemented yet.");
    }
    return Var(_output);
  }
};

template <SHType SHTYPE> struct ToString1 {
  static inline Type _inputType{{SHTYPE}};

  static SHTypesInfo inputTypes() { return _inputType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  std::string _output;

  SHVar activate(SHContext *context, const SHVar &input) {
    // do not .clear here, for speed, let From manage that
    if constexpr (SHTYPE == SHType::Bytes) {
      _output.clear();
      _output.assign((const char *)input.payload.bytesValue, size_t(input.payload.bytesSize));
    } else {
      throw ActivationError("Conversion pair not implemented yet.");
    }
    return Var(_output);
  }
};

template <SHType FROMTYPE> struct ToImage {
  static inline Type _inputElemType{{FROMTYPE}};
  static inline Type _inputType{{SHType::Seq, {.seqTypes = _inputElemType}}};

  static SHTypesInfo inputTypes() { return _inputType; }
  static SHTypesInfo outputTypes() { return CoreInfo::ImageType; }

  static inline Parameters _params{{"Width", SHCCSTR("The width of the output image."), {CoreInfo::IntType}},
                                   {"Height", SHCCSTR("The height of the output image."), {CoreInfo::IntType}},
                                   {"Channels", SHCCSTR("The channels of the output image."), {CoreInfo::IntType}}};

  static SHParametersInfo parameters() { return _params; }

  SHVar getParam(int index) {
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

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _width = uint16_t(value.payload.intValue);
      break;
    case 1:
      _height = uint16_t(value.payload.intValue);
      break;
    case 2:
      _channels = uint8_t(std::min(SHInt(4), std::max(SHInt(1), value.payload.intValue)));
      break;
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    FromSeq c;
    c.toImage<FROMTYPE>(_buffer, int(_width), int(_height), int(_channels), input);
    return Var(&_buffer.front(), _width, _height, _channels, 0);
  }

private:
  uint8_t _channels = 1;
  uint16_t _width = 16;
  uint16_t _height = 16;
  std::vector<uint8_t> _buffer;
};

template <SHType FROMTYPE> struct ToBytes {
  static inline Type _inputElemType{{FROMTYPE}};
  static inline Type _inputType{{SHType::Seq, {.seqTypes = _inputElemType}}};

  static SHTypesInfo inputTypes() { return _inputType; }
  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    FromSeq c;
    c.toBytes<FROMTYPE>(_buffer, input);
    return Var(_buffer);
  }

private:
  std::vector<uint8_t> _buffer;
};

template <SHType ToType> struct ToNumber {
  const VectorTypeTraits *_outputVectorType{nullptr};
  const NumberTypeTraits *_outputNumberType{nullptr};

  // If known at compose time these are set
  // otherwise each element should be evaluated during activate
  const VectorTypeTraits *_inputVectorType{nullptr};
  const NumberConversion *_numberConversion{nullptr};

  SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  SHTypesInfo outputTypes() {
    if (!_outputVectorType)
      return CoreInfo::AnyType;
    return _outputVectorType->type;
  }

  static const NumberTypeTraits *getEnumNumberType() {
    static const NumberTypeTraits *result = nullptr;
    if (!result) {
      result = NumberTypeLookup::getInstance().get(NumberType::Int32);
      shassert(result);
    }
    return result;
  }

  const NumberTypeTraits *determineFixedSeqNumberType(const SHTypeInfo &typeInfo) {
    if (!typeInfo.fixedSize) {
      return nullptr;
    }

    const NumberTypeTraits *fixedElemNumberType = nullptr;
    for (size_t i = 0; i < typeInfo.seqTypes.len; i++) {
      const SHTypeInfo &elementType = typeInfo.seqTypes.elements[i];
      const NumberTypeTraits *currentElemNumberType = NumberTypeLookup::getInstance().get(elementType.basicType);
      if (!currentElemNumberType)
        return nullptr;
      if (i == 0) {
        fixedElemNumberType = currentElemNumberType;
      } else {
        if (currentElemNumberType != fixedElemNumberType) {
          return nullptr;
        }
      }
    }

    return fixedElemNumberType;
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    _outputVectorType = VectorTypeLookup::getInstance().get(ToType);
    if (!_outputVectorType) {
      throw ComposeError("Conversion not implemented for this type");
    }

    _outputNumberType = NumberTypeLookup::getInstance().get(_outputVectorType->numberType);
    _inputVectorType = VectorTypeLookup::getInstance().get(data.inputType.basicType);

    if (_inputVectorType) {
      _numberConversion =
          NumberTypeLookup::getInstance().getConversion(_inputVectorType->numberType, _outputVectorType->numberType);
      shassert(_numberConversion);
    } else if (data.inputType.basicType == Seq) {
      const NumberTypeTraits *fixedNumberType = determineFixedSeqNumberType(data.inputType);
      if (fixedNumberType) {
        _numberConversion = fixedNumberType->conversionTable.get(_outputNumberType->type);
        shassert(_numberConversion);

        OVERRIDE_ACTIVATE(data, activateSeqElementsFixed);
      }
    }

    return _outputVectorType->type;
  }

  void parseVector(SHVar &output, const SHVar &input) {
    const VectorTypeTraits *inputVectorType =
        _inputVectorType ? _inputVectorType : VectorTypeLookup::getInstance().get(input.valueType);
    if (!inputVectorType) {
      throw ActivationError("Cannot cast given input type.");
    }

    const NumberConversion *conversion =
        _numberConversion
            ? _numberConversion
            : NumberTypeLookup::getInstance().getConversion(inputVectorType->numberType, _outputVectorType->numberType);
    shassert(conversion);

    // Can reduce call overhead by adding a single ConvertMultiple function with
    // in/out lengths
    uint8_t *srcPtr = (uint8_t *)&input.payload;
    uint8_t *dstPtr = (uint8_t *)&output.payload;
    size_t numToConvert = std::min(_outputVectorType->dimension, inputVectorType->dimension);
    for (size_t i = 0; i < numToConvert; i++) {
      conversion->convertOne(srcPtr, dstPtr);
      srcPtr += conversion->inStride;
      dstPtr += conversion->outStride;
    }
  }

  void parseSeqElements(SHVar &output, const SHSeq &sequence) {
    uint8_t *dstPtr = (uint8_t *)&output.payload;
    for (size_t i = 0; i < sequence.len; i++) {
      const SHVar &elem = sequence.elements[i];

      const NumberTypeTraits *elemNumberType = NumberTypeLookup::getInstance().get(elem.valueType);
      if (!elemNumberType) {
        throw ActivationError("Cannot cast given sequence element type.");
      }

      const NumberConversion *conversion = elemNumberType->conversionTable.get(_outputVectorType->numberType);
      shassert(conversion);

      conversion->convertOne(&elem.payload, dstPtr);
      dstPtr += conversion->outStride;
    }
  }

  void skipStringSeparators(const char *&strPtr) {
    while (*strPtr && (std::isspace(strPtr[0]) || strPtr[0] == ',' || strPtr[0] == ';')) {
      ++strPtr;
    }
  }

  void parseStringElements(SHVar &output, const char *str, size_t length) {
    uint8_t *dstPtr = (uint8_t *)&output.payload;
    const char *strPtr = str;

    // Skip seq header
    while (*strPtr && (std::isspace(strPtr[0]) || strPtr[0] == '(' || strPtr[0] == '{' || strPtr[0] == '[')) {
      ++strPtr;
    }

    for (size_t i = 0; i < _outputVectorType->dimension; i++) {
      _outputNumberType->convertParse(dstPtr, strPtr, (char **)&strPtr);
      skipStringSeparators(strPtr);
      dstPtr += _outputNumberType->size;
    }
  }

  void parseEnumValue(SHVar &output, SHEnum value) {
    _numberConversion = getEnumNumberType()->conversionTable.get(_outputVectorType->numberType);
    shassert(_numberConversion);

    _numberConversion->convertOne(&value, &output.payload);
  }

  SHVar activateSeqElementsFixed(SHContext *context, const SHVar &input) {
    shassert(_outputVectorType);
    shassert(_numberConversion);

    const SHSeq &sequence = input.payload.seqValue;
    SHVar output{};
    output.valueType = _outputVectorType->shType;

    uint8_t *dstPtr = (uint8_t *)&output.payload;
    for (size_t i = 0; i < sequence.len; i++) {
      const SHVar &elem = sequence.elements[i];

      _numberConversion->convertOne(&elem.payload, dstPtr);
      dstPtr += _numberConversion->outStride;
    }

    return output;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    shassert(_outputVectorType);

    SHVar output{};
    output.valueType = _outputVectorType->shType;

    switch (input.valueType) {
    case Seq:
      parseSeqElements(output, input.payload.seqValue);
      break;
    case Enum:
      parseEnumValue(output, input.payload.enumValue);
      break;
    case String:
      parseStringElements(output, input.payload.stringValue, input.payload.stringLen);
      break;
    default:
      parseVector(output, input);
      break;
    }

    return output;
  }
};

struct ToString {
  VarStringStream stream;
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }
  SHVar activate(SHContext *context, const SHVar &input) {
    stream.write(input);
    return Var(stream.str());
  }
};

RUNTIME_CORE_SHARD(ToString);
RUNTIME_SHARD_inputTypes(ToString);
RUNTIME_SHARD_outputTypes(ToString);
RUNTIME_SHARD_activate(ToString);
RUNTIME_SHARD_END(ToString);

struct ToHex {
  VarStringStream stream;
  static inline Types toHexTypes{CoreInfo::IntType, CoreInfo::BytesType, CoreInfo::StringType};
  static SHTypesInfo inputTypes() { return toHexTypes; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }
  SHVar activate(SHContext *context, const SHVar &input) {
    stream.tryWriteHex(input);
    return Var(stream.str());
  }
};

RUNTIME_CORE_SHARD(ToHex);
RUNTIME_SHARD_inputTypes(ToHex);
RUNTIME_SHARD_outputTypes(ToHex);
RUNTIME_SHARD_activate(ToHex);
RUNTIME_SHARD_END(ToHex);

struct VarAddr {
  VarStringStream stream;
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::IntType; }
  SHVar activate(SHContext *context, const SHVar &input) {
    auto v = referenceVariable(context, input.payload.stringValue);
    auto res = Var(reinterpret_cast<int64_t>(v));
    releaseVariable(v);
    return res;
  }
};

RUNTIME_CORE_SHARD(VarAddr);
RUNTIME_SHARD_inputTypes(VarAddr);
RUNTIME_SHARD_outputTypes(VarAddr);
RUNTIME_SHARD_activate(VarAddr);
RUNTIME_SHARD_END(VarAddr);

struct BitSwap32 {
  static SHTypesInfo inputTypes() { return CoreInfo::IntType; }
  static SHTypesInfo outputTypes() { return CoreInfo::IntType; }
  SHVar activate(SHContext *context, const SHVar &input) {
    auto i32 = static_cast<uint32_t>(input.payload.intValue);
    i32 = __builtin_bswap32(i32);
    return Var(static_cast<int64_t>(i32));
  }
};

RUNTIME_CORE_SHARD(BitSwap32);
RUNTIME_SHARD_inputTypes(BitSwap32);
RUNTIME_SHARD_outputTypes(BitSwap32);
RUNTIME_SHARD_activate(BitSwap32);
RUNTIME_SHARD_END(BitSwap32);

struct BitSwap64 {
  static SHTypesInfo inputTypes() { return CoreInfo::IntType; }
  static SHTypesInfo outputTypes() { return CoreInfo::IntType; }
  SHVar activate(SHContext *context, const SHVar &input) {
    auto i64 = static_cast<uint64_t>(input.payload.intValue);
    i64 = __builtin_bswap64(i64);
    return Var(static_cast<int64_t>(i64));
  }
};

RUNTIME_CORE_SHARD(BitSwap64);
RUNTIME_SHARD_inputTypes(BitSwap64);
RUNTIME_SHARD_outputTypes(BitSwap64);
RUNTIME_SHARD_activate(BitSwap64);
RUNTIME_SHARD_END(BitSwap64);

template <SHType ET> struct ExpectX {
  static inline Type outputType{{ET}};
  SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  SHTypesInfo outputTypes() { return outputType; }
  SHVar activate(SHContext *context, const SHVar &input) {
    if (unlikely(input.valueType != ET)) {
      SHLOG_ERROR("Unexpected value: {}", input);
      throw ActivationError("Expected type " + type2Name(ET) + " got instead " + type2Name(input.valueType));
    }
    return input;
  }
};

#define EXPECT_SHARD(_name_, _shtype_)                 \
  struct Expect##_name_ : public ExpectX<_shtype_> {}; \
  RUNTIME_CORE_SHARD(Expect##_name_);                  \
  RUNTIME_SHARD_inputTypes(Expect##_name_);            \
  RUNTIME_SHARD_outputTypes(Expect##_name_);           \
  RUNTIME_SHARD_activate(Expect##_name_);              \
  RUNTIME_SHARD_END(Expect##_name_);

EXPECT_SHARD(Float, Float);
EXPECT_SHARD(Float2, Float2);
EXPECT_SHARD(Float3, Float3);
EXPECT_SHARD(Float4, Float4);
EXPECT_SHARD(Bool, Bool);
EXPECT_SHARD(Int, Int);
EXPECT_SHARD(Int2, Int2);
EXPECT_SHARD(Int3, Int3);
EXPECT_SHARD(Int4, Int4);
EXPECT_SHARD(Bytes, Bytes);
EXPECT_SHARD(String, String);
EXPECT_SHARD(Image, Image);
EXPECT_SHARD(Seq, Seq);
EXPECT_SHARD(Table, Table);
EXPECT_SHARD(Wire, SHType::Wire);

template <Type &ET> struct ExpectXComplex {
  static inline Parameters params{{"Unsafe",
                                   SHCCSTR("If we should skip performing deep type hashing and comparison. "
                                           "(generally fast but this might improve performance)"),
                                   {CoreInfo::BoolType}}};
  static inline uint64_t ExpectedHash = deriveTypeHash(ET);

  bool _unsafe{false};

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return ET; }

  void setParam(int index, const SHVar &value) { _unsafe = value.payload.boolValue; }

  SHVar getParam(int index) { return Var(_unsafe); }

  SHVar activate(SHContext *context, const SHVar &input) {
    const static SHTypeInfo info = ET;
    if (unlikely(input.valueType != info.basicType)) {
      SHLOG_ERROR("Unexpected value: {} expected type: {}", input, info);
      throw ActivationError("Expected type " + type2Name(info.basicType) + " got instead " + type2Name(input.valueType));
    } else if (!_unsafe) {
      auto inputTypeHash = deriveTypeHash(input);
      if (unlikely(inputTypeHash != ExpectedHash)) {
        SHLOG_ERROR("Unexpected value: {} expected type: {}", input, info);
        throw ActivationError("Expected type " + type2Name(info.basicType) + " got instead " + type2Name(input.valueType));
      }
    }
    return input;
  }
};

struct ExpectLike {
  ParamVar _example{};
  SHTypeInfo _expectedType{};
  bool _dispose{false};
  uint64_t _outputTypeHash{0};
  bool _unsafe{false};
  bool _derived{false};

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static inline Parameters params{{"Example",
                                   SHCCSTR("The example value to expect, in the case of a used wire, the "
                                           "output type of that wire will be used."),
                                   {CoreInfo::AnyType}},
                                  {"Unsafe",
                                   SHCCSTR("If we should skip performing deep type hashing and comparison. "
                                           "(generally fast but this might improve performance)"),
                                   {CoreInfo::BoolType}}};
  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) {
    if (index == 0)
      _example = value;
    else if (index == 1)
      _unsafe = value.payload.boolValue;
  }

  SHVar getParam(int index) {
    if (index == 0)
      return _example;
    else
      return Var(_unsafe);
  }

  void destroy() {
    if (_expectedType.basicType != None && _dispose) {
      freeDerivedInfo(_expectedType);
      _expectedType = {};
      _dispose = false;
    }
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    if (_example.isVariable()) {
      throw ComposeError("The example value of ExpectLike cannot be a variable");
    } else {
      if (_expectedType.basicType != None && _dispose) {
        freeDerivedInfo(_expectedType);
        _expectedType = {};
        _dispose = false;
      }
      if (_example->valueType == SHType::Wire) {
        auto wire = SHWire::sharedFromRef(_example->payload.wireValue);
        _expectedType = wire->outputType;
        _outputTypeHash = deriveTypeHash(_expectedType);
        return _expectedType;
      } else {
        SHVar example = _example;
        _expectedType = deriveTypeInfo(example, data);
        _dispose = true;
        _outputTypeHash = deriveTypeHash(_expectedType);
        return _expectedType;
      }
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (unlikely(input.valueType != _expectedType.basicType)) {
      SHLOG_ERROR("Unexpected value: {} expected type: {}", input, _expectedType);
      throw ActivationError("Unexpected input type");
    } else if (!_unsafe) {
      auto inputTypeHash = deriveTypeHash(input);
      if (unlikely(inputTypeHash != _outputTypeHash)) {
        SHLOG_ERROR("Unexpected value: {} expected type: {}", input, _expectedType);
        throw ActivationError("Unexpected input type");
      }
    }
    return input;
  }
};

struct ToBase64 {
  std::string output;
  static inline Types _inputTypes{{CoreInfo::BytesType, CoreInfo::StringType}};
  static SHTypesInfo inputTypes() { return _inputTypes; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }
  SHVar activate(SHContext *context, const SHVar &input) {
    output.clear();
    if (input.valueType == Bytes) {
      auto req = boost::beast::detail::base64::encoded_size(input.payload.bytesSize);
      output.resize(req);
      auto written = boost::beast::detail::base64::encode(output.data(), input.payload.bytesValue, input.payload.bytesSize);
      output.resize(written);
    } else {
      const auto len = input.payload.stringLen > 0 || input.payload.stringValue == nullptr ? input.payload.stringLen
                                                                                           : strlen(input.payload.stringValue);
      auto req = boost::beast::detail::base64::encoded_size(len);
      output.resize(req);
      auto written = boost::beast::detail::base64::encode(output.data(), input.payload.stringValue, len);
      output.resize(written);
    }
    return Var(output);
  }
};

struct FromBase64 {
  std::vector<uint8_t> output;
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }
  SHVar activate(SHContext *context, const SHVar &input) {
    output.clear();
    auto len = input.payload.stringLen > 0 || input.payload.stringValue == nullptr ? input.payload.stringLen
                                                                                   : strlen(input.payload.stringValue);
    auto req = boost::beast::detail::base64::decoded_size(len);
    output.resize(req);
    auto [written, _] = boost::beast::detail::base64::decode(output.data(), input.payload.stringValue, len);
    output.resize(written);
    return Var(output.data(), output.size());
  }
};

struct HexToBytes {
  std::vector<uint8_t> output;
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }

  static int char2int(char input) {
    if (input >= '0' && input <= '9')
      return input - '0';
    if (input >= 'A' && input <= 'F')
      return input - 'A' + 10;
    if (input >= 'a' && input <= 'f')
      return input - 'a' + 10;
    throw std::invalid_argument("Invalid input string");
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    output.clear();
    auto src = input.payload.stringValue;
    // allow 0x prefix
    if (src[0] == '0' && (src[1] == 'x' || src[1] == 'X'))
      src += 2;
    while (*src && src[1]) {
      output.emplace_back(char2int(*src) * 16 + char2int(src[1]));
      src += 2;
    }
    return Var(output.data(), output.size());
  }
};

struct StringToBytes {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    const auto len = SHSTRLEN(input);
    return Var((uint8_t *)input.payload.stringValue, uint32_t(len));
  }
};

void registerCastingShards() {
  REGISTER_SHARD("ToInt", ToNumber<Int>);
  REGISTER_SHARD("ToInt2", ToNumber<Int2>);
  REGISTER_SHARD("ToInt3", ToNumber<Int3>);
  REGISTER_SHARD("ToInt4", ToNumber<Int4>);
  REGISTER_SHARD("ToInt8", ToNumber<Int8>);
  REGISTER_SHARD("ToInt16", ToNumber<Int16>);
  REGISTER_SHARD("ToColor", ToNumber<Color>);
  REGISTER_SHARD("ToFloat", ToNumber<Float>);
  REGISTER_SHARD("ToFloat2", ToNumber<Float2>);
  REGISTER_SHARD("ToFloat3", ToNumber<Float3>);
  REGISTER_SHARD("ToFloat4", ToNumber<Float4>);

  REGISTER_CORE_SHARD(ToString);
  REGISTER_CORE_SHARD(ToHex);

  REGISTER_CORE_SHARD(VarAddr);

  REGISTER_CORE_SHARD(BitSwap32);
  REGISTER_CORE_SHARD(BitSwap64);

  REGISTER_CORE_SHARD(ExpectInt);
  REGISTER_CORE_SHARD(ExpectInt2);
  REGISTER_CORE_SHARD(ExpectInt3);
  REGISTER_CORE_SHARD(ExpectInt4);
  REGISTER_CORE_SHARD(ExpectFloat);
  REGISTER_CORE_SHARD(ExpectFloat2);
  REGISTER_CORE_SHARD(ExpectFloat3);
  REGISTER_CORE_SHARD(ExpectFloat4);
  REGISTER_CORE_SHARD(ExpectBytes);
  REGISTER_CORE_SHARD(ExpectString);
  REGISTER_CORE_SHARD(ExpectImage);
  REGISTER_CORE_SHARD(ExpectBool);
  REGISTER_CORE_SHARD(ExpectSeq);
  REGISTER_CORE_SHARD(ExpectWire);
  REGISTER_CORE_SHARD(ExpectTable);

  using ImageToFloats = ToSeq<SHType::Image, SHType::Float>;
  using FloatsToImage = ToImage<SHType::Float>;
  REGISTER_SHARD("ImageToFloats", ImageToFloats);
  REGISTER_SHARD("FloatsToImage", FloatsToImage);

  using BytesToInts = ToSeq<SHType::Bytes, SHType::Int>;
  using BytesToString = ToString1<SHType::Bytes>;
  using IntsToBytes = ToBytes<SHType::Int>;
  REGISTER_SHARD("BytesToInts", BytesToInts);
  REGISTER_SHARD("BytesToString", BytesToString);
  REGISTER_SHARD("IntsToBytes", IntsToBytes);
  REGISTER_SHARD("StringToBytes", StringToBytes);

  using ExpectFloatSeq = ExpectXComplex<CoreInfo::FloatSeqType>;
  REGISTER_SHARD("ExpectFloatSeq", ExpectFloatSeq);
  using ExpectIntSeq = ExpectXComplex<CoreInfo::IntSeqType>;
  REGISTER_SHARD("ExpectIntSeq", ExpectIntSeq);
  using ExpectBytesSeq = ExpectXComplex<CoreInfo::BytesSeqType>;
  REGISTER_SHARD("ExpectBytesSeq", ExpectBytesSeq);
  using ExpectStringSeq = ExpectXComplex<CoreInfo::StringSeqType>;
  REGISTER_SHARD("ExpectStringSeq", ExpectStringSeq);

  REGISTER_SHARD("ExpectLike", ExpectLike);

  REGISTER_SHARD("ToBase64", ToBase64);
  REGISTER_SHARD("FromBase64", FromBase64);
  REGISTER_SHARD("HexToBytes", HexToBytes);
}
}; // namespace shards
