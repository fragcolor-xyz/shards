/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

#include <shards/core/foundation.hpp>
#include <shards/shards.h>
#include <shards/core/shared.hpp>
#include <shards/core/module.hpp>
#include "math.hpp"
#include <math.h>
#include <deque>
#include <stdexcept>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <stdexcept>

using namespace boost::multiprecision;

struct BigIntDefaultHelpText {
  static inline const SHOptionalString BigIntInputOutput =
      SHCCSTR("Big integer represented as bytes.");
};

namespace shards {
namespace BigInt {
inline Var to_var(const cpp_int &bi, std::vector<uint8_t> &buffer) {
  buffer.clear();
  buffer.emplace_back(uint8_t(bi < 0));
  export_bits(bi, std::back_inserter(buffer), 8);
  return Var(&buffer.front(), buffer.size());
}

inline cpp_int from_var_payload(const SHVarPayload &payload) {
  cpp_int bib;
  import_bits(bib, payload.bytesValue + 1, payload.bytesValue + payload.bytesSize);
  auto negative = bool(payload.bytesValue[0]);
  if (negative)
    bib *= -1;
  return bib;
}

inline cpp_int from_var(const SHVar &op) { return from_var_payload(op.payload); }



struct ToBigInt {
  std::vector<uint8_t> _buffer;

  static SHOptionalString help() {
    return SHCCSTR(
        "Converts various input types to a big integer represented as bytes. Big integers are signed and can be of arbitrary "
        "size. This allows for users to work with numbers that are too large to be represented by standard integer types.");
  }

  static inline Types InputTypes{CoreInfo::IntType, CoreInfo::FloatType, CoreInfo::StringType, CoreInfo::BytesType};
  static SHTypesInfo inputTypes() { return InputTypes; }
  static SHOptionalString inputHelp() { return SHCCSTR("The input value to convert to a big integer."); }
  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString outputHelp() { return BigIntDefaultHelpText::BigIntInputOutput; }

  SHVar activate(SHContext *context, const SHVar &input) {
    cpp_int bi;
    switch (input.valueType) {
    case SHType::Int: {
      bi = input.payload.intValue;
    } break;
    case SHType::Float: {
      bi = cpp_int(input.payload.floatValue);
    } break;
    case SHType::String: {
      auto sv = SHSTRVIEW(input);
      bi = cpp_int(sv);
    } break;
    case SHType::Bytes: {
      import_bits(bi, input.payload.bytesValue, input.payload.bytesValue + input.payload.bytesSize);
    } break;
    default: {
      throw ActivationError("Invalid input type");
    }
    }
    return to_var(bi, _buffer);
  }
};

struct BigIntOutputBuffer {
  struct Output {
    std::vector<uint8_t> data;
  };

  std::deque<Output> _outputs;
  size_t _index{0};

  void reset() { _index = 0; }

  Output &getOutput() {
    if (_index >= _outputs.size())
      _outputs.emplace_back();
    auto &output = _outputs[_index];
    _index++;
    return output;
  }
};

template <typename TOp> struct BigIntBinaryOperation {
  TOp op;
  BigIntOutputBuffer _outputs;

  Math::OpType validateTypes(const SHTypeInfo &lhs, const SHType &rhs, SHTypeInfo &resultType) {
    Math::OpType opType = Math::OpType::Invalid;
    if (lhs.basicType == SHType::Bytes && rhs == SHType::Bytes) {
      opType = Math::OpType::Direct;
      resultType = CoreInfo::BytesType;
    }
    return opType;
  }

  void operateDirect(SHVar &outputVar, const SHVar &a, const SHVar &b) {
    cpp_int bia = from_var(a);
    cpp_int bib = from_var(b);

    cpp_int bres = op.template apply(bia, bib);

    auto &output = _outputs.getOutput();
    outputVar = to_var(bres, output.data);
    outputVar.flags = SHVAR_FLAGS_FOREIGN; // prevent double frees!
  }

  void operateBroadcast(SHVar &output, const SHVar &a, const SHVar &b) {
    throw std::logic_error("invalid broadcast on bigint types");
  }

  void reset() { _outputs.reset(); }
};

// TOp follows the same interface as the base BinaryOperation accepts
// with the addition of reset() to recycle the output buffers
template <typename TOp> struct BinaryOperation : public ::shards::Math::BinaryOperation<TOp> {
  using Math::BinaryBase::_operand;
  using Math::BinaryBase::_opType;
  using Math::BinaryOperation<TOp>::op;

  static inline Types BigIntInputTypes{{CoreInfo::BytesType, CoreInfo::BytesSeqType}};

  static SHTypesInfo inputTypes() { return BigIntInputTypes; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Any valid big integer(s) represented as bytes supported by "
                   "this operation.");
  }
  static SHTypesInfo outputTypes() { return BigIntInputTypes; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("Returns the result of the operation as a big integer or sequence of big integers represented as bytes.");
  }

