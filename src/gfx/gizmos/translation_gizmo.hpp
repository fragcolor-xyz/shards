#ifndef BCCA14A9_4E3A_4D04_B838_CE099ABD609E
#define BCCA14A9_4E3A_4D04_B838_CE099ABD609E

#include "gizmos.hpp"
#include <gfx/linalg.hpp>

namespace gfx {
namespace gizmos {
// Note: While only a single handle of the TranslationGizmo may be selected at any point in time,
// handles from multiple different gizmos may be selected at the same time and manipulated.
// In such a case, do be forewarned that some awkward behaviour may occur as multiple gizmos
// are not expected to be active and used at the same time.
struct TranslationGizmo : public IGizmo, public IGizmoCallbacks {
  float4x4 transform = linalg::identity;

  Handle handles[3];
  Box handleSelectionBoxes[3];
  float4x4 dragStartTransform;
  float3 dragStartPoint;

  float scale = 1.0f;
  const float axisRadius = 0.05f;
  const float axisLength = 0.55f;

  float getGlobalAxisRadius() const { return axisRadius * scale; }
  float getGlobalAxisLength() const { return axisLength * scale; }

  TranslationGizmo() {
    for (size_t i = 0; i < 3; i++) {
      handles[i].userData = (void *)i;
      handles[i].callbacks = this;
    }
  }

  void update(InputContext &inputContext) {
    float3x3 invTransform = linalg::inverse(extractRotationMatrix(transform));
    float3 localRayDir = linalg::mul(invTransform, inputContext.rayDirection);

    for (size_t i = 0; i < 3; i++) {
      auto &handle = handles[i];
      auto &selectionBox = handleSelectionBoxes[i];

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

      auto &min = selectionBox.min;
      auto &max = selectionBox.max;
      min = (-t1 * getGlobalAxisRadius() - t2 * getGlobalAxisRadius()) * hitboxScale.x;
      max =
          (t1 * getGlobalAxisRadius() + t2 * getGlobalAxisRadius()) * hitboxScale.x + fwd * getGlobalAxisLength() * hitboxScale.y;

      selectionBox.transform = transform;

      float hitDistance = intersectBox(inputContext.eyeLocation, inputContext.rayDirection, handleSelectionBoxes[i]);
      inputContext.updateHandle(handle, hitDistance);
    }
  }

  size_t getHandleIndex(Handle &inHandle) { return size_t(inHandle.userData); }

  float3 getAxisDirection(size_t index, float4x4 transform) {
    float3 base{};
    base[index] = 1.0f;
    return linalg::normalize(linalg::mul(transform, float4(base, 0)).xyz());
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
    delta = linalg::dot(delta, fwd) * fwd;
    transform = linalg::mul(linalg::translation_matrix(delta), dragStartTransform);
  }

  void render(InputContext &inputContext, GizmoRenderer &renderer) {
    for (size_t i = 0; i < 3; i++) {
      auto &handle = handles[i];
      auto &selectionBox = handleSelectionBoxes[i];

      bool hovering = inputContext.hovering && inputContext.hovering == &handle;

#if 0
      // Debug draw
      float4 color = float4(.7, .7, .7, 1.);
      uint32_t thickness = 1;
      if (hovering) {
        color = float4(.5, 1., .5, 1.);
        thickness = 2;
      }

      auto &min = selectionBox.min;
      auto &max = selectionBox.max;
      float3 center = (max + min) / 2.0f;
      float3 size = max - min;

      renderer.getShapeRenderer().addBox(selectionBox.transform, center, size, color, thickness);
#endif

      float3 loc = extractTranslation(selectionBox.transform);
      float3 dir = getAxisDirection(i, selectionBox.transform);
      float4 axisColor = axisColors[i];
      axisColor = float4(axisColor.xyz() * (hovering ? 1.1f : 0.9f), 1.0f);
      renderer.addHandle(loc, dir, getGlobalAxisRadius(), getGlobalAxisLength(), axisColor, GizmoRenderer::CapType::Arrow,
                         axisColor);
    }
  }
};
} // namespace gizmos
} // namespace gfx

#endif /* BCCA14A9_4E3A_4D04_B838_CE099ABD609E */
