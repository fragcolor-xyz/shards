#ifndef CB_RANDOM_HPP
#define CB_RANDOM_HPP

#include "random.hpp"
#include "shared.hpp"

namespace chainblocks {
namespace Random {
struct Base {
  static double frand() {
    return double(_gen()) * (1.0 / double(xorshift::max()));
  }

  static double frand(double max) { return frand() * max; }

  static int64_t rand() { return int64_t(_gen()); }

  static int64_t rand(int64_t max) { return rand() % max; }

private:
#ifdef NDEBUG
  static inline thread_local std::random_device _rd{};
  static inline thread_local xorshift _gen{_rd};
#else
  static inline thread_local xorshift _gen{};
#endif
};

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
        res.payload.intValue = Base::rand();
      else
        res.payload.intValue = Base::rand(_max.payload.intValue);
    } else if constexpr (CBTYPE == CBType::Float) {
      if (_max.valueType == None)
        res.payload.floatValue = Base::frand();
      else
        res.payload.floatValue = Base::frand(_max.payload.floatValue);
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
