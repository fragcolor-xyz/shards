#ifndef BCCA14A9_4E3A_4D04_B838_CE099ABD609E
#define BCCA14A9_4E3A_4D04_B838_CE099ABD609E

#include "gizmos.hpp"
#include <gfx/linalg.hpp>

namespace gfx {
namespace gizmos {

// For two axis composite movements
struct CombinedAxes {
  float3 x{};
  float3 y{};
  float3 z{};
  int xi;
  int yi;
  int zi;

  static CombinedAxes get(int baseIndex) {
    CombinedAxes r;
    r.xi = (baseIndex + 1) % 3;
    r.yi = (baseIndex + 2) % 3;
    r.zi = (baseIndex + 0) % 3;
    r.x[r.xi] = 1.0f;
    r.y[r.yi] = 1.0f;
    r.z[r.zi] = 1.0f;
    return r;
  }
};

// Note: If multiple gizmos are to be active at any time, ensure that they are created in the same Gizmos.Context
//       This is to prevent multiple handles from different gizmos being selected at the same time,
//       resulting in unexpected behaviour.
struct TranslationGizmo : public IGizmo, public IGizmoCallbacks {
  float4x4 transform = linalg::identity;

  Handle handles[6];
  Box handleSelectionBoxes[6];

  float4x4 dragStartTransform;
  float3 dragStartPoint;

  float scale = 1.0f;
  const float axisRadius = 0.2f;
  const float visualAxisRadius = 0.1f;
  const float axisLength = 1.0f;

  // How far the combined planes extend from the gizmo center (relative to handle length)
  const float combinedAxisSizeRatio = 0.5f;

  float getVisualAxisRadius() const { return visualAxisRadius * scale; }
  float getAxisRadius() const { return axisRadius * scale; }
  float getAxisLength() const { return axisLength * scale; }

  TranslationGizmo() {
    for (size_t i = 0; i < 6; i++) {
      handles[i].userData = (void *)i;
      handles[i].callbacks = this;
    }
  }

  void update(InputContext &inputContext) {
    float4x4 transform = this->transform; // Cache old since it might be updated during moving
    float3x3 invTransform = linalg::inverse(extractRotationMatrix(transform));
    float3 localRayDir = linalg::mul(invTransform, inputContext.rayDirection);

    for (size_t i = 0; i < 6; i++) {
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
      const float2 hitboxScale = linalg::lerp(float2(1.1f, 1.1f), float2(0.5f, 1.0f), angleFactor);

      auto &min = selectionBox.min;
      auto &max = selectionBox.max;
      int layer = 0;
      if (i < 3) {
        min = (-t1 * getAxisRadius() - t2 * getAxisRadius()) * hitboxScale.x;
        max = (t1 * getAxisRadius() + t2 * getAxisRadius()) * hitboxScale.x +
              fwd * getAxisLength() * hitboxScale.y * 1.0f;
      } else {
        float thickness = getAxisRadius() * 0.3f;
        CombinedAxes ca = CombinedAxes::get(i - 3);
        float3 xy = ca.x + ca.y;
        min = float3() - ca.z * thickness + xy * getAxisLength() * 0.2f;
        max = float3() + ca.z * thickness + xy * getAxisLength() * combinedAxisSizeRatio;
        layer = -1;
      }

      selectionBox.transform = transform;

      inputContext.updateHandle(
          handle, GizmoHit(intersectBox(inputContext.eyeLocation, inputContext.rayDirection, handleSelectionBoxes[i]), layer));
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

    if (index < 3) {
      dragStartPoint = hitOnPlane(context.eyeLocation, context.rayDirection, extractTranslation(dragStartTransform),
                                  getAxisDirection(index, dragStartTransform));
    } else {
      CombinedAxes ca = CombinedAxes::get(index - 3);
      float3 axis = linalg::normalize(linalg::mul(transform, float4(ca.z, 0.0f)).xyz());
      dragStartPoint =
          hitOnPlaneUnprojected(context.eyeLocation, context.rayDirection, extractTranslation(dragStartTransform), axis);
    }
  }

  virtual void released(InputContext &context, Handle &handle) {
    size_t index = getHandleIndex(handle);
    SPDLOG_DEBUG("Handle {} ({}) released", index, getAxisDirection(index, dragStartTransform));
  }

  virtual void move(InputContext &context, Handle &inHandle) {
    size_t index = getHandleIndex(inHandle);
    float3 fwd = getAxisDirection(getHandleIndex(inHandle), dragStartTransform);

    if (index < 3) {
      float3 hitPoint = hitOnPlane(context.eyeLocation, context.rayDirection, dragStartPoint, fwd);

      float3 delta = hitPoint - dragStartPoint;
      delta = linalg::dot(delta, fwd) * fwd;
      transform = linalg::mul(linalg::translation_matrix(delta), dragStartTransform);
    } else {
      CombinedAxes ca = CombinedAxes::get(index - 3);
      float3 axis = linalg::normalize(linalg::mul(transform, float4(ca.z, 0.0f)).xyz());

      float3 hitPoint =
          hitOnPlaneUnprojected(context.eyeLocation, context.rayDirection, extractTranslation(dragStartTransform), axis);

      float3 delta = hitPoint - dragStartPoint;
      transform = linalg::mul(linalg::translation_matrix(delta), dragStartTransform);
    }
  }

  void render(InputContext &inputContext, GizmoRenderer &renderer) {
    for (size_t i = 0; i < 6; i++) {
      auto &handle = handles[i];
      bool hovering = inputContext.hovering && inputContext.hovering == &handle;
      float3 loc = extractTranslation(transform);
      float3 dir = getAxisDirection(i, transform);
      if (i < 3) {
        float4 axisColor = axisColors[i];
        axisColor = float4(axisColor.xyz() * (hovering ? 1.2f : 0.9f), 1.0f);
        renderer.addHandle(loc, dir, getVisualAxisRadius(), getAxisLength(), axisColor, GizmoRenderer::CapType::Arrow,
                           axisColor);
      } else {
        CombinedAxes ca = CombinedAxes::get(i - 3);
        float3 x = linalg::normalize(linalg::mul(transform, float4(ca.x, 0.0f)).xyz());
        float3 y = linalg::normalize(linalg::mul(transform, float4(ca.y, 0.0f)).xyz());
        float maxf = getAxisLength() * combinedAxisSizeRatio;
        float size = maxf * 0.5f;
        float3 center = loc + (maxf - size * 0.5f) * (x + y);

        float brightness = hovering ? 0.98f : 0.68f;
        float3 base = axisColors[ca.zi].xyz();
        float a = hovering ? 0.8f : 0.5f;
        float4 color = float4(linalg::lerp(base, float3(brightness), hovering ? 0.2f : 0.7f), a);

        renderer.getShapeRenderer().addRect(center, x, y, float2(size), float4(base, 1.0f), 1);
        if (hovering) {
          renderer.getShapeRenderer().addSolidRect(center, x, y, float2(size), color, false);
        }
      }

#if GIZMO_DEBUG
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
    }
  }
};
} // namespace gizmos
} // namespace gfx

#endif /* BCCA14A9_4E3A_4D04_B838_CE099ABD609E */
