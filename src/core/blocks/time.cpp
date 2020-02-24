/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

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

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    auto tnow = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> dt = tnow - _clock.Start;
    return Var(dt.count());
  }
};

struct NowMs : public Now {
  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    auto tnow = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> dt = tnow - _clock.Start;
    return Var(dt.count());
  }
};
} // namespace Time

void registerTimeBlocks() {
  REGISTER_CBLOCK("Time.Now", Time::Now);
  REGISTER_CBLOCK("Time.NowMs", Time::NowMs);
}
} // namespace chainblocks
