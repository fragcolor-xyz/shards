#pragma once
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
  bool beginFrame(float targetDeltaTime, float &deltaTime);
  float getAbsoluteTime() const;
};
} // namespace gfx
