/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#include "chainblocks.hpp"
#include "shared.hpp"

#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>

using namespace boost::multiprecision;

namespace chainblocks {
namespace BigInt {
struct ToBigInt {
  std::vector<uint8_t> _buffer;

  static inline Types InputTypes{CoreInfo::IntType, CoreInfo::StringType};
  static CBTypesInfo inputTypes() { return InputTypes; }
  static CBTypesInfo outputTypes() { return CoreInfo::BytesType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    _buffer.clear();
    cpp_int bi;
    switch (input.valueType) {
    case Int: {
      bi = input.payload.intValue;
    } break;
    case String: {
      bi = cpp_int(input.payload.stringValue);
    } break;
    default: {
      throw ActivationError("Invalid input type");
    }
    }
    export_bits(bi, std::back_inserter(_buffer), 8);
    return Var(&_buffer.front(), _buffer.size());
  }
};

struct Add {};

struct Subtract {};

struct Multiply {};

struct Divide {};

struct Mod {};

struct Exp10 {
  std::vector<uint8_t> _buffer;

  static CBTypesInfo inputTypes() { return CoreInfo::IntType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BytesType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    cpp_int bi = input.payload.intValue;
    cpp_dec_float_100 bf(bi);
    bf = pow(cpp_dec_float_100(10), bf);
    bi = cpp_int(bf);
    export_bits(bi, std::back_inserter(_buffer), 8);
    return Var(&_buffer.front(), _buffer.size());
  }
};

struct ToFloat {
  static CBTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static CBTypesInfo outputTypes() { return CoreInfo::FloatType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    cpp_int bi;
    import_bits(bi, input.payload.bytesValue,
                input.payload.bytesValue + input.payload.bytesSize);
    cpp_dec_float_100 bf(bi);
    return Var(bi.convert_to<double>());
  }
}; // shiftedBy(Exp10)

void registerBlocks() {
  REGISTER_CBLOCK("BigInt", ToBigInt);
  REGISTER_CBLOCK("BigInt.Exp10", Exp10);
  REGISTER_CBLOCK("BigInt.ToFloat", ToFloat);
}
} // namespace BigInt
} // namespace chainblocks