  SHParametersInfo parameters() {
    static Parameters params{
        {"Operand", SHCCSTR("The bytes variable representing the operand"), {CoreInfo::BytesVarType, CoreInfo::BytesVarSeqType}}};
    return params;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    op.reset();
    return Math::BinaryOperation<TOp>::activate(context, input);
  }
};

struct BigOperandBase {
  std::vector<uint8_t> _buffer;

  static SHTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString inputHelp() { return BigIntDefaultHelpText::BigIntInputOutput; }
  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString outputHelp() { return BigIntDefaultHelpText::BigIntInputOutput; }

  SHParametersInfo parameters() {
    static Parameters params{
        {"Operand", SHCCSTR("The big integer to compare against represented as bytes."), {CoreInfo::BytesVarType}}};
    return params;
  }

  ParamVar _op{};

  void setParam(int index, const SHVar &value) { _op = value; }

  SHVar getParam(int index) { return _op; }

  void cleanup(SHContext *context) { _op.cleanup(); }

  void warmup(SHContext *context) { _op.warmup(context); }

  const SHVar &getOperand() {
    SHVar &op = _op.get();
    if (op.valueType == SHType::None) {
      throw ActivationError("Operand is None, should be valid bigint bytes");
    }
    return op;
  }
};

struct RegOperandBase {
  std::vector<uint8_t> _buffer;

  static SHTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString inputHelp() { return BigIntDefaultHelpText::BigIntInputOutput; }
  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString outputHelp() { return SHCCSTR("Returns the BigInt result of the operation as bytes."); }

  SHParametersInfo parameters() {
    static Parameters params{
        {"Operand", SHCCSTR("The integer operand, can be a variable"), {CoreInfo::IntType, CoreInfo::IntVarType}}};
    return params;
  }

  ParamVar _op{};

  void setParam(int index, const SHVar &value) { _op = value; }

  SHVar getParam(int index) { return _op; }

  void cleanup(SHContext *context) { _op.cleanup(); }

  void warmup(SHContext *context) { _op.warmup(context); }

  const SHVar &getOperand() {
    SHVar &op = _op.get();
    if (op.valueType == SHType::None) {
      throw ActivationError("Operand is None, should be an integer");
    }
    return op;
  }
};

struct AddOp : public BigInt::BinaryOperation<BigIntBinaryOperation<Math::AddOp>> {
  static SHOptionalString help() {
    return SHCCSTR(
        "This shard adds the input big integer to the big integer specified in the Operand parameter and returns the result.");
  }

  SHParametersInfo parameters() {
    static Parameters params{
        {"Operand", SHCCSTR("The big integer to add to the input as bytes."), {CoreInfo::BytesVarType, CoreInfo::BytesVarSeqType}}};
    return params;
  }
};

struct SubtractOp : public BigInt::BinaryOperation<BigIntBinaryOperation<Math::SubtractOp>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard subtracts the input big integer from the big integer specified in the Operand parameter and "
                   "returns the result.");
  }

  SHParametersInfo parameters() {
    static Parameters params{
        {"Operand", SHCCSTR("The big integer to subtract from the input as bytes."), {CoreInfo::BytesVarType, CoreInfo::BytesVarSeqType}}};
    return params;
  }
};

struct MultiplyOp : public BigInt::BinaryOperation<BigIntBinaryOperation<Math::MultiplyOp>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard multiplies the input big integer with the big integer specified in the Operand parameter and "
                   "returns the result.");
  }

  SHParametersInfo parameters() {
    static Parameters params{
        {"Operand", SHCCSTR("The big integer to multiply the big integer input with."), {CoreInfo::BytesVarType, CoreInfo::BytesVarSeqType}}};
    return params;
  }
};

