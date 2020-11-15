/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#include "shared.hpp"
#include <random>

namespace chainblocks {
namespace Random {
struct RandBase {
  static inline thread_local std::random_device _rd{};
  static inline thread_local std::mt19937 _gen{_rd()};
  static inline thread_local std::uniform_int_distribution<> _uintdis{};
  static inline thread_local std::uniform_real_distribution<> _udis{0.0, 1.0};
};

template <Type &OUTTYPE, CBType CBTYPE> struct Rand : public RandBase {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return OUTTYPE; }
  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, CBVar value) { _max = value; }

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
       "The maximum (if integer, not including) value to output.",
       {CoreInfo::NoneType, OUTTYPE}}

  };
}; // namespace Random

using RandomInt = Rand<CoreInfo::IntType, CBType::Int>;
using RandomFloat = Rand<CoreInfo::FloatType, CBType::Float>;

void registerBlocks() {
  REGISTER_CBLOCK("RandomInt", RandomInt);
  REGISTER_CBLOCK("RandomFloat", RandomFloat);
}
} // namespace Random
} // namespace chainblocks
