#ifndef DC23C9EE_D7BB_4D93_94B5_F211AD1F898A
#define DC23C9EE_D7BB_4D93_94B5_F211AD1F898A

#include <chrono>

namespace shards::Time {

using Clock = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;
using DoubleSecDuration = std::chrono::duration<double>;

struct ProcessClock {
  TimePoint Start;
  ProcessClock() { Start = Clock::now(); }
};

using namespace std::chrono_literals;
struct DeltaTimer {

  // Limit delta time to avoid jumps after unpausing wires
  static inline const auto MaxDeltaTime = DoubleSecDuration(1.0f / 15.0f);

  TimePoint lastActivation;

  void reset() { lastActivation = Clock::now(); }

  template <typename TDur = DoubleSecDuration> typename TDur::rep update() {
    auto now = Clock::now();
    // TDur delta = std::min<TDur>(std::chrono::duration_cast<TDur>(MaxDeltaTime), (now - lastActivation));
    TDur delta = now - lastActivation;
    lastActivation = now;
    return delta.count();
  }
};
}; // namespace shards::Time

#endif /* DC23C9EE_D7BB_4D93_94B5_F211AD1F898A */