struct DivideOp : public BigInt::BinaryOperation<BigIntBinaryOperation<Math::DivideOp>> {
  static SHOptionalString help() {
    return SHCCSTR(
        "This shard divides the input big integer by the big integer specified in the Operand parameter and returns the result.");
  }

  SHParametersInfo parameters() {
    static Parameters params{
        {"Operand", SHCCSTR("The big integer to divide the big integer input with."), {CoreInfo::BytesVarType, CoreInfo::BytesVarSeqType}}};
    return params;
  }
};

struct XorOp : public BigInt::BinaryOperation<BigIntBinaryOperation<Math::XorOp>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard performs a bitwise XOR operation on the input big integer with the big integer specified in the "
                   "Operand parameter and returns the result. A bitwise XOR operation is a binary operation that compares each "
                   "bit of the binary representations of two numbers and returns 1 if the bits are different and 0 if they are "
                   "the same. The resulting number "
                   "is a big integer representation of the 1s and 0s concatenated.");
  }

  static SHOptionalString outputHelp() {
    return SHCCSTR("Returns the result of the XOR operation as a big integer represented as bytes.");
  }

  SHParametersInfo parameters() {
    static Parameters params{
        {"Operand", SHCCSTR("The second big integer to perform the XOR operation with."), {CoreInfo::BytesVarType, CoreInfo::BytesVarSeqType}}};
    return params;
  }
};

struct AndOp : public BigInt::BinaryOperation<BigIntBinaryOperation<Math::AndOp>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard performs a bitwise AND operation on the input big integer with the big integer specified in the "
                   "Operand parameter and returns the result. A bitwise AND operation is a binary operation that compares each "
                   "bit of the binary representations of two numbers and returns 1 if the bits are 1 and 0 otherwise. The "
                   "resulting number is a big integer "
                   "representation of the 1s and 0s concatenated.");
  }

  static SHOptionalString outputHelp() {
    return SHCCSTR("Returns the result of the AND operation as a big integer represented as bytes.");
  }

  SHParametersInfo parameters() {
    static Parameters params{
        {"Operand", SHCCSTR("The second big integer to perform the AND operation with."), {CoreInfo::BytesVarType, CoreInfo::BytesVarSeqType}}};
    return params;
  }
};

struct OrOp : public BigInt::BinaryOperation<BigIntBinaryOperation<Math::OrOp>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard performs a bitwise OR operation on the input big integer with the big integer specified in the "
                   "Operand parameter and returns the result. A bitwise OR operation is a binary operation that compares each "
                   "bit of the binary representations of two numbers and returns 1 if the either or both bits are 1 and 0 if "
                   "both bits are 0. The resulting number is a big integer "
                   "representation of the 1s and 0s concatenated.");
  }

  static SHOptionalString outputHelp() {
    return SHCCSTR("Returns the result of the OR operation as a big integer represented as bytes.");
  }

  SHParametersInfo parameters() {
    static Parameters params{
        {"Operand", SHCCSTR("The second big integer to perform the OR operation with."), {CoreInfo::BytesVarType, CoreInfo::BytesVarSeqType}}};
    return params;
  }
};

struct ModOp : public BigInt::BinaryOperation<BigIntBinaryOperation<Math::ModOp>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard computes the modulus of the input big integer with respect to the big integer specified in the "
                   "Operand parameter and returns the result.");
  }

  SHParametersInfo parameters() {
    static Parameters params{
        {"Operand", SHCCSTR("The big integer to compute the modulus with respect to."), {CoreInfo::BytesVarType, CoreInfo::BytesVarSeqType}}};
    return params;
  }
};

using Add = AddOp;
using Subtract = SubtractOp;
using Multiply = MultiplyOp;
using Divide = DivideOp;
using Xor = XorOp;
using And = AndOp;
using Or = OrOp;
using Mod = ModOp;

