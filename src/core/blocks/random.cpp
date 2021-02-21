/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2021 Giovanni Petrantoni */

#include "shared.hpp"
#include <random>

namespace chainblocks {
namespace Random {
struct RandBase {
  static inline std::random_device _rd{}; // don't make it thread_local!
  static inline thread_local std::mt19937 _gen{_rd()};
  static inline thread_local std::uniform_int_distribution<> _uintdis{};
  static inline thread_local std::uniform_real_distribution<> _udis{0.0, 1.0};
};

template <Type &OUTTYPE, CBType CBTYPE> struct Rand : public RandBase {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return OUTTYPE; }
  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) { _max = value; }

  CBVar getParam(int index) { return _max; }

  CBVar activate(CBContext *context, const CBVar &input) {
    CBVar res{};
    res.valueType = CBTYPE;
    if constexpr (CBTYPE == CBType::Int) {
      if (_max.valueType == None)
        res.payload.intValue = _uintdis(_gen);
      else
        res.payload.intValue = _uintdis(_gen) % _max.payload.intValue;
    } else if constexpr (CBTYPE == CBType::Float) {
      if (_max.valueType == None)
        res.payload.floatValue = _udis(_gen);
      else
        res.payload.floatValue = _udis(_gen) * _max.payload.floatValue;
    }
    return res;
  }

private:
  CBVar _max{};
  static inline Parameters _params{
      {"Max",
       CBCCSTR("The maximum (if integer, not including) value to output."),
       {CoreInfo::NoneType, OUTTYPE}}};
};

using RandomInt = Rand<CoreInfo::IntType, CBType::Int>;
using RandomFloat = Rand<CoreInfo::FloatType, CBType::Float>;

struct RandomBytes : public RandBase {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BytesType; }
  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
    _size = value.payload.intValue;
  }

  CBVar getParam(int index) { return Var(_size); }

  CBVar activate(CBContext *context, const CBVar &input) {
    _buffer.resize(_size);
    for (int64_t i = 0; i < _size; i++) {
      _buffer[i] = _uintdis(_gen) % 256;
    }
    return Var(_buffer.data(), _size);
  }

private:
  int64_t _size{32};
  std::vector<uint8_t> _buffer;
  static inline Parameters _params{
      {"Size", CBCCSTR("The amount of bytes to output."), {CoreInfo::IntType}}};
};

void registerBlocks() {
  REGISTER_CBLOCK("RandomInt", RandomInt);
  REGISTER_CBLOCK("RandomFloat", RandomFloat);
  REGISTER_CBLOCK("RandomBytes", RandomBytes);
}
} // namespace Random
} // namespace chainblocks
