#include "gestures.hpp"
#include "input/events.hpp"
#include "input/master.hpp"
#include "input/state.hpp"
#include "linalg.h"
#include <gfx/fmt.hpp>
#include <gfx/math.hpp>
#include <memory>
#include <spdlog/spdlog.h>

namespace shards::input {
using namespace gestures;

Pointer eventToPointer(const PointerTouchEvent &pte) { return Pointer{.position = pte.pos, .pressure = pte.pressure}; }

bool thresholdNormalizedAspectCorrect(const GestureInputs &in, float distance, float threshold) {
  float2 inputSize = in.state.region.size;
  float ds = std::max(inputSize.x, inputSize.y);
  return (distance / ds) > threshold;
}

void updatePointers(const GestureInputs &in, Pointers &outPointers, size_t numFingerThreshold,
                    const char *debugName = "unknown gesture") {
  if (in.canReceiveInput) {
    outPointers.filter(in.state.pointers);
    for (auto &evt : in.events) {
      if (PointerTouchEvent *pte = std::get_if<PointerTouchEvent>(&evt.event)) {
        if (evt.isConsumed() && evt.consumed.value().handler != in.consumer)
          continue;
        if (outPointers.contains(pte->index))
          continue;
        if (pte->pressed) {
          SPDLOG_INFO("[{}] Adding pointer {}", debugName, pte->index);
          outPointers.pointers.emplace(pte->index, eventToPointer(*pte));
          in.consumer(evt);
        }
      }
    }
  } else {
    outPointers.pointers.clear();
  }
}

bool updateReqs(bool &outRequirementsMet, const Pointers &pointers, size_t numRequiredFingers, bool ignoreExtraFingers = true) {
  if (!outRequirementsMet) {
    outRequirementsMet = pointers.size() == numRequiredFingers;
  } else {
    if (ignoreExtraFingers) {
      if (pointers.size() < numRequiredFingers)
        outRequirementsMet = false;
    } else {
      if (pointers.size() != numRequiredFingers)
        outRequirementsMet = false;
    }
  }
  return outRequirementsMet;
}

void updateActivation(bool &outActivate, GestureOutputs &out, const char *debugName = "unknown gesture") {
  if (!outActivate && out.requirementsMet && out.overActivationThreshold) {
    SPDLOG_INFO("[{}] Gesture activated", debugName);
    outActivate = true;
  }
  if (outActivate && !out.requirementsMet) {
    SPDLOG_INFO("[{}] Gesture deactivated", debugName);
    outActivate = false;
  }
}

void Pinch::update(const GestureInputs &in, GestureOutputs &out) {
  updatePointers(in, initialPointers, 2, "Rotate");
  bool oldRequirementsMet = requirementsMet;
  out.requirementsMet = updateReqs(requirementsMet, initialPointers, 2);
  auto &newPointers = in.state.pointers;

  delta = 0.0f;
  if (out.requirementsMet) {
    auto ofirst = initialPointers.pointers.begin();
    auto osecond = std::next(ofirst);
    float2 op1 = ofirst->second.position;
    float2 op2 = osecond->second.position;
    float2 d0 = op2 - op1;

    auto lastp1 = prevPointers.get(ofirst->first);
    auto lastp2 = prevPointers.get(osecond->first);

    float2 p1 = newPointers.pointers[ofirst->first].position;
    float2 p2 = newPointers.pointers[osecond->first].position;
    float2 d1 = p2 - p1;

    float l0 = linalg::length(d0);
    float l1 = linalg::length(d1);

    out.overActivationThreshold = thresholdNormalizedAspectCorrect(in, std::abs(linalg::distance(d1, d0)), threshold);

    if (activated && oldRequirementsMet && lastp1 && lastp2) {
      float prevd = linalg::length(lastp2->get().position - lastp1->get().position);
      delta = l1 - prevd;
      zoomScale = l1 / l0;

      SPDLOG_INFO("Pinch delta0:{} delta1:{}", linalg::distance(p2, op2), linalg::distance(p1, op1));
      SPDLOG_INFO("Pinch op1: {} p1: {} op2: {} p2: {}", op1, p1, op2, p2);
    }

    prevPointers = newPointers;
  } else {
    zoomScale = 1.0f;
  }

  updateActivation(activated, out, "Pinch");
}

void Rotate::update(const GestureInputs &in, GestureOutputs &out) {
  updatePointers(in, pointers, 2, "Rotate");
  out.requirementsMet = updateReqs(requirementsMet, pointers, 2);
  auto &newPointers = in.state.pointers;

  rotation = 0.0f;
  if (out.requirementsMet) {
    auto ofirst = pointers.pointers.begin();
    auto osecond = std::next(ofirst);
    float2 op1 = ofirst->second.position;
    float2 op2 = osecond->second.position;

    float2 d0 = linalg::normalize(op2 - op1);

    float2 p1 = newPointers.pointers[ofirst->first].position;
    float2 p2 = newPointers.pointers[osecond->first].position;

    float diameter = linalg::distance(p2, p1);
    float2 d1 = (p2 - p1) / diameter;

    if (activated)
      rotation = acos(linalg::dot(d0, d1));
    out.overActivationThreshold = thresholdNormalizedAspectCorrect(in, diameter * (rotation / gfx::pi2), threshold);
  }

  updateActivation(activated, out, "Rotate");
}

void MultiSlide::update(const GestureInputs &in, GestureOutputs &out) {
  updatePointers(in, startPointers, 2, "Slide");
  bool oldRequirementsMet = requirementsMet;
  out.requirementsMet = updateReqs(requirementsMet, startPointers, numFingers, ignoreExtraFingers);
  auto &newPointers = in.state.pointers;

  delta = float2{};

  if (requirementsMet) {
    float2 newSlideOffset = newPointers.centroid() - startPointers.centroid();
    out.overActivationThreshold = thresholdNormalizedAspectCorrect(in, linalg::length(newSlideOffset), threshold);
    if (activated) {
      if (oldRequirementsMet)
        delta = newSlideOffset - slideOffset;
      slideOffset = newSlideOffset;
    } else {
      slideOffset = float2{};
    }
  } else {
    slideOffset = float2{};
  }

  updateActivation(activated, out, fmt::format("Slide({})", numFingers).c_str());
}

void GestureRecognizer::update(bool canReceiveInput, EventConsumer &consumer, const InputState &state,
                               std::span<ConsumableEvent> events) {
  bool canActivateGesture = !activeGesture && !waitForPointerReset;

  // Detect and update gestures
  std::optional<GestureOutputs> activeGestureOutput;
  for (auto &gesture : gestures) {
    GestureOutputs out;
    gesture->update(GestureInputs{canReceiveInput, consumer, state, events}, out);
    if (canActivateGesture && out.requirementsMet && out.overActivationThreshold) {
      activeGesture = gesture.get();
      waitForPointerReset = true;
    }
    if (gesture.get() == activeGesture) {
      activeGestureOutput = out;
    }
  }

  // Consume all pointer events while gesture is active
  if (activeGesture) {
    for (auto &event : events) {
      if (isPointerEvent(event.event)) {
        consumer(event);
      }
    }
  }

  // Reset tracked gesture when requirements are no longer met
  if (activeGestureOutput && !activeGestureOutput->requirementsMet) {
    activeGesture = nullptr;
  }

  // Wait for all fingers to be removed from the touch device
  // before retriggering any gestures
  if (waitForPointerReset && !state.pointers.any()) {
    waitForPointerReset = false;
  }
}
} // namespace shards::input
