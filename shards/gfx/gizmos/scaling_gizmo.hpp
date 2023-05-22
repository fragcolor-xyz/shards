#ifndef D62A84E5_196A_4759_825B_4E8E9C073EC6
#define D62A84E5_196A_4759_825B_4E8E9C073EC6

#include "gizmos.hpp"
#include <gfx/linalg.hpp>

namespace gfx {
namespace gizmos {
struct ScalingGizmo : public IGizmo, public IGizmoCallbacks {
  float4x4 transform = linalg::identity; // load identity matrix

  Handle handles[4];
  Box handleSelectionBoxes[4];

  // The sensitivities of the axes and the centered cube vary due to different methods of calculating delta.
  // The axes utilize the delta of the ray intersection with the axis plane, whereas the centered cube relies on the delta of the
  // cursor position.
  const float axisSensitivity = 1.5f;
  const float centeredCubeSensitivity = 0.003f;
  const float4 centeredCubeColor = float4(0.3f, 0.3f, 0.3f, 1.0f);
  float4x4 dragStartTransform;
  float3 dragStartPoint;
  float2 dragStartCursor;

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

      // The size of the selection box for the axis handles is reduced to avoid overlapping with the hitbox of the centered cube.
      selectionBox.min = (-t1 * getGlobalAxisRadius() - t2 * getGlobalAxisRadius()) * hitboxScale.x +
                         fwd * getGlobalAxisLength() * hitboxScale.y * (angleFactor + 0.18f);
      selectionBox.max =
          (t1 * getGlobalAxisRadius() + t2 * getGlobalAxisRadius()) * hitboxScale.x + fwd * getGlobalAxisLength() * hitboxScale.y;

      // The selection box of the axis handle remains unchanged during object scaling.
      selectionBox.transform = linalg::identity;

      inputContext.updateHandle(handle, intersectBox(inputContext.eyeLocation, inputContext.rayDirection, selectionBox));
    }

    // The center cube
    {
      auto &handle = handles[3];
      auto &selectionBox = handleSelectionBoxes[3];

      float hitbox_size = getGlobalAxisLength() * 0.2f;
      selectionBox.min = float3(-hitbox_size, -hitbox_size, -hitbox_size);
      selectionBox.max = float3(hitbox_size, hitbox_size, hitbox_size);
      selectionBox.transform = linalg::identity;

      inputContext.updateHandle(handle, intersectBox(inputContext.eyeLocation, inputContext.rayDirection, selectionBox));
    }
  }

  size_t getHandleIndex(Handle &inHandle) { return size_t(inHandle.userData); }

  float3 getAxisDirection(size_t index, float4x4 transform) {
    float3 base{};
    base[index] = 1.0f;
    return linalg::normalize(linalg::mul(transform, float4(base, 0)).xyz());
  }

  /*
    The centred cube is implemented as a handle with a cube-shaped selection box.
    Its behavior draws inspiration from the scaling gizmo in Unity, where the direction of dragging determines how the object
    scales.

    Two different solutions for the centred cube were considered.
    The first solution involved intersecting the view ray with a plane parallel to the viewport and passing through the object's
    center. The second solution relied on the change in cursor position.

    The second solution was chosen for the following reasons:
      - The first solution proved more challenging to implement since the ray casting depended on the camera direction, which was
    not available in the input context.
      - The second solution offers a more intuitive reasoning as the direction of dragging is directly reflected in the cursor
    position changes.
  */
  virtual void grabbed(InputContext &context, Handle &handle) {
    handle.grabbed = true;
    dragStartTransform = transform;

    size_t index = getHandleIndex(handle);
    SPDLOG_DEBUG("Handle {} ({}) grabbed", index, getAxisDirection(index, dragStartTransform));

    if (index == 3) {
      dragStartCursor = context.inputState.cursorPosition;
    } else {
      dragStartPoint = hitOnPlane(context.eyeLocation, context.rayDirection, extractTranslation(dragStartTransform),
                                  getAxisDirection(index, dragStartTransform));
    }
  }

