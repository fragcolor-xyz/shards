#ifndef GFX_LOOP
#define GFX_LOOP

#include <chrono>

namespace gfx {

struct Loop {
  using Duration = std::chrono::high_resolution_clock::duration;
  using TimePoint = std::chrono::high_resolution_clock::time_point;
  using Clock = std::chrono::high_resolution_clock;

  TimePoint startTime;
  TimePoint lastFrameTime;
  bool haveFirstFrame = false;

  Loop();
  bool beginFrame(double targetDeltaTime, double &deltaTime);
  double getAbsoluteTime() const;
};
inline Loop::Loop() : startTime(Clock::now()) {}
inline bool Loop::beginFrame(double targetDeltaTime, double &outDeltaTime) {
  TimePoint now = Clock::now();
  outDeltaTime = std::chrono::duration<double>(now - lastFrameTime).count();
  if (!haveFirstFrame) {
    outDeltaTime = 0.0f;
    lastFrameTime = now;
    haveFirstFrame = true;
    return true;
  } else if (outDeltaTime >= targetDeltaTime) {
    lastFrameTime = now;
    return true;
  }
  return false;
}

inline double Loop::getAbsoluteTime() const {
  TimePoint now = Clock::now();
  return std::chrono::duration<double>(now - startTime).count();
}
} // namespace gfx

#endif // GFX_LOOP
