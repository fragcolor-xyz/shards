#ifndef A234D1C4_5FF6_468F_826F_BC431A25D754
#define A234D1C4_5FF6_468F_826F_BC431A25D754

#include "linalg.h"
#include <functional>
#include <gfx/drawables/mesh_tree_drawable.hpp>
#include <optional>
#include <unordered_map>
#include <stdexcept>

namespace gfx {

namespace animation {
enum class Interpolation { Linear, Step, Cubic };

enum class BuiltinTarget {
  Translation,
  Rotation,
  Scale,
};

using Value = std::variant<float3, float4>;

struct Track {
  MeshTreeDrawable::Ptr::weak_type targetNode;
  BuiltinTarget target;

  std::vector<float> times;
  std::vector<float> data;
  size_t elementSize{};
  Interpolation interpolation{};

  Track() = default;
  Track(Track &&) = default;
  Track &operator=(Track &&) = default;
  Track(const Track &) = default;
  Track &operator=(const Track &) = default;

  Value getValue(size_t keyframeIndex) const;
};

struct State {
  size_t targetNode{};
  BuiltinTarget target{};
  Value value;

  State(size_t targetNode, BuiltinTarget target, Value value) : targetNode(targetNode), target(target), value(value) {}
};

struct Frame {
  std::vector<State> states;

  Frame() = default;
  Frame(Frame &&) = default;
  Frame &operator=(Frame &&) = default;
  Frame(const Frame &) = default;
  Frame &operator=(const Frame &) = default;
};

inline Value unpack(animation::BuiltinTarget target, const float *data, size_t dataSize) {
  switch (target) {
  case animation::BuiltinTarget::Rotation:
    if (dataSize < 4) {
      throw std::out_of_range("Insufficient data for float4 (Rotation)");
    }
    return float4(data);
  case animation::BuiltinTarget::Translation:
  case animation::BuiltinTarget::Scale:
    if (dataSize < 3) {
      throw std::out_of_range("Insufficient data for float3 (Translation or Scale)");
    }
    return float3(data);
  default:
    throw std::logic_error("Invalid animation target to unpack");
  }
}

inline Value Track::getValue(size_t keyframeIndex) const {
  auto offset = keyframeIndex * elementSize;
  assert(offset + elementSize <= data.size() && "Keyframe index out of range or element size too large");
  return unpack(target, data.data() + offset, elementSize);
}

}; // namespace animation

struct Animation {
  std::vector<animation::Track> tracks;

  float getDuration() const;

  Animation() = default;
  Animation(Animation &&) = default;
  Animation &operator=(Animation &&) = default;
  Animation(const Animation &) = default;
  Animation &operator=(const Animation &) = default;
};

inline float Animation::getDuration() const {
  float maxDuration{};
  for (auto &track : tracks) {
    maxDuration = std::max(maxDuration, track.times.back());
  }
  return maxDuration;
}

} // namespace gfx

#endif /* A234D1C4_5FF6_468F_826F_BC431A25D754 */