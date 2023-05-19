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
  hitDistance = FLT_MAX;
  hovering = nullptr;

  // Reset tracker for held handle
  heldHandleUpdated = false;
}

// TODO: move raycasting out of updateHandle, maybe even call it with a distance parameter
// let the gizmo itself handle the raycasting however it needs to instead perhaps
void InputContext::updateHandle(Handle &handle) {
  bool isHeld = &handle == held;

  //auto &box = handle.selectionBox;
  //float4x4 invHandleTransform = linalg::inverse(handle.selectionBoxTransform);
  //float3 rayLoc1 = linalg::mul(invHandleTransform, float4(eyeLocation, 1)).xyz();
  //float3 rayDir1 = linalg::mul(invHandleTransform, float4(rayDirection, 0)).xyz();
  //float2 hit = intersectAABB(rayLoc1, rayDir1, box.min, box.max);

  handle.resolveHover(*this);

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

void BoxHandle::resolveHover(InputContext &context) {
  auto &box = selectionBox;
  float4x4 invHandleTransform = linalg::inverse(selectionBoxTransform);
  float3 rayLoc1 = linalg::mul(invHandleTransform, float4(context.eyeLocation, 1)).xyz();
  float3 rayDir1 = linalg::mul(invHandleTransform, float4(context.rayDirection, 0)).xyz();
  float2 hit = intersectAABB(rayLoc1, rayDir1, box.min, box.max);

  if (hit.x < hit.y) {
    if (hit.x < context.hitDistance) {
      context.hitDistance = hit.x;
      context.hovering = this;
    }
  }
}

void DiscHandle::resolveHover(InputContext &context) {
  float3 axisDir{};
  axisDir[size_t(userData)] = 1.0f;

  // find a unit vector perpendicular to the forward axis and the camera direction
  float3 rotPlaneX = linalg::normalize(linalg::cross(axisDir, context.rayDirection));
  //float3 hitLoc = hitOnPlane(context.eyeLocation, context.rayDirection, selectionDisc.center, rotPlaneX);
  float3 hitLoc = hitOnPlane(selectionDisc.center + axisDir, context.rayDirection, selectionDisc.center, rotPlaneX);
  float distanceFromCenter = linalg::distance(hitLoc, selectionDisc.center);

  static int debugged{};
  if (axisDir[0] == 1.0f) {
    debugged++;
    if (debugged % 300 == 0) {
      SPDLOG_DEBUG("axisDir: {} {} {}    rotplaneX: {} {} {}    hitLoc: {} {} {}  distance: {}", axisDir.x, axisDir.y, axisDir.z,
                   rotPlaneX.x, rotPlaneX.y, rotPlaneX.z, hitLoc.x, hitLoc.y, hitLoc.z, distanceFromCenter);
      SPDLOG_DEBUG("discRadii: {} {}", selectionDisc.innerRadius, selectionDisc.outerRadius);
    }
  }
  
  if (distanceFromCenter <= selectionDisc.outerRadius && distanceFromCenter >= selectionDisc.innerRadius) {
    if (distanceFromCenter < context.hitDistance) {
      context.hitDistance = distanceFromCenter;
      context.hovering = this;
    }
  }
}

} // namespace gizmos
} // namespace gfx
