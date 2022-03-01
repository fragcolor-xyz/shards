/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "blockwrapper.hpp"
#include "chainblocks.h"
#include "number_types.hpp"
#include "shared.hpp"
#include <boost/beast/core/detail/base64.hpp>
#include <type_traits>

namespace chainblocks {
struct FromImage {
  // TODO SIMD this
  template <CBType OF> void toSeq(std::vector<Var> &output, const CBVar &input) {
    if constexpr (OF == CBType::Float) {
      // assume we want 0-1 normalized values
      if (input.valueType != CBType::Image)
        throw ActivationError("Expected Image type.");

      auto pixsize = 1;
      if ((input.payload.imageValue.flags & CBIMAGE_FLAGS_16BITS_INT) == CBIMAGE_FLAGS_16BITS_INT)
        pixsize = 2;
      else if ((input.payload.imageValue.flags & CBIMAGE_FLAGS_32BITS_FLOAT) == CBIMAGE_FLAGS_32BITS_FLOAT)
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
  template <CBType OF> void toImage(std::vector<uint8_t> &buffer, int w, int h, int c, const CBVar &input) {
    // TODO SIMD this
    if (input.payload.seqValue.len == 0)
      throw ActivationError("Input sequence was empty.");

    const int flatsize = std::min(w * h * c, int(input.payload.seqValue.len));
    buffer.resize(flatsize);

    if constexpr (OF == CBType::Float) {
      // assuming it's scaled 0-1
      for (int i = 0; i < flatsize; i++) {
        buffer[i] = uint8_t(input.payload.seqValue.elements[i].payload.floatValue * 255.0);
      }
    } else {
      throw ActivationError("Conversion pair not implemented yet.");
    }
  }

  template <CBType OF> void toBytes(std::vector<uint8_t> &buffer, const CBVar &input) {
    // TODO SIMD this
    if (input.payload.seqValue.len == 0)
      throw ActivationError("Input sequence was empty.");

    buffer.resize(size_t(input.payload.seqValue.len));

    if constexpr (OF == CBType::Int) {
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
  template <CBType OF> void toSeq(std::vector<Var> &output, const CBVar &input) {
    if constexpr (OF == CBType::Int) {
      // assume we want 0-1 normalized values
      if (input.valueType != CBType::Bytes)
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

template <CBType CBTYPE, CBType CBOTHER> struct ToSeq {
  static inline Type _inputType{{CBTYPE}};
  static inline Type _outputElemType{{CBOTHER}};
  static inline Type _outputType{{CBType::Seq, {.seqTypes = _outputElemType}}};

  static CBTypesInfo inputTypes() { return _inputType; }
  static CBTypesInfo outputTypes() { return _outputType; }

  std::vector<Var> _output;

  CBVar activate(CBContext *context, const CBVar &input) {
    // do not .clear here, for speed, let From manage that
    if constexpr (CBTYPE == CBType::Image) {
      FromImage c;
      c.toSeq<CBOTHER>(_output, input);
    } else if constexpr (CBTYPE == CBType::Bytes) {
      FromBytes c;
      c.toSeq<CBOTHER>(_output, input);
    } else {
      throw ActivationError("Conversion pair not implemented yet.");
    }
    return Var(_output);
  }
};

template <CBType CBTYPE> struct ToString1 {
  static inline Type _inputType{{CBTYPE}};

  static CBTypesInfo inputTypes() { return _inputType; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }

  std::string _output;

  CBVar activate(CBContext *context, const CBVar &input) {
    // do not .clear here, for speed, let From manage that
    if constexpr (CBTYPE == CBType::Bytes) {
      _output.clear();
      _output.assign((const char *)input.payload.bytesValue, size_t(input.payload.bytesSize));
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

  static inline Parameters _params{{"Width", CBCCSTR("The width of the output image."), {CoreInfo::IntType}},
                                   {"Height", CBCCSTR("The height of the output image."), {CoreInfo::IntType}},
                                   {"Channels", CBCCSTR("The channels of the output image."), {CoreInfo::IntType}}};

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
      _channels = uint8_t(std::min(CBInt(4), std::max(CBInt(1), value.payload.intValue)));
      break;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
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

template <CBType FROMTYPE> struct ToBytes {
  static inline Type _inputElemType{{FROMTYPE}};
  static inline Type _inputType{{CBType::Seq, {.seqTypes = _inputElemType}}};

  static CBTypesInfo inputTypes() { return _inputType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BytesType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    FromSeq c;
    c.toBytes<FROMTYPE>(_buffer, input);
    return Var(_buffer);
  }

private:
  std::vector<uint8_t> _buffer;
};

template <CBType ToType> struct ToNumber {
  const VectorTypeTraits *_outputVectorType{nullptr};
  const NumberTypeTraits *_outputNumberType{nullptr};

  // If known at compose time these are set
  // otherwise each element should be evaluated during activate
  const VectorTypeTraits *_inputVectorType{nullptr};
  const NumberConversion *_numberConversion{nullptr};

  CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  CBTypesInfo outputTypes() {
    if (!_outputVectorType)
      return CoreInfo::AnyType;
    return _outputVectorType->type;
  }

  static const NumberTypeTraits *getEnumNumberType() {
    static const NumberTypeTraits *result = nullptr;
    if (!result) {
      result = NumberTypeLookup::getInstance().get(NumberType::Int32);
      cbassert(result);
    }
    return result;
  }

  const NumberTypeTraits *determineFixedSeqNumberType(const CBTypeInfo &typeInfo) {
    if (!typeInfo.fixedSize) {
      return nullptr;
    }

    const NumberTypeTraits *fixedElemNumberType = nullptr;
    for (size_t i = 0; i < typeInfo.seqTypes.len; i++) {
      const CBTypeInfo &elementType = typeInfo.seqTypes.elements[i];
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

  CBTypeInfo compose(const CBInstanceData &data) {
    _outputVectorType = VectorTypeLookup::getInstance().get(ToType);
    if (!_outputVectorType) {
      throw ComposeError("Conversion not implemented for this type");
    }

    _outputNumberType = NumberTypeLookup::getInstance().get(_outputVectorType->numberType);
    _inputVectorType = VectorTypeLookup::getInstance().get(data.inputType.basicType);

    if (_inputVectorType) {
      _numberConversion =
          NumberTypeLookup::getInstance().getConversion(_inputVectorType->numberType, _outputVectorType->numberType);
      cbassert(_numberConversion);
    } else if (data.inputType.basicType == Seq) {
      const NumberTypeTraits *fixedNumberType = determineFixedSeqNumberType(data.inputType);
      if (fixedNumberType) {
        _numberConversion = fixedNumberType->conversionTable.get(_outputNumberType->type);
        cbassert(_numberConversion);

        OVERRIDE_ACTIVATE(data, activateSeqElementsFixed);
      }
    }

    return _outputVectorType->type;
  }

  void parseVector(CBVar &output, const CBVar &input) {
    const VectorTypeTraits *inputVectorType =
        _inputVectorType ? _inputVectorType : VectorTypeLookup::getInstance().get(input.valueType);
    if (!inputVectorType) {
      throw ActivationError("Cannot cast given input type.");
    }

    const NumberConversion *conversion =
        _numberConversion
            ? _numberConversion
            : NumberTypeLookup::getInstance().getConversion(inputVectorType->numberType, _outputVectorType->numberType);
    cbassert(conversion);

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

  void parseSeqElements(CBVar &output, const CBSeq &sequence) {
    uint8_t *dstPtr = (uint8_t *)&output.payload;
    for (size_t i = 0; i < sequence.len; i++) {
      const CBVar &elem = sequence.elements[i];

      const NumberTypeTraits *elemNumberType = NumberTypeLookup::getInstance().get(elem.valueType);
      if (!elemNumberType) {
        throw ActivationError("Cannot cast given sequence element type.");
      }

      const NumberConversion *conversion = elemNumberType->conversionTable.get(_outputVectorType->numberType);
      cbassert(conversion);

      conversion->convertOne(&elem.payload, dstPtr);
      dstPtr += conversion->outStride;
    }
  }

  void skipStringSeparators(const char *&strPtr) {
    while (*strPtr && (std::isspace(strPtr[0]) || strPtr[0] == ',' || strPtr[0] == ';')) {
      ++strPtr;
    }
  }

  void parseStringElements(CBVar &output, const char *str, size_t length) {
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

  void parseEnumValue(CBVar &output, CBEnum value) {
    _numberConversion = getEnumNumberType()->conversionTable.get(_outputVectorType->numberType);
    cbassert(_numberConversion);

    _numberConversion->convertOne(&value, &output.payload);
  }

  CBVar activateSeqElementsFixed(CBContext *context, const CBVar &input) {
    cbassert(_outputVectorType);
    cbassert(_numberConversion);

    const CBSeq &sequence = input.payload.seqValue;
    CBVar output{};
    output.valueType = _outputVectorType->cbType;

    uint8_t *dstPtr = (uint8_t *)&output.payload;
    for (size_t i = 0; i < sequence.len; i++) {
      const CBVar &elem = sequence.elements[i];

      _numberConversion->convertOne(&elem.payload, dstPtr);
      dstPtr += _numberConversion->outStride;
    }

    return output;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    cbassert(_outputVectorType);

    CBVar output{};
    output.valueType = _outputVectorType->cbType;

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
  static inline Types toHexTypes{CoreInfo::IntType, CoreInfo::BytesType, CoreInfo::StringType};
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
      CBLOG_ERROR("Unexpected value: {}", input);
      throw ActivationError("Expected type " + type2Name(ET) + " got instead " + type2Name(input.valueType));
    }
    return input;
  }
};

#define EXPECT_BLOCK(_name_, _cbtype_)                 \
  struct Expect##_name_ : public ExpectX<_cbtype_> {}; \
  RUNTIME_CORE_BLOCK(Expect##_name_);                  \
  RUNTIME_BLOCK_inputTypes(Expect##_name_);            \
  RUNTIME_BLOCK_outputTypes(Expect##_name_);           \
  RUNTIME_BLOCK_activate(Expect##_name_);              \
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
  static inline Parameters params{{"Unsafe",
                                   CBCCSTR("If we should skip performing deep type hashing and comparison. "
                                           "(generally fast but this might improve performance)"),
                                   {CoreInfo::BoolType}}};
  static inline uint64_t ExpectedHash = deriveTypeHash(ET);

  bool _unsafe{false};

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return ET; }

  void setParam(int index, const CBVar &value) { _unsafe = value.payload.boolValue; }

  CBVar getParam(int index) { return Var(_unsafe); }

  CBVar activate(CBContext *context, const CBVar &input) {
    const static CBTypeInfo info = ET;
    if (unlikely(input.valueType != info.basicType)) {
      CBLOG_ERROR("Unexpected value: {} expected type: {}", input, info);
      throw ActivationError("Expected type " + type2Name(info.basicType) + " got instead " + type2Name(input.valueType));
    } else if (!_unsafe) {
      auto inputTypeHash = deriveTypeHash(input);
      if (unlikely(inputTypeHash != ExpectedHash)) {
        CBLOG_ERROR("Unexpected value: {} expected type: {}", input, info);
        throw ActivationError("Expected type " + type2Name(info.basicType) + " got instead " + type2Name(input.valueType));
      }
    }
    return input;
  }
};

struct ExpectLike {
  ParamVar _example{};
  CBTypeInfo _expectedType{};
  bool _dispose{false};
  uint64_t _outputTypeHash{0};
  bool _unsafe{false};
  bool _derived{false};

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static inline Parameters params{{"Example",
                                   CBCCSTR("The example value to expect, in the case of a used chain, the "
                                           "output type of that chain will be used."),
                                   {CoreInfo::AnyType}},
                                  {"Unsafe",
                                   CBCCSTR("If we should skip performing deep type hashing and comparison. "
                                           "(generally fast but this might improve performance)"),
                                   {CoreInfo::BoolType}}};
  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
    if (index == 0)
      _example = value;
    else if (index == 1)
      _unsafe = value.payload.boolValue;
  }

  CBVar getParam(int index) {
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

  CBTypeInfo compose(const CBInstanceData &data) {
    if (_example.isVariable()) {
      throw ComposeError("The example value of ExpectLike cannot be a variable");
    } else {
      if (_expectedType.basicType != None && _dispose) {
        freeDerivedInfo(_expectedType);
        _expectedType = {};
        _dispose = false;
      }
      if (_example->valueType == CBType::Chain) {
        auto chain = CBChain::sharedFromRef(_example->payload.chainValue);
        _expectedType = chain->outputType;
        _outputTypeHash = deriveTypeHash(_expectedType);
        return _expectedType;
      } else {
        CBVar example = _example;
        _expectedType = deriveTypeInfo(example, data);
        _dispose = true;
        _outputTypeHash = deriveTypeHash(_expectedType);
        return _expectedType;
      }
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(input.valueType != _expectedType.basicType)) {
      CBLOG_ERROR("Unexpected value: {} expected type: {}", input, _expectedType);
      throw ActivationError("Unexpected input type");
    } else if (!_unsafe) {
      auto inputTypeHash = deriveTypeHash(input);
      if (unlikely(inputTypeHash != _outputTypeHash)) {
        CBLOG_ERROR("Unexpected value: {} expected type: {}", input, _expectedType);
        throw ActivationError("Unexpected input type");
      }
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
  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BytesType; }
  CBVar activate(CBContext *context, const CBVar &input) {
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
  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BytesType; }

  static int char2int(char input) {
    if (input >= '0' && input <= '9')
      return input - '0';
    if (input >= 'A' && input <= 'F')
      return input - 'A' + 10;
    if (input >= 'a' && input <= 'f')
      return input - 'a' + 10;
    throw std::invalid_argument("Invalid input string");
  }

  CBVar activate(CBContext *context, const CBVar &input) {
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
  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BytesType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    const auto len = CBSTRLEN(input);
    return Var((uint8_t *)input.payload.stringValue, uint32_t(len));
  }
};

void registerCastingBlocks() {
  REGISTER_CBLOCK("ToInt", ToNumber<Int>);
  REGISTER_CBLOCK("ToInt2", ToNumber<Int2>);
  REGISTER_CBLOCK("ToInt3", ToNumber<Int3>);
  REGISTER_CBLOCK("ToInt4", ToNumber<Int4>);
  REGISTER_CBLOCK("ToInt8", ToNumber<Int8>);
  REGISTER_CBLOCK("ToInt16", ToNumber<Int16>);
  REGISTER_CBLOCK("ToColor", ToNumber<Color>);
  REGISTER_CBLOCK("ToFloat", ToNumber<Float>);
  REGISTER_CBLOCK("ToFloat2", ToNumber<Float2>);
  REGISTER_CBLOCK("ToFloat3", ToNumber<Float3>);
  REGISTER_CBLOCK("ToFloat4", ToNumber<Float4>);

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

  using BytesToInts = ToSeq<CBType::Bytes, CBType::Int>;
  using BytesToString = ToString1<CBType::Bytes>;
  using IntsToBytes = ToBytes<CBType::Int>;
  REGISTER_CBLOCK("BytesToInts", BytesToInts);
  REGISTER_CBLOCK("BytesToString", BytesToString);
  REGISTER_CBLOCK("IntsToBytes", IntsToBytes);
  REGISTER_CBLOCK("StringToBytes", StringToBytes);

  using ExpectFloatSeq = ExpectXComplex<CoreInfo::FloatSeqType>;
  REGISTER_CBLOCK("ExpectFloatSeq", ExpectFloatSeq);
  using ExpectIntSeq = ExpectXComplex<CoreInfo::IntSeqType>;
  REGISTER_CBLOCK("ExpectIntSeq", ExpectIntSeq);
  using ExpectBytesSeq = ExpectXComplex<CoreInfo::BytesSeqType>;
  REGISTER_CBLOCK("ExpectBytesSeq", ExpectBytesSeq);
  using ExpectStringSeq = ExpectXComplex<CoreInfo::StringSeqType>;
  REGISTER_CBLOCK("ExpectStringSeq", ExpectStringSeq);

  REGISTER_CBLOCK("ExpectLike", ExpectLike);

  REGISTER_CBLOCK("ToBase64", ToBase64);
  REGISTER_CBLOCK("FromBase64", FromBase64);
  REGISTER_CBLOCK("HexToBytes", HexToBytes);
}
}; // namespace chainblocks
