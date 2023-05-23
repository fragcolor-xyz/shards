#ifndef TO_BE_REPLACED
#define TO_BE_REPLACED

#include "gizmos.hpp"
#include <gfx/linalg.hpp>

namespace gfx {
namespace gizmos {
struct RotationGizmo : public IGizmo, public IGizmoCallbacks {
  float4x4 transform = linalg::identity;

  Handle handles[3];
  Disc handleSelectionDiscs[3];
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
    for (int i = 0; i < 3; ++i) {
      handles[i].userData = reinterpret_cast<void *>(i);
      handles[i].callbacks = this;
      // Can possibly allow adjustment of radii in the future
      handleSelectionDiscs[i].outerRadius = initialOuterRadius;
      handleSelectionDiscs[i].innerRadius = initialInnerRadius;
      float3 normal{};
      normal[i] = 1.0f;
      handleSelectionDiscs[i].normal = normal;
    }
  }

  // update from IGizmo, seems to update gizmo based on inputcontext
  // updates mainly the hitbox for the handle and calls updateHandle to check if the handle is selected (via raycasting)
  virtual void update(InputContext &inputContext) {
    float3x3 invTransform = linalg::inverse(extractRotationMatrix(transform));
    float3 localRayDir = linalg::mul(invTransform, inputContext.rayDirection);

    for (size_t i = 0; i < 3; i++) {
      auto &handle = handles[i];
      auto &selectionDisc = handleSelectionDiscs[i];

      float hitDistance = intersectDisc(inputContext.eyeLocation, inputContext.rayDirection, selectionDisc);
      inputContext.updateHandle(handle, hitDistance);

      selectionDisc.center = extractTranslation(transform);
    }
  }

  size_t getHandleIndex(Handle &inHandle) { return size_t(inHandle.userData); }

  float3 getAxisDirection(size_t index, float4x4 transform) {
    float3 base{};
    base[index] = 1.0f;
    return linalg::normalize(linalg::mul(transform, float4(base, 0)).xyz());
  }

  // Called when handle grabbed from IGizmoCallbacks
  virtual void grabbed(InputContext &context, Handle &handle) {
    dragStartTransform = transform;

    size_t index = getHandleIndex(handle);
    SPDLOG_DEBUG("Handle {} ({}) grabbed", index, getAxisDirection(index, dragStartTransform));

    auto &selectionDisc = handleSelectionDiscs[index];
    float d;
    // Check if ray intersects with plane of the disc (either side)
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
      //SPDLOG_DEBUG("dragTangentDir {} {} {}", dragTangentDir.x, dragTangentDir.y, dragTangentDir.z);
      //SPDLOG_DEBUG("Delta: {}", delta);
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
      }

      transform = linalg::mul(dragStartTransform, rotationMat);
    }
  }

  // render from IGizmo
  virtual void render(InputContext &inputContext, GizmoRenderer &renderer) {

    // Rotate the discs according to the current transform of the object
    float3x3 rotationMat = extractRotationMatrix(transform);
    float3x3 axisDirs{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
    axisDirs = linalg::mul(axisDirs, rotationMat);

    for (size_t i = 0; i < 3; i++) {
      auto &handle = handles[i];
      auto &selectionDisc = handleSelectionDiscs[i];
      selectionDisc.normal = axisDirs[i]; // update the normal of the disc according to its new rotation

      bool hovering = inputContext.hovering && inputContext.hovering == &handle;

      float4 axisColor = axisColors[i];
      axisColor = float4(axisColor.xyz() * (hovering ? 1.1f : 0.9f), 1.0f);
      renderer.getShapeRenderer().addDisc(selectionDisc.center, axisDirs[(i + 1) % 3], axisDirs[(i + 2) % 3],
                                          selectionDisc.outerRadius, selectionDisc.innerRadius, axisColor);
    }
  }
};
} // namespace gizmos
} // namespace gfx

#endif /* TO_BE_REPLACED */