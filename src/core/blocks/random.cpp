#ifndef CB_RANDOM_HPP
#define CB_RANDOM_HPP

#include "random.hpp"
#include "shared.hpp"

namespace chainblocks {
namespace Random {
using Rng = Rng<void>;
template <Type &OUTTYPE, CBType CBTYPE> struct Rand {
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
        res.payload.intValue = Rng::rand();
      else
        res.payload.intValue = Rng::rand(_max.payload.intValue);
    } else if constexpr (CBTYPE == CBType::Float) {
      if (_max.valueType == None)
        res.payload.floatValue = Rng::frand();
      else
        res.payload.floatValue = Rng::frand(_max.payload.floatValue);
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

#endif /* CB_RANDOM_HPP */
