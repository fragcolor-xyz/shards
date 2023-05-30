#ifndef ROTATION_GIZMO_HPP
#define ROTATION_GIZMO_HPP

#include "gizmos.hpp"
#include <gfx/linalg.hpp>

namespace gfx {
namespace gizmos {
// A gizmo that allows the rotation of an object about the 3 axes. The gizmo is composed of 3
// handles, each of which allows rotation about a single axis.
//
// Note: If multiple gizmos are to be active at any time, ensure that they are created in the same Gizmos.Context
//       This is to prevent multiple handles from different gizmos being selected at the same time,
//       resulting in unexpected behaviour.
struct RotationGizmo : public IGizmo, public IGizmoCallbacks {
  float4x4 transform = linalg::identity;

  Handle handles[4];
  Disc handleSelectionDiscs[4];
  float4x4 dragStartTransform;
  float3 dragStartPoint;
  float3 dragTangentDir;
  float3 dragNormalDir;

  float scale = 1.0f;
  const float axisRadius = 0.05f;
  const float axisLength = 0.55f;
  const float initialOuterRadius = 1.0f;
  const float initialInnerRadius = 0.9f;

  float getGlobalAxisRadius() const { return axisRadius * scale; }
  float getGlobalAxisLength() const { return axisLength * scale; }