#define BIGINT_LOGIC_OP(__NAME__, __OP__, HELP_TEXT, OUTPUT_HELP_TEXT)         \
  struct __NAME__ : public BigOperandBase {                                    \
    static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }            \
    static SHOptionalString outputHelp() { return SHCCSTR(OUTPUT_HELP_TEXT); } \
    static SHOptionalString help() { return SHCCSTR(HELP_TEXT); }              \
                                                                               \
    SHVar activate(SHContext *context, const SHVar &input) {                   \
      cpp_int bia = from_var(input);                                           \
      auto op = getOperand();                                                  \
      cpp_int bib = from_var(op);                                              \
      bool res = bia __OP__ bib;                                               \
      return Var(res);                                                         \
    }                                                                          \
  }

BIGINT_LOGIC_OP(Is, ==, "This shard checks if the input big integer is equal to the operand.",
                "Returns true if the input big integer is equal to the big integer specified in the Operand parameter and false otherwise.");
BIGINT_LOGIC_OP(IsNot, !=, "This shard checks if the input big integer is not equal to the operand.",
                "Returns true if the input big integer is not equal to the big integer specified in the Operand parameter and false otherwise.");
BIGINT_LOGIC_OP(IsMore, >, "This shard checks if the input big integer is greater than the operand.",
                "Returns true if the input big integer is greater than the big integer specified in the Operand parameter and false otherwise.");
BIGINT_LOGIC_OP(IsLess, <, "This shard checks if the input big integer is less than the operand.",
                "Returns true if the input big integer is less than the big integer specified in the Operand parameter and false otherwise.");
BIGINT_LOGIC_OP(IsMoreEqual, >=, "This shard checks if the input big integer is greater than or equal to the operand.",
                "Returns true if the input big integer is greater than or equal to the big integer specified in the Operand parameter and false otherwise.");
BIGINT_LOGIC_OP(IsLessEqual, <=, "This shard checks if the input big integer is less than or equal to the operand.",
                "Returns true if the input big integer is less than or equal to the big integer specified in the Operand parameter and false otherwise.");

struct MinOp final {
  template <typename T> T apply(const T &a, const T &b) { return std::min(a, b); }
};

struct MaxOp final {
  template <typename T> T apply(const T &a, const T &b) { return std::max(a, b); }
};

struct Min : public BigInt::BinaryOperation<BigIntBinaryOperation<MinOp>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard compares the input big integer with the big integer specified in the Operand parameter and "
                   "returns the smaller of the two.");
  }
  static SHOptionalString outputHelp() { return SHCCSTR("Returns the smaller of the two input big integers."); }

  SHParametersInfo parameters() {
    static Parameters params{
        {"Operand", SHCCSTR("The big integer to compare the input with."), {CoreInfo::BytesVarType, CoreInfo::BytesVarSeqType}}};
    return params;
  }
};

struct Max : public BigInt::BinaryOperation<BigIntBinaryOperation<MaxOp>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard compares the input big integer with the big integer specified in the Operand parameter and "
                   "returns the larger of the two.");
  }
  static SHOptionalString outputHelp() { return SHCCSTR("Returns the larger of the two input big integers."); }

  SHParametersInfo parameters() {
    static Parameters params{
        {"Operand", SHCCSTR("The big integer to compare the input with."), {CoreInfo::BytesVarType, CoreInfo::BytesVarSeqType}}};
    return params;
  }
};

#define BIGINT_REG_BINARY_OP(__NAME__, __OP__)                                                                                 \
  struct __NAME__ : public RegOperandBase {                                                                                    \
    static SHOptionalString help() {                                                                                           \
      return SHCCSTR("This shard raises the input big integer to the power of the exponent specified in " \
                     "the  Operand parameter.");                                                                               \
    }                                                                                                                          \
    SHParametersInfo parameters() {                                                                                            \
      static Parameters params{                                                                                                \
          {"Operand",                                                                                                          \
           SHCCSTR("The power to which the input big integer will be raised. This must be a non-negative integer."),           \
           {CoreInfo::IntType, CoreInfo::IntVarType}}};                                                                        \
      return params;                                                                                                           \
    }                                                                                                                          \
    SHVar activate(SHContext *context, const SHVar &input) {                                                                   \
      cpp_int bia = from_var(input);                                                                                           \
      auto op = getOperand();                                                                                                  \
      if (op.valueType != SHType::Int)                                                                                         \
        throw ActivationError("Pow operand should be an Int");                                                                 \
      cpp_int bres = __OP__(bia, op.payload.intValue);                                                                         \
      return to_var(bres, _buffer);                                                                                            \
    }                                                                                                                          \
  }

