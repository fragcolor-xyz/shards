/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "shared.hpp"
#include <chrono>

namespace shards {
namespace Time {
struct ProcessClock {
  decltype(std::chrono::high_resolution_clock::now()) Start;
  ProcessClock() { Start = std::chrono::high_resolution_clock::now(); }
};

struct Now {
  static inline ProcessClock _clock{};

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto tnow = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> dt = tnow - _clock.Start;
    return Var(dt.count());
  }
};

struct NowMs : public Now {
  SHVar activate(SHContext *context, const SHVar &input) {
    auto tnow = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> dt = tnow - _clock.Start;
    return Var(dt.count());
  }
};

struct Delta {
  ProcessClock _clock{};

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }

  void warmup(SHContext *context) { _clock.Start = std::chrono::high_resolution_clock::now(); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto tnow = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> dt = tnow - _clock.Start;
    _clock.Start = tnow; // reset timer
    return Var(dt.count());
  }
};

struct DeltaMs : public Delta {
  SHVar activate(SHContext *context, const SHVar &input) {
    auto tnow = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> dt = tnow - _clock.Start;
    _clock.Start = tnow; // reset timer
    return Var(dt.count());
  }
};

struct EpochMs {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::IntType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    using namespace std::chrono;
    milliseconds ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
    return Var(int(ms.count()));
  }
};

struct Pop {
  static inline Types PSeqTypes{{CoreInfo::AnyType, CoreInfo::IntType}};
  static inline Type PSeqType = Type::SeqOf(PSeqTypes);
  static inline Type VPSeqType = Type::VariableOf(PSeqTypes);
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static inline Parameters Params{{"Sequence",
                                   SHCCSTR("A variables sequence of pairs [value "
                                           "pop-epoch-time-ms] with types [Any Int]"),
                                   {VPSeqType, CoreInfo::NoneType}}};

  SHOptionalString help() {
    return SHCCSTR("This shards delays its output until one of the values of "
                   "the sequence parameter expires.");
  }

  ParamVar _pseq{};
  OwnedVar _last{};

  SHParametersInfo parameters() { return Params; }

  void setParam(int index, const SHVar &value) { _pseq = value; }

  SHVar getParam(int index) { return _pseq; }

  SHTypeInfo compose(const SHInstanceData &data) {
    if (!_pseq.isVariable())
      throw ComposeError("Time.Pop expects a variable");

    for (auto info : data.shared) {
      if (strcmp(info.name, _pseq.variableName()) == 0) {
        if (info.exposedType.basicType != Seq || info.exposedType.seqTypes.len != 2)
          throw ComposeError("Time.Pop expected a sequence of pairs");

        if (!info.isMutable)
          throw ComposeError("Time.Pop expects a mutable sequence variable");

        if (info.exposedType.seqTypes.elements[1] != CoreInfo::IntType)
          throw ComposeError("Time.Pop expects a pair of [Any Int] values");

        return info.exposedType.seqTypes.elements[0];
      }
    }

    throw ComposeError("Time.Pop variable not found");
  }

  void warmup(SHContext *context) { _pseq.warmup(context); }

  void cleanup() { _pseq.cleanup(); }

  SHVar activate(SHContext *context, const SHVar &input) {
    using namespace std::chrono;
    while (true) {
      auto &seq = _pseq.get();
      milliseconds ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
      auto now = ms.count();
      for (uint32_t idx = 0; idx < seq.payload.seqValue.len; idx++) {
        auto &v = seq.payload.seqValue.elements[idx];
        auto &time = v.payload.seqValue.elements[1]; // time ms in epoch here
        if (now >= time.payload.intValue) {
          _last = v.payload.seqValue.elements[0];
          // remove the item fast as order does not matter, time deadline is the
          // only thing that matters)
          arrayDelFast(seq.payload.seqValue, idx);
          return _last;
        }
      }
      // if we are here means we need to wait
      SH_SUSPEND(context, 0);
    }
  }
};
} // namespace Time

void registerTimeShards() {
  REGISTER_SHARD("Time.Now", Time::Now);
  REGISTER_SHARD("Time.NowMs", Time::NowMs);
  REGISTER_SHARD("Time.Delta", Time::Delta);
  REGISTER_SHARD("Time.DeltaMs", Time::DeltaMs);
  REGISTER_SHARD("Time.EpochMs", Time::EpochMs);
  REGISTER_SHARD("Time.Pop", Time::Pop);
}
} // namespace shards
