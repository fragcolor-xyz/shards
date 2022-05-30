/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

#include "shared.hpp"
#include <random>

namespace shards {
namespace Random {
struct RandBase {
  static inline std::random_device _rd{}; // don't make it thread_local!
  static inline thread_local std::mt19937 _gen{_rd()};
  static inline thread_local std::uniform_int_distribution<> _uintdis{};
  static inline thread_local std::uniform_real_distribution<> _udis{0.0, 1.0};
};

template <Type &OUTTYPE, SHType SHTYPE> struct Rand : public RandBase {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return OUTTYPE; }
  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) { _max = value; }

  SHVar getParam(int index) { return _max; }

  void cleanup() { _max.cleanup(); }

  void warmup(SHContext *context) { _max.warmup(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    SHVar res{};
    res.valueType = SHTYPE;

    auto max = _max.get();
    if constexpr (SHTYPE == SHType::Int) {
      if (max.valueType == None)
        res.payload.intValue = _uintdis(_gen);
      else
        res.payload.intValue = _uintdis(_gen) % max.payload.intValue;
    } else if constexpr (SHTYPE == SHType::Float) {
      if (max.valueType == None)
        res.payload.floatValue = _udis(_gen);
      else
        res.payload.floatValue = _udis(_gen) * max.payload.floatValue;
    }
    return res;
  }

private:
  static inline Parameters _params{{"Max",
                                    SHCCSTR("The maximum (if integer, not including) value to output."),
                                    {CoreInfo::NoneType, OUTTYPE, Type::VariableOf(OUTTYPE)}}};
  ParamVar _max{};
};

using RandomInt = Rand<CoreInfo::IntType, SHType::Int>;
using RandomFloat = Rand<CoreInfo::FloatType, SHType::Float>;

struct RandomBytes : public RandBase {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }
  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) { _size = value.payload.intValue; }

  SHVar getParam(int index) { return Var(_size); }

  SHVar activate(SHContext *context, const SHVar &input) {
    _buffer.resize(_size);
    for (int64_t i = 0; i < _size; i++) {
      _buffer[i] = _uintdis(_gen) % 256;
    }
    return Var(_buffer.data(), _size);
  }

private:
  int64_t _size{32};
  std::vector<uint8_t> _buffer;
  static inline Parameters _params{{"Size", SHCCSTR("The amount of bytes to output."), {CoreInfo::IntType}}};
};

void registerShards() {
  REGISTER_SHARD("RandomInt", RandomInt);
  REGISTER_SHARD("RandomFloat", RandomFloat);
  REGISTER_SHARD("RandomBytes", RandomBytes);
}
} // namespace Random
} // namespace shards
