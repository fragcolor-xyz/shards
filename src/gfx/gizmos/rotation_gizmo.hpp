#ifndef ROTATION_GIZMO_HPP
#define ROTATION_GIZMO_HPP

#include "gizmos.hpp"
#include <gfx/linalg.hpp>

namespace gfx {
namespace gizmos {
// A gizmo that allows the rotation of an object about the 3 axes. The gizmo is composed of 3
// handles, each of which allows rotation about a single axis.
//
// Note: While only a single handle of the RotationGizmo may be selected at any point in time,
// handles from multiple different gizmos may be selected at the same time and manipulated.
// In such a case, do be forewarned that some awkward behaviour may occur as multiple gizmos
// are not expected to be active and used at the same time.
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
    // float3x3 invTransform = linalg::inverse(extractRotationMatrix(transform));
    // float3 localRayDir = linalg::mul(invTransform, inputContext.rayDirection);

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
    float3x2 screenSpaceBaseVec = inputContext.getScreenSpacePlaneAxes();
    handleSelectionDiscs[3].normal = linalg::normalize(linalg::cross(screenSpaceBaseVec.x, screenSpaceBaseVec.y));
    handleSelectionDiscs[3].xBase = screenSpaceBaseVec.x;
    handleSelectionDiscs[3].yBase = screenSpaceBaseVec.y;

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

  float3 getAxisDirection(size_t index, float4x4 transform) {
    return handleSelectionDiscs[index].normal;
  }

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

      // Construct a plane whose normal is the vector from hitPoint to context.eyeLocation and project tangent onto this plane
      dragStartPoint = hitPoint;
      dragTangentDir = linalg::normalize(linalg::cross(selectionDisc.normal, radiusVec));

      dragNormalDir = linalg::normalize(context.eyeLocation - hitPoint);
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
    if (intersectPlane(context.eyeLocation, context.rayDirection, selectionDisc.center, dragNormalDir, d)) {
      float3 hitPoint = context.eyeLocation + d * context.rayDirection;
      float3 deltaVec = dragStartPoint - hitPoint;

      float delta = linalg::dot(deltaVec, dragTangentDir);
      // Use this to check direction vector of tangent and delta for the effective distance moved along the tangent
      // SPDLOG_DEBUG("dragTangentDir {} {} {}", dragTangentDir.x, dragTangentDir.y, dragTangentDir.z);
      // SPDLOG_DEBUG("Delta: {}", delta);
      float sinTheta = std::sin(delta);
      float cosTheta = std::cos(delta);

      float4x4 rotationMat;

      switch (handleIndex) {
      case 0:
        // rotating about x-axis
        rotationMat = float4x4{
            {1, 0, 0, 0},
            {0, cosTheta, -sinTheta, 0},
            {0, sinTheta, cosTheta, 0},
            {0, 0, 0, 1},
        };
        break;
      case 1:
        // rotating about y-axis
        rotationMat = float4x4{
            {cosTheta, 0, sinTheta, 0},
            {0, 1, 0, 0},
            {-sinTheta, 0, cosTheta, 0},
            {0, 0, 0, 1},
        };
        break;
      case 2:
        // rotating about z-axis
        rotationMat = float4x4{
            {cosTheta, -sinTheta, 0, 0},
            {sinTheta, cosTheta, 0, 0},
            {0, 0, 1, 0},
            {0, 0, 0, 1},
        };
        break;
      case 3:
        // rotating about screen-space axis
        float x = selectionDisc.normal.x;
        float y = selectionDisc.normal.y;
        float z = selectionDisc.normal.z;
        float x2 = x * x;
        float y2 = y * y;
        float z2 = z * z;
        float xy = x * y;
        float xz = x * z;
        float yz = y * z;

        // Construct the rotation matrix
        rotationMat = {
          {x2 + (1 - x2) * cosTheta, xy * (1 - cosTheta) - z * sinTheta, xz * (1 - cosTheta) + y * sinTheta, 0},
          {xy * (1 - cosTheta) + z * sinTheta, y2 + (1 - y2) * cosTheta, yz * (1 - cosTheta) - x * sinTheta, 0},
          {xz * (1 - cosTheta) - y * sinTheta, yz * (1 - cosTheta) + x * sinTheta, z2 + (1 - z2) * cosTheta, 0},
          {0, 0, 0, 1}
        };
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
      // The xbase and ybase of each disc is the normal of the other discs (for discs 1-3)
      renderer.getShapeRenderer().addDisc(selectionDisc.center, selectionDisc.xBase, selectionDisc.yBase,
                                          selectionDisc.outerRadius, selectionDisc.innerRadius, axisColor);
    }
  }
};
} // namespace gizmos
} // namespace gfx

#endif