/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

#include "math.h"
#include "shared.hpp"

#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>

using namespace boost::multiprecision;

namespace shards {
namespace BigInt {
inline Var to_var(const cpp_int &bi, std::vector<uint8_t> &buffer) {
  buffer.clear();
  buffer.emplace_back(uint8_t(bi < 0));
  export_bits(bi, std::back_inserter(buffer), 8);
  return Var(&buffer.front(), buffer.size());
}

inline cpp_int from_var(const SHVar &op) {
  cpp_int bib;
  import_bits(bib, op.payload.bytesValue + 1, op.payload.bytesValue + op.payload.bytesSize);
  auto negative = bool(op.payload.bytesValue[0]);
  if (negative)
    bib *= -1;
  return bib;
}

struct ToBigInt {
  std::vector<uint8_t> _buffer;

  static inline Types InputTypes{CoreInfo::IntType, CoreInfo::FloatType, CoreInfo::StringType, CoreInfo::BytesType};
  static SHTypesInfo inputTypes() { return InputTypes; }
  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString outputHelp() { return SHCCSTR("Big integer represented as bytes."); }

  SHVar activate(SHContext *context, const SHVar &input) {
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
      import_bits(bi, input.payload.bytesValue, input.payload.bytesValue + input.payload.bytesSize);
    } break;
    default: {
      throw ActivationError("Invalid input type");
    }
    }
    return to_var(bi, _buffer);
  }
};

template <typename T> struct BigIntBinaryOp : public ::shards::Math::BinaryOperation<T> {
  std::deque<std::vector<uint8_t>> _buffers;
  size_t _offset{0};

  static inline Types BigIntInputTypes{{CoreInfo::BytesType, CoreInfo::BytesSeqType}};

  static SHTypesInfo inputTypes() { return BigIntInputTypes; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Any valid big integer(s) represented as bytes supported by "
                   "this operation.");
  }
  static SHTypesInfo outputTypes() { return BigIntInputTypes; }

  SHParametersInfo parameters() {
    static Parameters params{
        {"Operand", SHCCSTR("The bytes variable representing the operand"), {CoreInfo::BytesVarType, CoreInfo::BytesVarSeqType}}};
    return params;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    _offset = 0;
    return ::shards::Math::BinaryOperation<T>::activate(context, input);
  }
};

#define BIGINT_MATH_OP(__NAME__, __OP__)                                                    \
  struct __NAME__ : public BigIntBinaryOp<__NAME__> {                                       \
    void operator()(SHVar &output, const SHVar &input, const SHVar &operand, void *pself) { \
      auto self = reinterpret_cast<__NAME__ *>(pself);                                      \
      std::vector<uint8_t> *buffer = nullptr;                                               \
      if (self->_buffers.size() <= _offset) {                                               \
        buffer = &self->_buffers.emplace_back();                                            \
      } else {                                                                              \
        buffer = &self->_buffers[_offset];                                                  \
      }                                                                                     \
      cpp_int bia = from_var(input);                                                        \
      cpp_int bib = from_var(operand);                                                      \
      cpp_int bres = bia __OP__ bib;                                                        \
      output = to_var(bres, *buffer);                                                       \
      _offset++;                                                                            \
    }                                                                                       \
  };

struct BigOperandBase {
  std::vector<uint8_t> _buffer;

  static SHTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString inputHelp() { return SHCCSTR("Big integer represented as bytes."); }
  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString outputHelp() { return SHCCSTR("Big integer represented as bytes."); }

  SHParametersInfo parameters() {
    static Parameters params{{"Operand", SHCCSTR("The bytes variable representing the operand"), {CoreInfo::BytesVarType}}};
    return params;
  }

  ParamVar _op{};

  void setParam(int index, const SHVar &value) { _op = value; }

  SHVar getParam(int index) { return _op; }

  void cleanup() { _op.cleanup(); }

  void warmup(SHContext *context) { _op.warmup(context); }

  const SHVar &getOperand() {
    SHVar &op = _op.get();
    if (op.valueType == None) {
      throw ActivationError("Operand is None, should be valid bigint bytes");
    }
    return op;
  }
};

struct RegOperandBase {
  std::vector<uint8_t> _buffer;

