/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

#ifndef CB_NO_BIGINT_BLOCKS

#include "math.h"
#include "shared.hpp"

#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>

using namespace boost::multiprecision;

namespace chainblocks {
namespace BigInt {
inline Var to_var(const cpp_int &bi, std::vector<uint8_t> &buffer) {
  buffer.clear();
  buffer.emplace_back(uint8_t(bi < 0));
  export_bits(bi, std::back_inserter(buffer), 8);
  return Var(&buffer.front(), buffer.size());
}

inline cpp_int from_var(const CBVar &op) {
  cpp_int bib;
  import_bits(bib, op.payload.bytesValue + 1,
              op.payload.bytesValue + op.payload.bytesSize);
  auto negative = bool(op.payload.bytesValue[0]);
  if (negative)
    bib *= -1;
  return bib;
}

struct ToBigInt {
  std::vector<uint8_t> _buffer;

  static inline Types InputTypes{CoreInfo::IntType, CoreInfo::FloatType,
                                 CoreInfo::StringType, CoreInfo::BytesType};
  static CBTypesInfo inputTypes() { return InputTypes; }
  static CBTypesInfo outputTypes() { return CoreInfo::BytesType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    cpp_int bi;
    switch (input.valueType) {
    case Int: {
      bi = input.payload.intValue;
    } break;
    case Float: {
      bi = cpp_int(input.payload.floatValue);
    } break;
    case String: {
      bi = cpp_int(input.payload.stringValue);
    } break;
    case Bytes: {
      import_bits(bi, input.payload.bytesValue,
                  input.payload.bytesValue + input.payload.bytesSize);
    } break;
    default: {
      throw ActivationError("Invalid input type");
    }
    }
    return to_var(bi, _buffer);
  }
};

template <typename T>
struct BigIntBinaryOp : public ::chainblocks::Math::BinaryOperation<T> {
  std::deque<std::vector<uint8_t>> _buffers;
  size_t _offset{0};

  static inline Types BigIntInputTypes{
      {CoreInfo::BytesType, CoreInfo::BytesSeqType}};

  static CBTypesInfo inputTypes() { return BigIntInputTypes; }
  static CBTypesInfo outputTypes() { return BigIntInputTypes; }

  CBParametersInfo parameters() {
    static Parameters params{
        {"Operand",
         CBCCSTR("The bytes variable representing the operand"),
         {CoreInfo::BytesVarType, CoreInfo::BytesVarSeqType}}};
    return params;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    _offset = 0;
    return ::chainblocks::Math::BinaryOperation<T>::activate(context, input);
  }
};

#define BIGINT_MATH_OP(__NAME__, __OP__)                                       \
  struct __NAME__ : public BigIntBinaryOp<__NAME__> {                          \
    void operator()(CBVar &output, const CBVar &input, const CBVar &operand,   \
                    void *pself) {                                             \
      auto self = reinterpret_cast<__NAME__ *>(pself);                         \
      std::vector<uint8_t> *buffer = nullptr;                                  \
      if (self->_buffers.size() <= _offset) {                                  \
        buffer = &self->_buffers.emplace_back();                               \
      } else {                                                                 \
        buffer = &self->_buffers[_offset];                                     \
      }                                                                        \
      cpp_int bia = from_var(input);                                           \
      cpp_int bib = from_var(operand);                                         \
      cpp_int bres = bia __OP__ bib;                                           \
      output = to_var(bres, *buffer);                                          \
      _offset++;                                                               \
    }                                                                          \
  };

struct BigOperandBase {
  std::vector<uint8_t> _buffer;

  static CBTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BytesType; }

  CBParametersInfo parameters() {
    static Parameters params{
        {"Operand",
         CBCCSTR("The bytes variable representing the operand"),
         {CoreInfo::BytesVarType}}};
    return params;
  }

  ParamVar _op{};

  void setParam(int index, const CBVar &value) { _op = value; }

  CBVar getParam(int index) { return _op; }

