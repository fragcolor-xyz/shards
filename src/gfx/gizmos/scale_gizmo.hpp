#ifndef IDENTIFIER_TO_BE_REPLACED
#define IDENTIFIER_TO_BE_REPLACED

#include "gizmos.hpp"
#include <gfx/linalg.hpp>

namespace gfx {
namespace gizmos {
struct ScalingGizmo : public IGizmo, public IGizmoCallbacks {
  float4x4 transform = linalg::identity; // load identity matrix

  Handle handles[4];
  const float sensitivity = 0.01f;
  float4x4 dragStartTransform;
  float3 dragStartPoint;

  float scale = 1.0f;
  const float axisRadius = 0.05f;
  const float axis Length = 0.55f;

  float getGlobalAxisRadius() const { return axisRadius * scale; }
  float getGlobalAxisLength() const { return axisLength * scale; }

  ScalingGizmo() {
    for (size_t i = 0; i < 4; i++) {
      handles[i].userData = (void *)i;
      handles[i].callbacks = this;
    }
  }

  void update(InputContext &inputContext) {
    float3x3 invTransform = linalg::inverse(extractRotationMatrix(transform));
    float3 localRayDir = linalg::mul(invTransform, inputContext.rayDirection);

    for (size_t i = 0; i < 4; i++) {
      auto &handle = handles[i];

      float3 fwd{};
      fwd[i] = 1.0f;
      float3 t1 = float3(-fwd.z, -fwd.x, fwd.y);
      float3 t2 = linalg::cross(fwd, t1);

      // Slightly decrease hitbox size if view direction is parallel
      // e.g. looking towards +z you are less likely to want to click on the z axis
      float dotThreshold = 0.8f;
      float angleFactor = std::max(0.0f, (linalg::abs(linalg::dot(localRayDir, fwd)) - dotThreshold) / (1.0f - dotThreshold));

      // Make hitboxes slightly bigger than the actual visuals
      const float2 hitboxScale = linalg::lerp(float2(2.2f, 1.2f), float2(0.8f, 1.0f), angleFactor);

      auto &min = handle.selectionBox.min;
      auto &max = handle.selectionBox.max;
      min = (-t1 * getGlobalAxisRadius() - t2 * getGlobalAxisRadius()) * hitboxScale.x;
      max =
          (t1 * getGlobalAxisRadius() + t2 * getGlobalAxisRadius()) * hitboxScale.x + fwd * getGlobalAxisLength() * hitboxScale.y;

      handle.selectionBoxTransform = transform;

      inputContext.updateHandle(handle);
    }
  }

  size_t getHandleIndex(Handle &inHandle) { return size_t(inHandle.userData); }

  float3 getAxisDirection(size_t index, float4x4 transform) {
    float3 base{};
    base[index] = 1.0f;
    return linalg::normalize(linalg::mul(transform, float4(base, 0)).xyz());
  }

  virtual void grabbed(InputConetxt &context, Handle &handle) {
    dragStartTransform = transform; // transformation matrix before drag starts

    size_t index = getHandleIndex(handle);
    SPDLOG_DEBUG("Handle {} ({}) grabbed", index, getAxisDirection(index, dragStartTransform));

    dragStartPoint = hitOnPlane(context.eyeLocation, context.rayDirection, extractTranslation(dragStartTransform),
                                getAxisDirection(index, dragStartTransform));
  }

  virtual void released(InputContext &context, Handle &handle) {
    size_t index = getHandleIndex(handle);
    SPDLOG_DEBUG("Handle {} ({}) released", index, getAxisDirection(index, dragStartTransform));
  }

  virtual void move(InputContext &context, Handle &inHandle) {
    size_t index = getHandleIndex(inHandle);
    float3 fwd = getAxisDirection(index, dragStartTransform);
    float3 hitPoint = hitOnPlane(context.eyeLocation, context.rayDirection, dragStartPoint, fwd);

    float3 delta = hitPoint - dragStartPoint;
    float3 scaling;
    if (i == 3) {
      // Grabbing the centered cube - scale all sides uniformly
      float uniformScaling = std::min(std::abs(delta.x), std::min(std::abs(delta.y), std::abs(delta.z)));
      scaling = float3(uniformScaling);
    }
    else {
        // Grabbing a cube on the axis - scale in one direction
        delta = linalg::dot(delta, fwd) * fwd;
        scaling = delta * sensitivity;
    }
    transform = linalg::mul(linalg::scaling_matrix(scaling), dragStartTransform);
  }

  void render(InputContext &inputContext, GizmoRenderer &renderer) {
    for (size_t i = 0; i < 3; i++) {
      auto &handle = handles[i];

      bool hovering = inputContext.hovering && inputContext.hovering == &handle;

      float3 loc = extractTranslation(handle.selectionBoxTransform);
      float3 dir = getAxisDirection(i, handle.selectionBoxTransform);
      float4 cubeColor;
      if (i == 3) {
        cubeColor = colorToFloat(colorFromRGBA(0x555555FF)) * 1.1f;
      }
      else {
        cubeColor = axisColors[i];
      }
      cubeColor = float4(cubeColor.xyz() * (hovering ? 1.1f : 0.9f), 1.0f);
      renderer.addHandle(loc, dir, getGlobalAxisRadius(), getGlobalAxisLength(), axisColor, GizmoRenderer::CapType::Cube,
                         axisColor);
    }
  }
}
} // namespace gizmos
} // namespace gfx

#endif /* IDENTIFIER_TO_BE_REPLACED */
