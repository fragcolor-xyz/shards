#include "gizmo_input.hpp"
#include <float.h>

namespace gfx {
namespace gizmos {

void InputContext::begin(const InputState &inputState, ViewPtr view) {
  this->prevInputState = this->inputState;
  this->inputState = inputState;
  updateView(view);

  // Reset hover state
  hitDistance = FLT_MAX;
  hovered = nullptr;

  // Reset tracker for held handle
  heldHandleUpdated = false;
}

void InputContext::updateHandle(Handle &handle) {
  bool isHeld = &handle == held;

  auto &box = handle.selectionBox;
  float4x4 invHandleTransform = linalg::inverse(handle.selectionBoxTransform);
  float3 rayLoc1 = linalg::mul(invHandleTransform, float4(eyeLocation, 1)).xyz();
  float3 rayDir1 = linalg::mul(invHandleTransform, float4(rayDirection, 0)).xyz();
  float2 hit = intersectAABB(rayLoc1, rayDir1, box.min, box.max);
  if (hit.x < hit.y) {
    if (hit.x < hitDistance) {
      hitDistance = hit.x;
      hovered = &handle;
    }
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
  hovered = held;

  // Execute movement callback
  Handle &handle = *held;
  assert(handle.callbacks);
  handle.callbacks->move(*this, handle);
}

void InputContext::updateHitLocation() {
  // Hit location from raycast results
  if (hovered)
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
  } else if (!held && hovered && !prevInputState.pressed && inputState.pressed) {
    held = hovered;
    Handle &handle = *hovered;
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
