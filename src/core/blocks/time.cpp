/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2021 Giovanni Petrantoni */

#include "shared.hpp"
#include <chrono>

namespace chainblocks {
namespace Time {
struct ProcessClock {
  decltype(std::chrono::high_resolution_clock::now()) Start;
  ProcessClock() { Start = std::chrono::high_resolution_clock::now(); }
};

struct Now {
  static inline ProcessClock _clock{};

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::FloatType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto tnow = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> dt = tnow - _clock.Start;
    return Var(dt.count());
  }
};

struct NowMs : public Now {
  CBVar activate(CBContext *context, const CBVar &input) {
    auto tnow = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> dt = tnow - _clock.Start;
    return Var(dt.count());
  }
};

struct Delta {
  ProcessClock _clock{};

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::FloatType; }

  void warmup(CBContext *context) {
    _clock.Start = std::chrono::high_resolution_clock::now();
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto tnow = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> dt = tnow - _clock.Start;
    _clock.Start = tnow; // reset timer
    return Var(dt.count());
  }
};

struct DeltaMs : public Delta {
  CBVar activate(CBContext *context, const CBVar &input) {
    auto tnow = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> dt = tnow - _clock.Start;
    _clock.Start = tnow; // reset timer
    return Var(dt.count());
  }
};

struct EpochMs {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::IntType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    using namespace std::chrono;
    milliseconds ms =
        duration_cast<milliseconds>(system_clock::now().time_since_epoch());
    return Var(ms.count());
  }
};

struct Pop {
  static inline Types PSeqTypes{{CoreInfo::AnyType, CoreInfo::IntType}};
  static inline Type PSeqType = Type::SeqOf(PSeqTypes);
  static inline Type VPSeqType = Type::VariableOf(PSeqTypes);
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static inline Parameters Params{
      {"Sequence",
       CBCCSTR("A variables sequence of pairs [value "
               "pop-epoch-time-ms] with types [Any Int]"),
       {VPSeqType, CoreInfo::NoneType}}};

  CBOptionalString help() {
    return CBCCSTR("This blocks delays its output until one of the values of "
                   "the sequence parameter expires.");
  }

  ParamVar _pseq{};

  CBParametersInfo parameters() { return Params; }

  void setParam(int index, const CBVar &value) { _pseq = value; }

  CBVar getParam(int index) { return _pseq; }

  CBTypeInfo compose(const CBInstanceData &data) {
    if (!_pseq.isVariable())
      throw ComposeError("Time.Pop expects a variable");

    for (auto info : data.shared) {
      if (strcmp(info.name, _pseq.variableName()) == 0) {
        if (info.exposedType.basicType != Seq ||
            info.exposedType.seqTypes.len != 2)
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

  void warmup(CBContext *context) { _pseq.warmup(context); }

  void cleanup() { _pseq.cleanup(); }

  CBVar activate(CBContext *context, const CBVar &input) {
    using namespace std::chrono;
    while (true) {
      auto &seq = _pseq.get();
      milliseconds ms =
          duration_cast<milliseconds>(system_clock::now().time_since_epoch());
      auto now = ms.count();
      for (uint32_t idx = 0; idx < seq.payload.seqValue.len; idx++) {
        auto &v = seq.payload.seqValue.elements[idx];
        auto &time = v.payload.seqValue.elements[1]; // time ms in epoch here
        if (now >= time.payload.intValue) {
          // remove the item fast as order does not matter, time deadline is the
          // only thing that matters)
          arrayDelFast(seq.payload.seqValue, idx);
          return v.payload.seqValue.elements[0];
        }
      }
      // if we are here means we need to wait
      CB_SUSPEND(context, 0);
    }
  }
};
} // namespace Time

void registerTimeBlocks() {
  REGISTER_CBLOCK("Time.Now", Time::Now);
  REGISTER_CBLOCK("Time.NowMs", Time::NowMs);
  REGISTER_CBLOCK("Time.Delta", Time::Delta);
  REGISTER_CBLOCK("Time.DeltaMs", Time::DeltaMs);
  REGISTER_CBLOCK("Time.EpochMs", Time::EpochMs);
  REGISTER_CBLOCK("Time.Pop", Time::Pop);
}
} // namespace chainblocks
