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

  float getGlobalAxisRadius() const { return axisRadius * scale; }
  float getGlobalAxisLength() const { return axisLength * scale; }

  RotationGizmo() {
    for (int i = 0; i < 3; ++i) {
      handles[i].userData = reinterpret_cast<void *>(i);
      handles[i].callbacks = this;
      handleSelectionDiscs[i].outerRadius = 1.0f;
      handleSelectionDiscs[i].innerRadius = 0.9f;
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

      float3 fwd{};
      fwd[i] = 1.0f;

      //// might not actually require a scaling on the rotation ring yet

      // float3 t1 = float3(-fwd.z, -fwd.x, fwd.y);
      // float3 t2 = linalg::cross(fwd, t1);

      // Slightly decrease hitbox size if view direction is parallel
      // e.g. looking towards +z you are less likely to want to click on the z axis
      float dotThreshold = 0.8f;
      float angleFactor = std::max(0.0f, (linalg::abs(linalg::dot(localRayDir, fwd)) - dotThreshold) / (1.0f - dotThreshold));

      // Make hitboxes slightly bigger than the actual visuals
      // const float2 hitboxScale = linalg::lerp(float2(2.2f, 1.2f), float2(0.8f, 1.0f), angleFactor);

      // what is left is to adjust whether it should scale the radius less or more
      // currently its same scale as for a box, but scaling radius may have a lot more impact
      /*auto& innerRadius = handle.selectionBox.innerRadius;
      auto& outerRadius = handle.selectionBox.outerRadius;*/
      // innerRadius = (-t1 * getGlobalAxisRadius() - t2 * getGlobalAxisRadius()) * hitboxScale.x;
      // outerRadius = (t1 * getGlobalAxisRadius() + t2 * getGlobalAxisRadius()) * hitboxScale.x +
      //               fwd * getGlobalAxisLength() * hitboxScale.y;

      ///// may want to update this in the future to allow rotation of the gizmos themselves
      // selectionDisc.transform = transform;

      float hitDistance = intersectDisc(inputContext.eyeLocation, inputContext.rayDirection, selectionDisc);
      inputContext.updateHandle(handle, hitDistance);

      // auto &min = handle.selectionBox.min;
      // auto &max = handle.selectionBox.max;
      // min = (-t1 * getGlobalAxisRadius() - t2 * getGlobalAxisRadius()) * hitboxScale.x;
      // max =
      //     (t1 * getGlobalAxisRadius() + t2 * getGlobalAxisRadius()) * hitboxScale.x + fwd * getGlobalAxisLength() *
      //     hitboxScale.y;

      // handle.selectionBoxTransform = transform;
      selectionDisc.center = extractTranslation(transform);

      // inputContext.updateHandle(handle);
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
    if (intersectPlane(context.eyeLocation, context.rayDirection, selectionDisc.center, selectionDisc.normal, d)) {
      float3 hitPoint = context.eyeLocation + d * context.rayDirection;
      float3 radiusVec = hitPoint - selectionDisc.center;

      // solnA using the tangent to the disc and the normal of the disc to form the plane of rotation
      dragStartPoint = hitPoint;
      dragTangentDir = linalg::normalize(linalg::cross(selectionDisc.normal, radiusVec));

      // solnB using the plane normal to the eye location as the plane of rotation, where tangent is projected onto this plane
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

  /*
    for x-axis: x axis is to the right, means rotation circle is on y-z plane
    only half the circle needs to be coloured, based on the direction of the camera
    once clicked, find the point on the circle clicked, then project new mouse position onto the tangent on the point of the
    circle then find the angle between the tangent and the x axis, and rotate the object by that angle rotate anti-clockwise:
      click left, pull down
      click top pull left
      click right pull up
      click bottom pull right
    rotate clockwise: opposite of the above.

    for unity, allow rotation of degrees over 360 and with negative values, not wrapping around (like in unreal)
    for unity, they also have a 4th circle that can rotate the cube around the plane perpendicular to the camera
  */

  // Called every update while handle is being held from IGizmoCallbacks
  virtual void move(InputContext &context, Handle &handle) {

    auto handleIndex = getHandleIndex(handle);
    auto &selectionDisc = handleSelectionDiscs[handleIndex];

    float d;
    // if (intersectPlane(context.eyeLocation, context.rayDirection, selectionDisc.center, selectionDisc.normal, d)) {
    if (intersectPlane(context.eyeLocation, context.rayDirection, selectionDisc.center, dragNormalDir, d)) {
      float3 hitPoint = context.eyeLocation + d * context.rayDirection;
      float3 deltaVec = dragStartPoint - hitPoint;

      float delta = linalg::dot(deltaVec, dragTangentDir);
      SPDLOG_DEBUG("dragTangentDir {} {} {}", dragTangentDir.x, dragTangentDir.y, dragTangentDir.z);
      SPDLOG_DEBUG("Delta: {}", delta);
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

      // for the delta, depending on how far left/top or right/bottom, change the amount of rotation
      transform = linalg::mul(dragStartTransform, rotationMat);
    }
  }

  // render from IGizmo
  virtual void render(InputContext &inputContext, GizmoRenderer &renderer) {
    float3 axisDirs[]{{1.0, 0, 0}, {0, 1.0, 0}, {0, 0, 1.0}};

    for (size_t i = 0; i < 3; i++) {
      auto &handle = handles[i];
      auto &selectionDisc = handleSelectionDiscs[i];

      bool hovering = inputContext.hovering && inputContext.hovering == &handle;

      float3 center = extractTranslation(transform);
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