  static SHTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString inputHelp() { return SHCCSTR("Big integer represented as bytes."); }
  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString outputHelp() { return SHCCSTR("Big integer represented as bytes."); }

  SHParametersInfo parameters() {
    static Parameters params{
        {"Operand", SHCCSTR("The integer operand, can be a variable"), {CoreInfo::IntType, CoreInfo::IntVarType}}};
    return params;
  }

  ParamVar _op{};

  void setParam(int index, const SHVar &value) { _op = value; }

  SHVar getParam(int index) { return _op; }

  void cleanup() { _op.cleanup(); }

  void warmup(SHContext *context) { _op.warmup(context); }

  const SHVar &getOperand() {
    SHVar &op = _op.get();
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

#define BIGINT_LOGIC_OP(__NAME__, __OP__)                                                                                      \
  struct __NAME__ : public BigOperandBase {                                                                                    \
    static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }                                                            \
    static SHOptionalString outputHelp() { return SHCCSTR("A boolean value repesenting the result of the logic operation."); } \
                                                                                                                               \
    SHVar activate(SHContext *context, const SHVar &input) {                                                                   \
      cpp_int bia = from_var(input);                                                                                           \
      auto op = getOperand();                                                                                                  \
      cpp_int bib = from_var(op);                                                                                              \
      bool res = bia __OP__ bib;                                                                                               \
      return Var(res);                                                                                                         \
    }                                                                                                                          \
  }

BIGINT_LOGIC_OP(Is, ==);
BIGINT_LOGIC_OP(IsNot, !=);
BIGINT_LOGIC_OP(IsMore, >);
BIGINT_LOGIC_OP(IsLess, <);
BIGINT_LOGIC_OP(IsMoreEqual, >=);
BIGINT_LOGIC_OP(IsLessEqual, <=);

#define BIGINT_BINARY_OP(__NAME__, __OP__)                                                  \
  struct __NAME__ : public BigIntBinaryOp<__NAME__> {                                       \
    void operator()(SHVar &output, const SHVar &input, const SHVar &operand, void *pself) { \
      auto self = reinterpret_cast<__NAME__ *>(pself);                                      \
      std::vector<uint8_t> *buffer = nullptr;                                               \
      if (self->_buffers.size() <= _offset) {                                               \
        buffer = &self->_buffers.emplace_back();                                            \
      } else {                                                                              \
        buffer = &self->_buffers[_offset];                                                  \
      }                                                                                     \
      cpp_int bia = from_var(input);                                                        \
      cpp_int bib = from_var(operand);                                                      \
      cpp_int bres = __OP__(bia, bib);                                                      \
      output = to_var(bres, *buffer);                                                       \
      _offset++;                                                                            \
    }                                                                                       \
  };

BIGINT_BINARY_OP(Min, std::min);
BIGINT_BINARY_OP(Max, std::max);

#define BIGINT_REG_BINARY_OP(__NAME__, __OP__)                 \
  struct __NAME__ : public RegOperandBase {                    \
    SHVar activate(SHContext *context, const SHVar &input) {   \
      cpp_int bia = from_var(input);                           \
      auto op = getOperand();                                  \
      if (op.valueType != Int)                                 \
        throw ActivationError("Pow operand should be an Int"); \
      cpp_int bres = __OP__(bia, op.payload.intValue);         \
      return to_var(bres, _buffer);                            \
    }                                                          \
  }

BIGINT_REG_BINARY_OP(Pow, pow);

#define BIGINT_UNARY_OP(__NAME__, __OP__)                    \
  struct __NAME__ : public RegOperandBase {                  \
    SHParametersInfo parameters() { return {}; }             \
    SHVar activate(SHContext *context, const SHVar &input) { \
      cpp_int bia = from_var(input);                         \
      cpp_int bres = __OP__(bia);                            \
      return to_var(bres, _buffer);                          \
    }                                                        \
  }

BIGINT_UNARY_OP(Sqrt, sqrt);

struct ShiftBase {
  ParamVar _shift{Var(0)};

  void setParam(int index, const SHVar &value) { _shift = value; }

  SHVar getParam(int index) { return _shift; }

  void cleanup() { _shift.cleanup(); }

