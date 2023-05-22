#include "gizmo_input.hpp"
#include <float.h>
#include <spdlog/spdlog.h>

namespace gfx {
namespace gizmos {

void InputContext::begin(const InputState &inputState, ViewPtr view) {
  this->prevInputState = this->inputState;
  this->inputState = inputState;
  updateView(view);

  // Reset hover state
  hitDistance = std::numeric_limits<float>::infinity();
  hovering = nullptr;

  // Reset tracker for held handle
  heldHandleUpdated = false;
}

// Call with hit distance from raycast
void InputContext::updateHandle(Handle &handle, float hitDistance) {
  bool isHeld = &handle == held;

  if (hitDistance < this->hitDistance) {
    this->hitDistance = hitDistance;
    this->hovering = &handle;
  }

  // Movement callback for held handle
  if (isHeld) {
    updateHeldHandle();
  }
}

void InputContext::updateHeldHandle() {
  assert(held);

  heldHandleUpdated = true;

  // Force hovered to the same handle
  hovering = held;

  // Execute movement callback
  Handle &handle = *held;
  assert(handle.callbacks);
  handle.callbacks->move(*this, handle);
}

void InputContext::updateHitLocation() {
  // Hit location from raycast results
  if (hovering)
    hitLocation = eyeLocation + rayDirection * hitDistance;
  else
    hitLocation = float3(0, 0, 0);
}

void InputContext::end() {
  updateHitLocation();

  // If a held handle was not updated this frame, clear it
  if (held && !heldHandleUpdated)
    held = nullptr;

  if (held && !inputState.pressed) {
    Handle &handle = *held;
    assert(handle.callbacks);
    handle.callbacks->released(*this, handle);
    held = nullptr;
  } else if (!held && hovering && !prevInputState.pressed && inputState.pressed) {
    held = hovering;
    Handle &handle = *hovering;
    assert(handle.callbacks);
    handle.callbacks->grabbed(*this, handle);
  }
}

void InputContext::updateView(ViewPtr view) {
  float2 ndc2 = float2(inputState.cursorPosition) / float2(inputState.viewSize);
  ndc2 = ndc2 * 2.0f - 1.0f;
  ndc2.y = -ndc2.y;
  float4 ndc = float4(ndc2, 0.1f, 1.0f);

  cachedViewProj = linalg::mul(view->getProjectionMatrix(inputState.viewSize), view->view);
  cachedViewProjInv = linalg::inverse(cachedViewProj);
  float4x4 viewInv = linalg::inverse(view->view);

  float4 unprojected = linalg::mul(cachedViewProjInv, ndc);
  unprojected /= unprojected.w;

  eyeLocation = viewInv.w.xyz();
  rayDirection = linalg::normalize(unprojected.xyz() - eyeLocation);
}

} // namespace gizmos
} // namespace gfx
