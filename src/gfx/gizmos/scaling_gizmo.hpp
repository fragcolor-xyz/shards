#ifndef IDENTIFIER_TO_BE_REPLACED
#define IDENTIFIER_TO_BE_REPLACED

#include "gizmos.hpp"
#include <gfx/linalg.hpp>

namespace gfx {
namespace gizmos {
struct ScalingGizmo : public IGizmo, public IGizmoCallbacks {
  float4x4 transform = linalg::identity; // load identity matrix

  Handle handles[4];
  const float sensitivity = 1.0f;
  float4x4 dragStartTransform;
  float3 dragStartPoint;

  float scale = 1.0f;
  const float axisRadius = 0.05f;
  const float axisLength = 0.55f;

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

      // the fwd vector represent the axis direction of the handle
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
      if (i == 3) {
        float half_size = 0.1f;
        // min = float3(-half_size, -half_size, -half_size);
        // max = float3(half_size, half_size, half_size);
        if (min == max) {
          min = float3(-half_size, -half_size, -half_size);
          max = float3(half_size, half_size, half_size);
        } else {
          min = float3(-half_size, -half_size, -half_size) * hitboxScale.x;
          max = float3(half_size, half_size, half_size) * hitboxScale.y;
        }
        // SPDLOG_DEBUG("min: {}, max: {}", min, max);
      } else {
      min = (-t1 * getGlobalAxisRadius() - t2 * getGlobalAxisRadius()) * hitboxScale.x;
      max =
          (t1 * getGlobalAxisRadius() + t2 * getGlobalAxisRadius()) * hitboxScale.x + fwd * getGlobalAxisLength() * hitboxScale.y;
      }
      handle.selectionBoxTransform = transform;

      inputContext.updateHandle(handle);
    }
  }

  // void update(InputContext& inputContext) {
  //   for (size_t i = 0; i < 4; i++) {
  //     auto& handle = handles[i];

  //     // Retrieve the original hitbox dimensions and transform
  //     const auto& originalMin = handle.selectionBox.min;
  //     const auto& originalMax = handle.selectionBox.max;
  //     const auto& originalTransform = handle.selectionBoxTransform;

  //     // Update the transform of the handle
  //     handle.selectionBoxTransform = transform;

  //     // Restore the original hitbox dimensions
  //     handle.selectionBox.min = originalMin;
  //     handle.selectionBox.max = originalMax;

  //     // Update the input context for the handle
  //     inputContext.updateHandle(handle);
  //   }
  // }

  size_t getHandleIndex(Handle &inHandle) { return size_t(inHandle.userData); }

  float3 getAxisDirection(size_t index, float4x4 transform) {
    float3 base{};
    base[index] = 1.0f;
    return linalg::normalize(linalg::mul(transform, float4(base, 0)).xyz());
  }

  virtual void grabbed(InputContext &context, Handle &handle) {
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

  // this is executed to "move" the handle when dragged
  virtual void move(InputContext &context, Handle &inHandle) {
    size_t index = getHandleIndex(inHandle);
    float3 fwd = getAxisDirection(index, dragStartTransform);
    float3 hitPoint = hitOnPlane(context.eyeLocation, context.rayDirection, dragStartPoint, fwd);
    // SPDLOG_DEBUG("Handle {} ({}) moved to {}", index, fwd, hitPoint);

    float3 delta = hitPoint - dragStartPoint;
    float3 scaling;
    // if (index == 3) {
    //   // Grabbing the centered cube - scale all sides uniformly
    //   float uniformScaling = std::min(std::abs(delta.x), std::min(std::abs(delta.y), std::abs(delta.z)));
    //   scaling = float3(uniformScaling);
    // } else {
    //   // Grabbing a cube on the axis - scale in one direction
    //   scaling = float3(delta.x * sensitivity, delta.y * sensitivity, delta.z * sensitivity);
    // }
    // scaling = float3(1, 1, 1 * std::abs(delta.z * sensitivity));
    // SPDLOG_DEBUG("Scaling: {}", scaling);
    switch (index)
    {
    case 0: 
      scaling = float3(1 + delta.x * sensitivity,1 ,1);
      break;
    case 1:
      scaling = float3(1, 1 + delta.y * sensitivity, 1);
      break;
    case 2:
      scaling = float3(1, 1, 1 + delta.z * sensitivity);
      break;
    case 3:
      scaling = float3(1 + delta.x * sensitivity, 1 + delta.y * sensitivity, 1 + delta.z * sensitivity);
      break;
    }
    transform = linalg::mul(linalg::scaling_matrix(scaling), dragStartTransform);
  }

  void render(InputContext &inputContext, GizmoRenderer &renderer) {
    for (size_t i = 0; i < 4; i++) {
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

      // the origin of the handle
      float3 loc = extractTranslation(handle.selectionBoxTransform);
      // the direction of the handle
      float3 dir = getAxisDirection(i, handle.selectionBoxTransform);
      float4 cubeColor;
      if (i == 3) {
        cubeColor = float4(0.3f, 0.3f, 0.3f, 1.0f);
      } else {
        cubeColor = axisColors[i];
      }
      // cubeColor = axisColors[i];
      cubeColor = float4(cubeColor.xyz() * (hovering ? 1.1f : 0.9f), 1.0f);
      // addHandle(float3 origin, float3 direction, float radius, float length, float4 bodyColor, CapType capType, float4 capColor)
      // renderer.addHandle(loc, dir, getGlobalAxisRadius(), getGlobalAxisLength(), cubeColor, GizmoRenderer::CapType::Cube,
      //                    cubeColor);
      if (i == 3) {
        // SPDLOG_DEBUG("Adding centered cube handle");
        renderer.addCubeHandle(float3(0, 0, 0), 0.08f, cubeColor);
      } else {
        renderer.addHandle(loc, dir, getGlobalAxisRadius(), getGlobalAxisLength(), cubeColor, GizmoRenderer::CapType::Cube,
                         cubeColor);
      }
    }
  }
};
} // namespace gizmos
} // namespace gfx

#endif /* IDENTIFIER_TO_BE_REPLACED */