  void cleanup() { _op.cleanup(); }

  void warmup(CBContext *context) { _op.warmup(context); }

  const CBVar &getOperand() {
    CBVar &op = _op.get();
    if (op.valueType == None) {
      throw ActivationError("Operand is None, should be valid bigint bytes");
    }
    return op;
  }
};

struct RegOperandBase {
  std::vector<uint8_t> _buffer;

  static CBTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BytesType; }

  CBParametersInfo parameters() {
    static Parameters params{{"Operand",
                              CBCCSTR("The integer operand, can be a variable"),
                              {CoreInfo::IntType, CoreInfo::IntVarType}}};
    return params;
  }

  ParamVar _op{};

  void setParam(int index, const CBVar &value) { _op = value; }

  CBVar getParam(int index) { return _op; }

  void cleanup() { _op.cleanup(); }

  void warmup(CBContext *context) { _op.warmup(context); }

  const CBVar &getOperand() {
    CBVar &op = _op.get();
    if (op.valueType == None) {
      throw ActivationError("Operand is None, should be an integer");
    }
    return op;
  }
};

BIGINT_MATH_OP(Add, +);
BIGINT_MATH_OP(Subtract, -);
BIGINT_MATH_OP(Multiply, *);
BIGINT_MATH_OP(Divide, /);
BIGINT_MATH_OP(Xor, ^);
BIGINT_MATH_OP(And, &);
BIGINT_MATH_OP(Or, |);
BIGINT_MATH_OP(Mod, %);

#define BIGINT_LOGIC_OP(__NAME__, __OP__)                                      \
  struct __NAME__ : public BigOperandBase {                                    \
    static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }            \
    CBVar activate(CBContext *context, const CBVar &input) {                   \
      cpp_int bia = from_var(input);                                           \
      auto op = getOperand();                                                  \
      cpp_int bib = from_var(op);                                              \
      bool res = bia __OP__ bib;                                               \
      return Var(res);                                                         \
    }                                                                          \
  }

BIGINT_LOGIC_OP(Is, ==);
BIGINT_LOGIC_OP(IsNot, !=);
BIGINT_LOGIC_OP(IsMore, >);
BIGINT_LOGIC_OP(IsLess, <);
BIGINT_LOGIC_OP(IsMoreEqual, >=);
BIGINT_LOGIC_OP(IsLessEqual, <=);

#define BIGINT_BINARY_OP(__NAME__, __OP__)                                     \
  struct __NAME__ : public BigIntBinaryOp<__NAME__> {                          \
    void operator()(CBVar &output, const CBVar &input, const CBVar &operand,   \
                    void *pself) {                                             \
      auto self = reinterpret_cast<__NAME__ *>(pself);                         \
      std::vector<uint8_t> *buffer = nullptr;                                  \
      if (self->_buffers.size() <= _offset) {                                  \
        buffer = &self->_buffers.emplace_back();                               \
      } else {                                                                 \
        buffer = &self->_buffers[_offset];                                     \
      }                                                                        \
      cpp_int bia = from_var(input);                                           \
      cpp_int bib = from_var(operand);                                         \
      cpp_int bres = __OP__(bia, bib);                                         \
      output = to_var(bres, *buffer);                                          \
      _offset++;                                                               \
    }                                                                          \
  };

BIGINT_BINARY_OP(Min, std::min);
BIGINT_BINARY_OP(Max, std::max);

#define BIGINT_REG_BINARY_OP(__NAME__, __OP__)                                 \
  struct __NAME__ : public RegOperandBase {                                    \
    CBVar activate(CBContext *context, const CBVar &input) {                   \
      cpp_int bia = from_var(input);                                           \
      auto op = getOperand();                                                  \
      if (op.valueType != Int)                                                 \
        throw ActivationError("Pow operand should be an Int");                 \
      cpp_int bres = __OP__(bia, op.payload.intValue);                         \
      return to_var(bres, _buffer);                                            \
    }                                                                          \
  }

