#ifndef BDC85E29_353C_4D76_AA82_C2E6B6AAE576
#define BDC85E29_353C_4D76_AA82_C2E6B6AAE576

#include "input/master.hpp"
#include "input/events.hpp"
#include "state.hpp"
#include <span>
#include <optional>

namespace shards::input {

namespace gestures {
struct GestureInputs {
  bool canReceiveInput;
  EventConsumer &consumer;
  const InputState &state;
  std::span<ConsumableEvent> events;
};

struct GestureOutputs {
  bool requirementsMet{};
  bool overActivationThreshold{};
};

struct IGesture {
  virtual void update(const GestureInputs &in, GestureOutputs &out) = 0;
};

struct Pinch : public IGesture {
  float threshold = 0.08f;
  float zoomScale{};
  float delta{};
  Pointers prevPointers;
  Pointers initialPointers;
  bool requirementsMet{};
  bool activated{};
  void update(const GestureInputs &in, GestureOutputs &out) override;
};

struct Rotate : public IGesture {
  float threshold = 0.1f;
  float rotation{};
  Pointers pointers;
  bool requirementsMet{};
  bool activated{};
  void update(const GestureInputs &in, GestureOutputs &out) override;
};

struct MultiSlide : public IGesture {
  float threshold = 0.1f;
  size_t numFingers = 2;
  bool ignoreExtraFingers = false;
  // Movement from start of gesture
  float2 slideOffset{};
  // Movement from last frame
  float2 delta{};
  Pointers startPointers;
  bool requirementsMet{};
  bool activated{};
  void update(const GestureInputs &in, GestureOutputs &out) override;
};
} // namespace gestures

struct GestureRecognizer {
  std::vector<std::shared_ptr<gestures::IGesture>> gestures;
  gestures::IGesture *activeGesture{};
  bool waitForPointerReset{};

  void update(bool canReceiveInput, EventConsumer &consumer, const InputState &state, std::span<ConsumableEvent> events);
};
} // namespace shards::input

#endif /* BDC85E29_353C_4D76_AA82_C2E6B6AAE576 */
