/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#include "chainblocks.hpp"
#include "shared.hpp"

#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>

using namespace boost::multiprecision;

namespace chainblocks {
namespace BigNumber {
struct ToBigNumber {
  std::vector<uint8_t> _buffer;

  static CBTypesInfo inputTypes() { return CoreInfo::IntType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BytesType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    _buffer.clear();
    cpp_int bi = input.payload.intValue;
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
    _buffer.clear();
    cpp_int bi = input.payload.intValue;
    cpp_dec_float_50 bf(bi);
    auto bfexp10 = exp(log10(bf));
    cpp_int ri(bfexp10);
    export_bits(ri, std::back_inserter(_buffer), 8);
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
    LOG(DEBUG) << "ToFloat input: " << bi;
    cpp_dec_float_50 bf(bi);
    LOG(DEBUG) << "ToFloat to float: " << bf;
    return Var(bi.convert_to<double>());
  }
}; // shiftedBy(Exp10)

void registerBlocks() {
  REGISTER_CBLOCK("BigNumber", ToBigNumber);
  REGISTER_CBLOCK("BigNumber.Exp10", Exp10);
  REGISTER_CBLOCK("BigNumber.ToFloat", ToFloat);
}
} // namespace BigNumber
} // namespace chainblocks