  RotationGizmo() {
    float3x3 axisDirs{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
    for (int i = 0; i < 3; ++i) {
      handles[i].userData = reinterpret_cast<void *>(i);
      handles[i].callbacks = this;
      // Can possibly allow adjustment of radii in the future
      handleSelectionDiscs[i].outerRadius = initialOuterRadius;
      handleSelectionDiscs[i].innerRadius = initialInnerRadius;

      handleSelectionDiscs[i].normal = axisDirs[i];
      handleSelectionDiscs[i].xBase = axisDirs[(i + 1) % 3];
      handleSelectionDiscs[i].yBase = axisDirs[(i + 2) % 3];
    }

    // special values for 4th handle
    handles[3].userData = reinterpret_cast<void *>(3);
    handles[3].callbacks = this;
    handleSelectionDiscs[3].outerRadius = initialOuterRadius * 1.1;
    handleSelectionDiscs[3].innerRadius = initialInnerRadius * 1.1;

    handleSelectionDiscs[3].normal = {1.0, 0.0, 1.0};
    handleSelectionDiscs[3].xBase = {1.0, 0.0, -1.0};
    handleSelectionDiscs[3].yBase = {0.0, 1.0, 0.0};
  }

  // update from IGizmo, seems to update gizmo based on inputcontext
  // updates mainly the hitbox for the handle and calls updateHandle to check if the handle is selected (via raycasting)
  virtual void update(InputContext &inputContext) {
    // Rotate the 3 discs for x/y/z-axis according to the current transform of the object
    float3x3 rotationMat = extractRotationMatrix(transform);
    float3x3 axisDirs{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
    axisDirs = linalg::mul(axisDirs, rotationMat);
    for (int i = 0; i < 3; ++i) {
      handleSelectionDiscs[i].normal = axisDirs[i];
      handleSelectionDiscs[i].xBase = axisDirs[(i + 1) % 3];
      handleSelectionDiscs[i].yBase = axisDirs[(i + 2) % 3];
    }

    // Update 4th handle's rotation based on any changes to camera transform
    float3x3 screenSpaceBaseVec = inputContext.getScreenSpacePlaneAxes();
    handleSelectionDiscs[3].xBase = screenSpaceBaseVec[0];
    handleSelectionDiscs[3].yBase = screenSpaceBaseVec[1];
    handleSelectionDiscs[3].normal = screenSpaceBaseVec[2];

    // Update the center of all 4 handles, check for intersection with ray and update handle
    for (size_t i = 0; i < 4; i++) {
      auto &handle = handles[i];
      auto &selectionDisc = handleSelectionDiscs[i];

      selectionDisc.center = extractTranslation(transform);

      float hitDistance = intersectDisc(inputContext.eyeLocation, inputContext.rayDirection, selectionDisc);
      inputContext.updateHandle(handle, hitDistance);
    }
  }

  size_t getHandleIndex(Handle &inHandle) { return size_t(inHandle.userData); }

  float3 getAxisDirection(size_t index, float4x4 transform) { return handleSelectionDiscs[index].normal; }

  // Called when handle grabbed from IGizmoCallbacks
  virtual void grabbed(InputContext &context, Handle &handle) {
    dragStartTransform = transform;

    size_t index = getHandleIndex(handle);
    SPDLOG_DEBUG("Handle {} ({}) grabbed", index, getAxisDirection(index, dragStartTransform));

    auto &selectionDisc = handleSelectionDiscs[index];
    float d;
    // Check if ray intersects with plane of the disc (either side)
    // TODO: Check if there is a way to do it without checking both sides of the plane
    if (intersectPlane(context.eyeLocation, context.rayDirection, selectionDisc.center, selectionDisc.normal, d) ||
        intersectPlane(context.eyeLocation, context.rayDirection, selectionDisc.center, -selectionDisc.normal, d)) {
      float3 hitPoint = context.eyeLocation + d * context.rayDirection;
      float3 radiusVec = hitPoint - selectionDisc.center;

      // Construct a plane that is parallel to the camera view projeciton and project the tangent onto this plane
      dragStartPoint = hitPoint;
      dragTangentDir = linalg::normalize(linalg::cross(selectionDisc.normal, radiusVec));

      // we could possibly make this more efficient by projecting the tangent onto the 2d screen space and then
      // calculating delta for rotation using the amount moved by the mouse cursor along the tangent in screen space
      // instead of doing raycasting in 3d as we do now.

      dragNormalDir = context.getForwardVector(); // Get the normal vector to the camera/screen
      dragTangentDir = linalg::normalize(dragTangentDir - linalg::dot(dragTangentDir, dragNormalDir) * dragNormalDir);

      SPDLOG_DEBUG("Drag start point: {} {} {}", dragStartPoint.x, dragStartPoint.y, dragStartPoint.z);
    }
  }

  // Called when handle released from IGizmoCallbacks
  virtual void released(InputContext &context, Handle &handle) {
    size_t index = getHandleIndex(handle);
    SPDLOG_DEBUG("Handle {} ({}) released", index, getAxisDirection(index, dragStartTransform));
  }

  // Called every update while handle is being held from IGizmoCallbacks
  virtual void move(InputContext &context, Handle &handle) {

    auto handleIndex = getHandleIndex(handle);
    auto &selectionDisc = handleSelectionDiscs[handleIndex];

    float d;
    if (intersectPlane(context.eyeLocation, context.rayDirection, selectionDisc.center, dragNormalDir, d) ||
        intersectPlane(context.eyeLocation, context.rayDirection, selectionDisc.center, -dragNormalDir, d)) {
      float3 hitPoint = context.eyeLocation + d * context.rayDirection;
      float3 deltaVec = dragStartPoint - hitPoint;

      float delta = -linalg::dot(deltaVec, dragTangentDir);
      // Use this to check direction vector of tangent and delta for the effective distance moved along the tangent
      // SPDLOG_DEBUG("dragTangentDir {} {} {}", dragTangentDir.x, dragTangentDir.y, dragTangentDir.z);
      // SPDLOG_DEBUG("Delta: {}", delta);

      float4x4 rotationMat;
      float3 axis{};
      switch (handleIndex) {
      case 0:
      case 1:
      case 2:
        axis[handleIndex] = 1.0f;
        rotationMat = linalg::rotation_matrix(linalg::rotation_quat(axis, delta));
        break;
      case 3:
        float3x3 startRotation = extractRotationMatrix(dragStartTransform);
        axis = linalg::normalize(linalg::mul(linalg::inverse(startRotation), dragNormalDir));
        rotationMat = linalg::rotation_matrix(linalg::rotation_quat(axis, delta));
        break;
      }

      transform = linalg::mul(dragStartTransform, rotationMat);
    }
  }

  // render from IGizmo
  virtual void render(InputContext &inputContext, GizmoRenderer &renderer) {

    // render all 4 selection discs
    for (size_t i = 0; i < 4; i++) {
      auto &handle = handles[i];
      auto &selectionDisc = handleSelectionDiscs[i];
      bool hovering = inputContext.hovering && inputContext.hovering == &handle;

      float4 axisColor = axisColors[i];
      axisColor = float4(axisColor.xyz() * (hovering ? 1.1f : 0.9f), 1.0f);
      // Render without culling so it can be seen from either sides
      renderer.getShapeRenderer().addDisc(selectionDisc.center, selectionDisc.xBase, selectionDisc.yBase,
                                          selectionDisc.outerRadius, selectionDisc.innerRadius, axisColor, false);
    }
  }
};
} // namespace gizmos
} // namespace gfx

#endif