BIGINT_REG_BINARY_OP(Pow, pow);

#define BIGINT_UNARY_OP(__NAME__, __OP__)                                      \
  struct __NAME__ : public RegOperandBase {                                    \
    CBParametersInfo parameters() { return {}; }                               \
    CBVar activate(CBContext *context, const CBVar &input) {                   \
      cpp_int bia = from_var(input);                                           \
      cpp_int bres = __OP__(bia);                                              \
      return to_var(bres, _buffer);                                            \
    }                                                                          \
  }

BIGINT_UNARY_OP(Sqrt, sqrt);

struct ShiftBase {
  ParamVar _shift{Var(0)};

  void setParam(int index, const CBVar &value) { _shift = value; }

  CBVar getParam(int index) { return _shift; }

  void cleanup() { _shift.cleanup(); }

  void warmup(CBContext *context) { _shift.warmup(context); }
};

struct Shift : public ShiftBase {
  std::vector<uint8_t> _buffer;

  static CBTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BytesType; }

  CBParametersInfo parameters() {
    static Parameters params{
        {"By",
         CBCCSTR(
             "The shift is of the decimal point, i.e. of powers of ten, and is "
             "to the left if n is negative or to the right if n is positive."),
         {CoreInfo::IntType, CoreInfo::IntVarType}}};
    return params;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    cpp_int bi = from_var(input);
    cpp_dec_float_100 bf(bi);

    cpp_dec_float_100 bshift(_shift.get().payload.intValue);
    bshift = pow(cpp_dec_float_100(10), bshift);

    auto bres = cpp_int(bf * bshift);

    return to_var(bres, _buffer);
  }
};

struct ToFloat : public ShiftBase {
  static CBTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static CBTypesInfo outputTypes() { return CoreInfo::FloatType; }

  CBParametersInfo parameters() {
    static Parameters params{
        {"ShiftedBy",
         CBCCSTR(
             "The shift is of the decimal point, i.e. of powers of ten, and is "
             "to the left if n is negative or to the right if n is positive."),
         {CoreInfo::IntType}}};
    return params;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    cpp_int bi = from_var(input);
    cpp_dec_float_100 bf(bi);

    cpp_dec_float_100 bshift(_shift.get().payload.intValue);
    bshift = pow(cpp_dec_float_100(10), bshift);

    auto bres = bf * bshift;

    return Var(bres.convert_to<double>());
  }
};

struct ToInt {
  static CBTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static CBTypesInfo outputTypes() { return CoreInfo::IntType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    cpp_int bi = from_var(input);
    return Var(bi.convert_to<int64_t>());
  }
};

struct FromFloat : public ShiftBase {
  std::vector<uint8_t> _buffer;

  static CBTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BytesType; }

  CBParametersInfo parameters() {
    static Parameters params{
        {"ShiftedBy",
         CBCCSTR(
             "The shift is of the decimal point, i.e. of powers of ten, and is "
             "to the left if n is negative or to the right if n is positive."),
         {CoreInfo::IntType}}};
    return params;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    cpp_dec_float_100 bi(input.payload.floatValue);

    cpp_dec_float_100 bshift(_shift.get().payload.intValue);
    bshift = pow(cpp_dec_float_100(10), bshift);

    auto bres = bi * bshift;
    cpp_int bo(bres);

    return to_var(bo, _buffer);
  }
};

struct ToString {
  std::string _buffer;

  static CBTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    cpp_int bi = from_var(input);
    _buffer = bi.str();
    return Var(_buffer);
  }
};

struct ToBytes {
  std::vector<uint8_t> _buffer;