BIGINT_REG_BINARY_OP(Pow, pow);

#define BIGINT_UNARY_OP(__NAME__, __OP__)                                                                   \
  struct __NAME__ : public RegOperandBase {                                                                 \
    static SHOptionalString help() {                                                                        \
      return SHCCSTR("This shard computes the square root of the input big integer represented as bytes."); \
    }                                                                                                       \
    static SHOptionalString outputHelp() {                                                                  \
      return SHCCSTR("Returns the square root of the input big integer represented as bytes.");             \
    }                                                                                                       \
    SHParametersInfo parameters() { return {}; }                                                            \
    SHVar activate(SHContext *context, const SHVar &input) {                                                \
      cpp_int bia = from_var(input);                                                                        \
      cpp_int bres = __OP__(bia);                                                                           \
      return to_var(bres, _buffer);                                                                         \
    }                                                                                                       \
  }

BIGINT_UNARY_OP(Sqrt, sqrt);

struct ShiftBase {
  ParamVar _shift{Var(0)};

  void setParam(int index, const SHVar &value) { _shift = value; }

  SHVar getParam(int index) { return _shift; }

  void cleanup(SHContext *context) { _shift.cleanup(); }

  void warmup(SHContext *context) { _shift.warmup(context); }
};

struct Shift : public ShiftBase {
  std::vector<uint8_t> _buffer;

