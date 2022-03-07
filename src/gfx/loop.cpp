#include "loop.hpp"
#include <chrono>

namespace gfx {
Loop::Loop() : startTime(Clock::now()) {}

bool Loop::beginFrame(float targetDeltaTime, float &outDeltaTime) {
  TimePoint now = Clock::now();
  outDeltaTime = std::chrono::duration<float>(now - lastFrameTime).count();
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

float Loop::getAbsoluteTime() const {
  TimePoint now = Clock::now();
  return std::chrono::duration<float>(now - startTime).count();
}
} // namespace gfx