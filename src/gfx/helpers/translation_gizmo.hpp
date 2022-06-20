#ifndef BCCA14A9_4E3A_4D04_B838_CE099ABD609E
#define BCCA14A9_4E3A_4D04_B838_CE099ABD609E

#include "gizmos.hpp"

namespace gfx {
namespace gizmos {
struct TranslationGizmo : public IGizmo, public IGizmoCallbacks {
  float4x4 transform = linalg::identity;

  Handle handles[3];
  float4x4 dragStartTransform;
  float3 dragStartPoint;

  const float axisRadius = 0.1f;
  const float axisLength = 0.7f;

  TranslationGizmo() {
    for (size_t i = 0; i < 3; i++) {
      handles[i].userData = (void *)i;
      handles[i].callbacks = this;
    }
  }

  void update(InputContext &inputContext) {
    for (size_t i = 0; i < 3; i++) {
      auto &handle = handles[i];

      float3 fwd{};
      fwd[i] = 1.0f;
      float3 t1 = float3(-fwd.z, -fwd.x, fwd.y);
      float3 t2 = linalg::cross(fwd, t1);

      auto &min = handle.selectionBox.min;
      auto &max = handle.selectionBox.max;
      min = -t1 * axisRadius - t2 * axisRadius;
      max = t1 * axisRadius + t2 * axisRadius + fwd * axisLength;

      handle.selectionBoxTransform = transform;

      inputContext.updateHandle(handle);
    }
  }

  size_t getHandleIndex(Handle &inHandle) { return size_t(inHandle.userData); }

  float3 getAxisDirection(size_t index, float4x4 transform) {
    float3 base{};
    base[index] = 1.0f;
    return linalg::mul(transform, float4(base, 0)).xyz();
  }

  virtual void grabbed(InputContext &context, Handle &handle) {
    dragStartTransform = transform;

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
    float3 fwd = getAxisDirection(getHandleIndex(inHandle), dragStartTransform);
    float3 hitPoint = hitOnPlane(context.eyeLocation, context.rayDirection, dragStartPoint, fwd);

    float3 delta = hitPoint - dragStartPoint;
    transform = linalg::mul(linalg::translation_matrix(delta), dragStartTransform);
  }

  void render(InputContext &inputContext, GizmoRenderer &renderer) {
    for (size_t i = 0; i < 3; i++) {
      auto &handle = handles[i];

      // Debug draw
      float4 color = float4(.7, .7, .7, 1.);
      uint32_t thickness = 1;
      bool hovered = inputContext.hovered && inputContext.hovered == &handle;
      if (hovered) {
        color = float4(.5, 1., .5, 1.);
        thickness = 2;
      }

      auto &min = handle.selectionBox.min;
      auto &max = handle.selectionBox.max;
      float3 center = (max + min) / 2.0f;
      float3 size = max - min;

      renderer.getShapeRenderer().addBox(handle.selectionBoxTransform, center, size, color, thickness);

      float3 loc = extractTranslation(handle.selectionBoxTransform);
      float3 dir = getAxisDirection(i, handle.selectionBoxTransform);
      float4 axisColor = axisColors[i];
      axisColor.xyz() *= hovered ? 1.1f : 0.9f;
      renderer.addHandle(loc, dir, axisRadius, axisLength, axisColor, GizmoRenderer::CapType::Arrow, axisColor);
    }
  }
};
} // namespace gizmos
} // namespace gfx

#endif /* BCCA14A9_4E3A_4D04_B838_CE099ABD609E */