  static CBTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BytesType; }

  CBParametersInfo parameters() {
    static Parameters params{{"Bits",
                              CBCCSTR("The desired amount of bits for the "
                                      "output or 0 for automatic packing."),
                              {CoreInfo::IntType}}};
    return params;
  }

  ParamVar _bits{Var(0)};

  void setParam(int index, const CBVar &value) { _bits = value; }

  CBVar getParam(int index) { return _bits; }

  void warmup(CBContext *context) { _bits.warmup(context); }
  void cleanup() { _bits.cleanup(); }

  CBVar activate(CBContext *context, const CBVar &input) {
    const auto bits = _bits.get().payload.intValue;
    if (bits <= 0) {
      CBVar fixedInput = input;
      fixedInput.payload.bytesValue++;
      fixedInput.payload.bytesSize--;
      return fixedInput;
    } else {
      cpp_int bi = from_var(input);
      const auto usedBits = msb(bi) + 1;
      if (usedBits > bits) {
        throw ActivationError(
            "The number of used bits is higher than the requested bits");
      }
      const auto padding = bits - usedBits;
      _buffer.clear();
      export_bits(bi, std::back_inserter(_buffer), 8);
      // this is because we are using little endianess
      _buffer.insert(_buffer.begin(), padding / 8, 0);
      return Var(_buffer);
    }
  }
};

struct ToHex {
  VarStringStream stream;
  static inline Types toHexTypes{CoreInfo::IntType, CoreInfo::BytesType,
                                 CoreInfo::StringType};
  static CBTypesInfo inputTypes() { return toHexTypes; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }
  CBVar activate(CBContext *context, const CBVar &input) {
    CBVar fixedInput = input;
    fixedInput.payload.bytesValue++;
    fixedInput.payload.bytesSize--;
    stream.tryWriteHex(fixedInput);
    return Var(stream.str());
  }
};

struct Abs {
  std::vector<uint8_t> _buffer;

  static CBTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BytesType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    cpp_int bi = from_var(input);
    cpp_int abi = abs(bi);
    return to_var(abi, _buffer);
  }
};

void registerBlocks() {
  REGISTER_CBLOCK("BigInt", ToBigInt);
  REGISTER_CBLOCK("BigInt.Add", Add);
  REGISTER_CBLOCK("BigInt.Subtract", Subtract);
  REGISTER_CBLOCK("BigInt.Multiply", Multiply);
  REGISTER_CBLOCK("BigInt.Divide", Divide);
  REGISTER_CBLOCK("BigInt.Xor", Xor);
  REGISTER_CBLOCK("BigInt.And", And);
  REGISTER_CBLOCK("BigInt.Or", Or);
  REGISTER_CBLOCK("BigInt.Mod", Mod);
  REGISTER_CBLOCK("BigInt.Shift", Shift);
  REGISTER_CBLOCK("BigInt.ToFloat", ToFloat);
  REGISTER_CBLOCK("BigInt.ToInt", ToInt);
  REGISTER_CBLOCK("BigInt.FromFloat", FromFloat);
  REGISTER_CBLOCK("BigInt.ToString", ToString);
  REGISTER_CBLOCK("BigInt.ToBytes", ToBytes);
  REGISTER_CBLOCK("BigInt.ToHex", ToHex);
  REGISTER_CBLOCK("BigInt.Is", Is);
  REGISTER_CBLOCK("BigInt.IsNot", IsNot);
  REGISTER_CBLOCK("BigInt.IsMore", IsMore);
  REGISTER_CBLOCK("BigInt.IsLess", IsLess);
  REGISTER_CBLOCK("BigInt.IsMoreEqual", IsMoreEqual);
  REGISTER_CBLOCK("BigInt.IsLessEqual", IsLessEqual);
  REGISTER_CBLOCK("BigInt.Min", Min);
  REGISTER_CBLOCK("BigInt.Max", Max);
  REGISTER_CBLOCK("BigInt.Pow", Pow);
  REGISTER_CBLOCK("BigInt.Abs", Abs);
  REGISTER_CBLOCK("BigInt.Sqrt", Sqrt);
}
} // namespace BigInt
} // namespace chainblocks

#else
namespace chainblocks {
namespace BigInt {
void registerBlocks() {}
} // namespace BigInt
} // namespace chainblocks
#endif