  void warmup(SHContext *context) { _shift.warmup(context); }
};

struct Shift : public ShiftBase {
  std::vector<uint8_t> _buffer;

  static SHTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString inputHelp() { return SHCCSTR("Big integer represented as bytes."); }
  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString outputHelp() { return SHCCSTR("Big integer represented as bytes."); }

  SHParametersInfo parameters() {
    static Parameters params{{"By",
                              SHCCSTR("The shift is of the decimal point, i.e. of powers of ten, and is "
                                      "to the left if n is negative or to the right if n is positive."),
                              {CoreInfo::IntType, CoreInfo::IntVarType}}};
    return params;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    cpp_int bi = from_var(input);
    cpp_dec_float_100 bf(bi);

    cpp_dec_float_100 bshift(_shift.get().payload.intValue);
    bshift = pow(cpp_dec_float_100(10), bshift);

    auto bres = cpp_int(bf * bshift);

    return to_var(bres, _buffer);
  }
};

struct ToFloat : public ShiftBase {
  static SHOptionalString help() { return SHCCSTR("Converts a big integer value to a floating point number."); }

  static SHTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString inputHelp() { return SHCCSTR("Big integer represented as bytes."); }

  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }
  static SHOptionalString outputHelp() { return SHCCSTR("Floating point number representation of the big integer value."); }

  SHParametersInfo parameters() {
    static Parameters params{{"ShiftedBy",
                              SHCCSTR("The shift is of the decimal point, i.e. of powers of ten, and is "
                                      "to the left if n is negative or to the right if n is positive."),
                              {CoreInfo::IntType}}};
    return params;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    cpp_int bi = from_var(input);
    cpp_dec_float_100 bf(bi);

    cpp_dec_float_100 bshift(_shift.get().payload.intValue);
    bshift = pow(cpp_dec_float_100(10), bshift);

    auto bres = bf * bshift;

    return Var(bres.convert_to<double>());
  }
};

struct ToInt {
  static SHOptionalString help() { return SHCCSTR("Converts a big integer value to an integer."); }

  static SHTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString inputHelp() { return SHCCSTR("Big integer represented as bytes."); }

  static SHTypesInfo outputTypes() { return CoreInfo::IntType; }
  static SHOptionalString outputHelp() { return SHCCSTR("Integer representation of the big integer value."); }

  SHVar activate(SHContext *context, const SHVar &input) {
    cpp_int bi = from_var(input);
    return Var(bi.convert_to<int64_t>());
  }
};

struct FromFloat : public ShiftBase {
  static SHOptionalString help() { return SHCCSTR("Converts a floating point number to a big integer."); }

  static SHTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static SHOptionalString inputHelp() { return SHCCSTR("Floating point number."); }

  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString outputHelp() { return SHCCSTR("Big integer represented as bytes."); }

  SHParametersInfo parameters() {
    static Parameters params{{"ShiftedBy",
                              SHCCSTR("The shift is of the decimal point, i.e. of powers of ten, and is "
                                      "to the left if n is negative or to the right if n is positive."),
                              {CoreInfo::IntType}}};
    return params;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    cpp_dec_float_100 bi(input.payload.floatValue);

    cpp_dec_float_100 bshift(_shift.get().payload.intValue);
    bshift = pow(cpp_dec_float_100(10), bshift);

    auto bres = bi * bshift;
    cpp_int bo(bres);

    return to_var(bo, _buffer);
  }

private:
  std::vector<uint8_t> _buffer;
};

struct ToString {
  static SHOptionalString help() { return SHCCSTR("Converts the value to a string representation."); }

  static SHTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString inputHelp() { return SHCCSTR("Big integer represented as bytes."); }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }
  static SHOptionalString outputHelp() { return SHCCSTR("String representation of the big integer value."); }

  SHVar activate(SHContext *context, const SHVar &input) {
    cpp_int bi = from_var(input);
    _buffer = bi.str();
    return Var(_buffer);
  }

private:
  std::string _buffer;
};

struct ToBytes {
  std::vector<uint8_t> _buffer;

