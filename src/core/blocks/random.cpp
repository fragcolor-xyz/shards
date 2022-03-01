/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

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

  void cleanup() { _max.cleanup(); }

  void warmup(CBContext *context) { _max.warmup(context); }

  CBVar activate(CBContext *context, const CBVar &input) {
    CBVar res{};
    res.valueType = CBTYPE;

    auto max = _max.get();
    if constexpr (CBTYPE == CBType::Int) {
      if (max.valueType == None)
        res.payload.intValue = _uintdis(_gen);
      else
        res.payload.intValue = _uintdis(_gen) % max.payload.intValue;
    } else if constexpr (CBTYPE == CBType::Float) {
      if (max.valueType == None)
        res.payload.floatValue = _udis(_gen);
      else
        res.payload.floatValue = _udis(_gen) * max.payload.floatValue;
    }
    return res;
  }

private:
  static inline Parameters _params{{"Max",
                                    CBCCSTR("The maximum (if integer, not including) value to output."),
                                    {CoreInfo::NoneType, OUTTYPE, Type::VariableOf(OUTTYPE)}}};
  ParamVar _max{};
};

using RandomInt = Rand<CoreInfo::IntType, CBType::Int>;
using RandomFloat = Rand<CoreInfo::FloatType, CBType::Float>;

struct RandomBytes : public RandBase {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BytesType; }
  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) { _size = value.payload.intValue; }

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
  static inline Parameters _params{{"Size", CBCCSTR("The amount of bytes to output."), {CoreInfo::IntType}}};
};

void registerBlocks() {
  REGISTER_CBLOCK("RandomInt", RandomInt);
  REGISTER_CBLOCK("RandomFloat", RandomFloat);
  REGISTER_CBLOCK("RandomBytes", RandomBytes);
}
} // namespace Random
} // namespace chainblocks
