#ifndef GFX_MOVING_AVERAGE
#define GFX_MOVING_AVERAGE

#include <vector>

namespace gfx {

template <typename T = float> struct MovingAverage {
  std::vector<T> buffer;
  size_t length;
  size_t maxLength;
  size_t offset;

  MovingAverage(size_t inLength) {
    length = 0;
    maxLength = inLength;
    buffer.resize(maxLength);
    offset = 0;
  }

  void add(T value) {
    if (length < maxLength) {
      buffer[offset] = value;
      ++length;
    } else {
      buffer[offset] = value;
    }
    offset = (offset + 1) % maxLength;
  }

  void reset() {
    length = 0;
    offset = 0;
  }

  T getAverage() const {
    if (length == 0)
      return T(0);
    T sum(0);
    for (int i = 0; i < length; i++) {
      sum += buffer[i];
    }
    return T(sum / double(length));
  }

  T getMax(T init = 0) const {
    T maxValue = init;
    if (length == 0)
      return maxValue;
    for (int i = 0; i < length; i++) {
      maxValue = std::max<T>(maxValue, buffer[i]);
    }
    return maxValue;
  }
};

} // namespace gfx

#endif // GFX_MOVING_AVERAGE