  virtual void released(InputContext &context, Handle &handle) {
    size_t index = getHandleIndex(handle);
    if (index == 3) {
      for (size_t i = 0; i < 3; i++) {
        handles[i].grabbed = false;
        handles[i].grabOffset = 0.0f;
      }
    } else {
      handle.grabbed = false;
      handle.grabOffset = 0.0f;
    }
    SPDLOG_DEBUG("Handle {} ({}) released", index, getAxisDirection(index, dragStartTransform));
  }

  virtual void move(InputContext &context, Handle &inHandle) {
    size_t index = getHandleIndex(inHandle);
    float3 fwd = getAxisDirection(index, dragStartTransform);

    float3 delta;
    float3 scaling;
    if (index == 3) {
      float2 hitPoint = context.inputState.cursorPosition;
      delta = float3((hitPoint - dragStartCursor), 0);
      for (size_t i = 0; i < 3; i++) {
        handles[i].grabbed = true;
        handles[i].grabOffset = centeredCubeSensitivity * delta.x;
      }
      // Dragging the centered cube to the right increases the object's scale, while dragging it to the left decreases the
      // object's scale.
      float diameter = centeredCubeSensitivity * std::abs(delta.x);
      if (delta.x > 0) {
        scaling = float3(1 + diameter, 1 + diameter, 1 + diameter);
      } else {
        scaling = float3(1 - diameter, 1 - diameter, 1 - diameter);
      }
    } else {
      float3 hitPoint = hitOnPlane(context.eyeLocation, context.rayDirection, dragStartPoint, fwd);
      delta = hitPoint - dragStartPoint;
      handles[index].grabOffset = delta[index];
      scaling = float3(1.0f);
      scaling[index] = 1 + delta[index] * axisSensitivity;
    }
    transform = linalg::mul(linalg::scaling_matrix(scaling), dragStartTransform);
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

      auto &min = handle.selectionBox.min;
      auto &max = handle.selectionBox.max;
      float3 center = (max + min) / 2.0f;
      float3 size = max - min;

      renderer.getShapeRenderer().addBox(handle.selectionBoxTransform, center, size, color, thickness);
#endif

      float3 loc = extractTranslation(selectionBox.transform);
      float3 dir = getAxisDirection(i, selectionBox.transform);

      float4 cubeColor = axisColors[i];
      cubeColor = float4(cubeColor.xyz() * (hovering ? 1.1f : 0.9f), 1.0f);

      if (handles[i].grabbed) {
        float modifiedLength = getGlobalAxisLength() + handles[i].grabOffset * 0.5f;

        if (modifiedLength < 0.0f) {
          renderer.addHandle(loc, -dir, getGlobalAxisRadius(), -modifiedLength, cubeColor, GizmoRenderer::CapType::Cube,
                             cubeColor);
        } else {
          renderer.addHandle(loc, dir, getGlobalAxisRadius(), modifiedLength, cubeColor, GizmoRenderer::CapType::Cube, cubeColor);
        }
      } else {
        renderer.addHandle(loc, dir, getGlobalAxisRadius(), getGlobalAxisLength(), cubeColor, GizmoRenderer::CapType::Cube,
                           cubeColor);
      }
    }

    // The centered cube
    {
      auto &handle = handles[3];
      auto &selectionBox = handleSelectionBoxes[3];

      bool hovering = inputContext.hovering && inputContext.hovering == &handle;

      float3 loc = extractTranslation(selectionBox.transform);
      float4 cubeColor = float4(centeredCubeColor.xyz() * (hovering ? 1.1f : 0.9f), 1.0f);

      renderer.addCubeHandle(loc, getGlobalAxisLength() * 0.15f, cubeColor);
    }
  }
};
} // namespace gizmos
} // namespace gfx

#endif /* D62A84E5_196A_4759_825B_4E8E9C073EC6 */
