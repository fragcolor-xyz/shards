#ifndef BCCA14A9_4E3A_4D04_B838_CE099ABD609E
#define BCCA14A9_4E3A_4D04_B838_CE099ABD609E

#include "gizmos.hpp"
#include <gfx/linalg.hpp>

namespace gfx {
namespace gizmos {
struct TranslationGizmo : public IGizmo, public IGizmoCallbacks {
  float4x4 transform = linalg::identity;

  Handle handles[6];
  float4x4 dragStartTransform;
  float3 dragStartPoint;

  float scale = 1.0f;
  const float axisRadius = 0.05f;
  const float axisLength = 0.55f;

  float getGlobalAxisRadius() const { return axisRadius * scale; }
  float getGlobalAxisLength() const { return axisLength * scale; }

  TranslationGizmo() {
    for (size_t i = 0; i < 6; i++) {
      handles[i].userData = (void *)i;
      handles[i].callbacks = this;
    }
  }

  void update(InputContext &inputContext) {
    float3x3 invTransform = linalg::inverse(extractRotationMatrix(transform));
    float3 localRayDir = linalg::mul(invTransform, inputContext.rayDirection);

    for (size_t i = 0; i < 6; i++) {
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
      if (i < 3) {
      min = (-t1 * getGlobalAxisRadius() - t2 * getGlobalAxisRadius()) * hitboxScale.x + fwd * getGlobalAxisLength() * hitboxScale.y * 0.6f;
      max =
          (t1 * getGlobalAxisRadius() + t2 * getGlobalAxisRadius()) * hitboxScale.x + fwd * getGlobalAxisLength() * hitboxScale.y * 0.8f;
      } else {
        // TODO
        float hitboxSize = 0.15f;
        float hitboxThickness = 0.05f;
        switch (i) {
          case 3:
            min = float3(0, 0, hitboxThickness);
            max = float3(hitboxSize, hitboxSize, -hitboxThickness);
            break;
          case 4:
            min = float3(-hitboxThickness, 0, hitboxSize);
            max = float3(hitboxThickness, hitboxSize, 0);
            break;
          case 5:
            min = float3(0, -hitboxThickness, hitboxSize);
            max = float3(hitboxSize, hitboxThickness, 0);
            break;
        }
      }

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

  virtual void grabbed(InputContext &context, Handle &handle) {
    dragStartTransform = transform;

    size_t index = getHandleIndex(handle);
    SPDLOG_DEBUG("Handle {} ({}) grabbed", index, getAxisDirection(index, dragStartTransform));

    if (index < 3) {
    dragStartPoint = hitOnPlane(context.eyeLocation, context.rayDirection, extractTranslation(dragStartTransform),
                                getAxisDirection(index, dragStartTransform));
    } else {
      // TODO
      switch (index) {
        case 3:
          dragStartPoint = hitOnPlaneUnprojected(context.eyeLocation, context.rayDirection, extractTranslation(dragStartTransform), getAxisDirection(2, dragStartTransform));
          break;
        case 4:
          dragStartPoint = hitOnPlaneUnprojected(context.eyeLocation, context.rayDirection, extractTranslation(dragStartTransform), getAxisDirection(0, dragStartTransform));
          break;
        case 5:
          dragStartPoint = hitOnPlaneUnprojected(context.eyeLocation, context.rayDirection, extractTranslation(dragStartTransform), getAxisDirection(1, dragStartTransform));
          break;
      }
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
      float3 hitPoint;
      switch (index) {
        case 3:
          hitPoint = hitOnPlaneUnprojected(context.eyeLocation, context.rayDirection, extractTranslation(dragStartTransform), getAxisDirection(2, dragStartTransform));
          break;
        case 4:
          hitPoint = hitOnPlaneUnprojected(context.eyeLocation, context.rayDirection, extractTranslation(dragStartTransform), getAxisDirection(0, dragStartTransform));
          break;
        case 5:
          hitPoint = hitOnPlaneUnprojected(context.eyeLocation, context.rayDirection, extractTranslation(dragStartTransform), getAxisDirection(1, dragStartTransform));
          break;
      }
      float3 delta = hitPoint - dragStartPoint;
      transform = linalg::mul(linalg::translation_matrix(delta), dragStartTransform);
    }
  }

  void render(InputContext &inputContext, GizmoRenderer &renderer) {
    for (size_t i = 0; i < 6; i++) {
      auto &handle = handles[i];

      bool hovering = inputContext.hovering && inputContext.hovering == &handle;

// #if 0
      // Debug draw
      float4 color = float4(.7, .7, .7, 1.);
      uint32_t thickness = 1;
      if (hovering) {
        color = float4(.5, 1., .5, 1.);
        thickness = 2;
      }

      auto &min = handle.selectionBox.min;
      auto &max = handle.selectionBox.max;
      float3 center = (max + min) / 2.0f;
      float3 size = max - min;

      renderer.getShapeRenderer().addBox(handle.selectionBoxTransform, center, size, color, thickness);
// #endif

      float3 loc = extractTranslation(handle.selectionBoxTransform);
      float3 dir = getAxisDirection(i, handle.selectionBoxTransform);
      float4 axisColor = axisColors[i];
      axisColor = float4(axisColor.xyz() * (hovering ? 1.1f : 0.9f), 1.0f);
      if (i < 3) {
      renderer.addHandle(loc, dir, getGlobalAxisRadius(), getGlobalAxisLength(), axisColor, GizmoRenderer::CapType::Arrow,
                         axisColor);
      } else {
        float3 center;
        float3 xBase;
        float3 yBase;
        float2 size = float2(0.15f, 0.15f);
        float4 color;
        switch (i) {
          case 3:
            // handle on the xy-plane
            xBase = float3(1.0f, 0.0f, 0.0f);
            yBase = float3(0.0f, 1.0f, 0.0f);
            center = loc + float3(size.x / 2, size.y / 2, 0.0f);
            color = float4(axisColors[2].xyz() * (hovering ? 1.1f : 0.9f), 0.8f);
            break;
          case 4:
            // handle on the yz-plane
            xBase = float3(0.0f, 1.0f, 0.0f);
            yBase = float3(0.0f, 0.0f, 1.0f);
            center = loc + float3(0.0f, size.x / 2, size.y / 2);
            color = float4(axisColors[0].xyz() * (hovering ? 1.1f : 0.9f), 0.8f);
            break;
          case 5:
            // handle on the xz-plane
            xBase = float3(0.0f, 0.0f, 1.0f);
            yBase = float3(1.0f, 0.0f, 0.0f);
            center = loc + float3(size.y / 2, 0.0f, size.x / 2);
            color = float4(axisColors[1].xyz() * (hovering ? 1.1f : 0.9f), 0.8f);
            break;
        }
        uint32_t thickness = 1;
        renderer.getShapeRenderer().addSolidRect(center, xBase, yBase, size, color, thickness);
      }
    }
  }
};
} // namespace gizmos
} // namespace gfx

#endif /* BCCA14A9_4E3A_4D04_B838_CE099ABD609E */