  static SHTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString inputHelp() { return BigIntDefaultHelpText::BigIntInputOutput; }
  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("Returns the big integer with its decimal point shifted represented as bytes.");
  }

  static SHOptionalString help() {
    return SHCCSTR("This shard shifts the decimal point of the input big integer by the amount specified in the By parameter.");
  }

  SHParametersInfo parameters() {
    static Parameters params{{"By",
                              SHCCSTR("The number of decimal places to shift the input big integer by. A positive number shifts "
                                      "the decimal point to the right, while a negative number shifts it to the left."),
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
  static SHOptionalString help() {
    return SHCCSTR("This shard converts the input big integer value to a floating point number.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The big integer to convert, represented as bytes."); }

  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }
  static SHOptionalString outputHelp() { return SHCCSTR("Floating point number representation of the big integer value."); }

  SHParametersInfo parameters() {
    static Parameters params{{"ShiftedBy",
                              SHCCSTR("The number of decimal places to shift the input big integer by. A positive number shifts "
                                      "the decimal point to the right, while a negative number shifts it to the left."),
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
  static SHOptionalString help() { return SHCCSTR("This shard converts the input big integer value to an integer."); }

  static SHTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The big integer to convert, represented as bytes."); }

  static SHTypesInfo outputTypes() { return CoreInfo::IntType; }
  static SHOptionalString outputHelp() { return SHCCSTR("Integer representation of the big integer value."); }

  SHVar activate(SHContext *context, const SHVar &input) {
    cpp_int bi = from_var(input);
    return Var(bi.convert_to<int64_t>());
  }
};

struct FromFloat : public ShiftBase {
  static SHOptionalString help() { return SHCCSTR("This shard converts a floating point number to a big integer."); }

  static SHTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static SHOptionalString inputHelp() { return SHCCSTR("Floating point number."); }

  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString outputHelp() { return BigIntDefaultHelpText::BigIntInputOutput; }

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
  static SHOptionalString inputHelp() { return BigIntDefaultHelpText::BigIntInputOutput; }
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

  static SHOptionalString help() {
    return SHCCSTR("This shard converts a big integer value into a bytes representaion with the specified bit length set in the "
                   "Bits parameter. If set to 0, the output will be the minimum number of bytes required to represent the big "
                   "integer. If set to a positive number, the output will be padded with leading zeros to match the specified "
                   "bit length. An error is thrown if the number requires more bits than specified.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString inputHelp() { return SHCCSTR("Big integer to convert, represented as bytes."); }
  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }

  SHParametersInfo parameters() {
    static Parameters params{{"Bits",
                              SHCCSTR("The desired bit length for the output to take. If set to 0, the output will be the "
                                      "minimum number of bytes required to represent the big integer."),
                              {CoreInfo::IntType}}};
    return params;
  }

  ParamVar _bits{Var(0)};

  void setParam(int index, const SHVar &value) { _bits = value; }

  SHVar getParam(int index) { return _bits; }

  void warmup(SHContext *context) { _bits.warmup(context); }
  void cleanup(SHContext *context) { _bits.cleanup(); }

  SHVar activate(SHContext *context, const SHVar &input) {
    const int64_t bits = _bits.get().payload.intValue;
    if (bits <= 0) {
      SHVar fixedInput = input;
      fixedInput.payload.bytesValue++;
      fixedInput.payload.bytesSize--;
      return fixedInput;
    } else {
      cpp_int bi = from_var(input);
      const auto usedBits = msb(bi) + 1;
      if (int64_t(usedBits) > bits) {
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
  static SHOptionalString help() {
    return SHCCSTR("This shard converts the input big integer value to its hexadecimal representation.");
  }

  static inline Types toHexTypes{CoreInfo::IntType, CoreInfo::BytesType, CoreInfo::StringType};
  static SHTypesInfo inputTypes() { return toHexTypes; }
  static SHOptionalString inputHelp() { return SHCCSTR("Big integer to convert, represented as bytes."); }

  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("Returns the hexadecimal representation of the input big integer value.");
  }

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
  static SHOptionalString help() { return SHCCSTR("This shard computes the absolute value of a big integer."); }

  static SHTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString inputHelp() { return BigIntDefaultHelpText::BigIntInputOutput; }

  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The resulting big integer with an absolute value, represented as bytes."); }

  SHVar activate(SHContext *context, const SHVar &input) {
    cpp_int bi = from_var(input);
    cpp_int abi = abs(bi);
    return to_var(abi, _buffer);
  }

private:
  std::vector<uint8_t> _buffer;
};
} // namespace BigInt

SHARDS_REGISTER_FN(bigint) {
  REGISTER_SHARD("BigInt", BigInt::ToBigInt);
  REGISTER_SHARD("BigInt.Add", BigInt::Add);
  REGISTER_SHARD("BigInt.Subtract", BigInt::Subtract);
  REGISTER_SHARD("BigInt.Multiply", BigInt::Multiply);
  REGISTER_SHARD("BigInt.Divide", BigInt::Divide);
  REGISTER_SHARD("BigInt.Xor", BigInt::Xor);
  REGISTER_SHARD("BigInt.And", BigInt::And);
  REGISTER_SHARD("BigInt.Or", BigInt::Or);
  REGISTER_SHARD("BigInt.Mod", BigInt::Mod);
  REGISTER_SHARD("BigInt.Shift", BigInt::Shift);
  REGISTER_SHARD("BigInt.ToFloat", BigInt::ToFloat);
  REGISTER_SHARD("BigInt.ToInt", BigInt::ToInt);
  REGISTER_SHARD("BigInt.FromFloat", BigInt::FromFloat);
  REGISTER_SHARD("BigInt.ToString", BigInt::ToString);
  REGISTER_SHARD("BigInt.ToBytes", BigInt::ToBytes);
  REGISTER_SHARD("BigInt.ToHex", BigInt::ToHex);
  REGISTER_SHARD("BigInt.Is", BigInt::Is);
  REGISTER_SHARD("BigInt.IsNot", BigInt::IsNot);
  REGISTER_SHARD("BigInt.IsMore", BigInt::IsMore);
  REGISTER_SHARD("BigInt.IsLess", BigInt::IsLess);
  REGISTER_SHARD("BigInt.IsMoreEqual", BigInt::IsMoreEqual);
  REGISTER_SHARD("BigInt.IsLessEqual", BigInt::IsLessEqual);
  REGISTER_SHARD("BigInt.Min", BigInt::Min);
  REGISTER_SHARD("BigInt.Max", BigInt::Max);
  REGISTER_SHARD("BigInt.Pow", BigInt::Pow);
  REGISTER_SHARD("BigInt.Abs", BigInt::Abs);
  REGISTER_SHARD("BigInt.Sqrt", BigInt::Sqrt);
}

} // namespace shards