  static SHTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString inputHelp() { return SHCCSTR("Big integer represented as bytes."); }
  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }

  SHParametersInfo parameters() {
    static Parameters params{{"Bits",
                              SHCCSTR("The desired amount of bits for the "
                                      "output or 0 for automatic packing."),
                              {CoreInfo::IntType}}};
    return params;
  }

  ParamVar _bits{Var(0)};

  void setParam(int index, const SHVar &value) { _bits = value; }

  SHVar getParam(int index) { return _bits; }

  void warmup(SHContext *context) { _bits.warmup(context); }
  void cleanup() { _bits.cleanup(); }

  SHVar activate(SHContext *context, const SHVar &input) {
    const auto bits = _bits.get().payload.intValue;
    if (bits <= 0) {
      SHVar fixedInput = input;
      fixedInput.payload.bytesValue++;
      fixedInput.payload.bytesSize--;
      return fixedInput;
    } else {
      cpp_int bi = from_var(input);
      const auto usedBits = msb(bi) + 1;
      if (usedBits > bits) {
        throw ActivationError("The number of used bits is higher than the requested bits");
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
  static SHOptionalString help() { return SHCCSTR("Converts the value to a hexadecimal representation."); }

  static inline Types toHexTypes{CoreInfo::IntType, CoreInfo::BytesType, CoreInfo::StringType};
  static SHTypesInfo inputTypes() { return toHexTypes; }

  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }
  static SHOptionalString outputHelp() { return SHCCSTR("Hexadecimal representation of the integer value."); }

  SHVar activate(SHContext *context, const SHVar &input) {
    SHVar fixedInput = input;
    fixedInput.payload.bytesValue++;
    fixedInput.payload.bytesSize--;
    _stream.tryWriteHex(fixedInput);
    return Var(_stream.str());
  }

private:
  VarStringStream _stream;
};

struct Abs {
  static SHOptionalString help() { return SHCCSTR("Computes the absolute value of a big integer."); }

  static SHTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString inputHelp() { return SHCCSTR("Big integer represented as bytes."); }

  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString outputHelp() { return SHCCSTR("Big integer represented as bytes."); }

  SHVar activate(SHContext *context, const SHVar &input) {
    cpp_int bi = from_var(input);
    cpp_int abi = abs(bi);
    return to_var(abi, _buffer);
  }

private:
  std::vector<uint8_t> _buffer;
};

void registerShards() {
  REGISTER_SHARD("BigInt", ToBigInt);
  REGISTER_SHARD("BigInt.Add", Add);
  REGISTER_SHARD("BigInt.Subtract", Subtract);
  REGISTER_SHARD("BigInt.Multiply", Multiply);
  REGISTER_SHARD("BigInt.Divide", Divide);
  REGISTER_SHARD("BigInt.Xor", Xor);
  REGISTER_SHARD("BigInt.And", And);
  REGISTER_SHARD("BigInt.Or", Or);
  REGISTER_SHARD("BigInt.Mod", Mod);
  REGISTER_SHARD("BigInt.Shift", Shift);
  REGISTER_SHARD("BigInt.ToFloat", ToFloat);
  REGISTER_SHARD("BigInt.ToInt", ToInt);
  REGISTER_SHARD("BigInt.FromFloat", FromFloat);
  REGISTER_SHARD("BigInt.ToString", ToString);
  REGISTER_SHARD("BigInt.ToBytes", ToBytes);
  REGISTER_SHARD("BigInt.ToHex", ToHex);
  REGISTER_SHARD("BigInt.Is", Is);
  REGISTER_SHARD("BigInt.IsNot", IsNot);
  REGISTER_SHARD("BigInt.IsMore", IsMore);
  REGISTER_SHARD("BigInt.IsLess", IsLess);
  REGISTER_SHARD("BigInt.IsMoreEqual", IsMoreEqual);
  REGISTER_SHARD("BigInt.IsLessEqual", IsLessEqual);
  REGISTER_SHARD("BigInt.Min", Min);
  REGISTER_SHARD("BigInt.Max", Max);
  REGISTER_SHARD("BigInt.Pow", Pow);
  REGISTER_SHARD("BigInt.Abs", Abs);
  REGISTER_SHARD("BigInt.Sqrt", Sqrt);
}
} // namespace BigInt
} // namespace shards
