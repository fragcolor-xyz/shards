#pragma once
#include <vector>

namespace gfx {

struct MovingAverage {
  std::vector<float> buffer;
  size_t length;
  size_t maxLength;
  size_t offset;

  inline MovingAverage(size_t length);
  inline void add(float value);
  inline void reset();
  inline float getAverage() const;
};

MovingAverage::MovingAverage(size_t inLength) {
  length = 0;
  maxLength = inLength;
  buffer.resize(maxLength);
  offset = 0;
}

void MovingAverage::add(float value) {
  if (length < maxLength) {
    buffer[offset] = value;
    ++length;
  } else {
    buffer[offset] = value;
  }
  offset = (offset + 1) % maxLength;
}

void MovingAverage::reset() {
  length = 0;
  offset = 0;
}

float MovingAverage::getAverage() const {
  if (length == 0)
    return 0.0f;
  float sum = 0.0f;
  for (int i = 0; i < length; i++) {
    sum += buffer[i];
  }
  return sum / float(length);
}

} // namespace gfx