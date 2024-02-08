#ifndef AD244946_690B_4770_A0A2_D2AF76FFBBD0
#define AD244946_690B_4770_A0A2_D2AF76FFBBD0

#include "gizmos.hpp"
#include "../trs.hpp"
#include "../linalg.hpp"

namespace gfx {
namespace gizmos {

struct SelectionDisc {
  Torus torus;
  float3 baseX;
  float3 baseY;
};

// A gizmo that allows the rotation of an object about the 3 axes. The gizmo is composed of 3
// handles, each of which allows rotation about a single axis.
//
// Note: If multiple gizmos are to be active at any time, ensure that they are created in the same Gizmos.Context
//       This is to prevent multiple handles from different gizmos being selected at the same time,
//       resulting in unexpected behaviour.
struct RotationGizmo : public IGizmo, public IGizmoCallbacks {
public:
  TRS pivot;
  boost::container::small_vector<TRS, 16> transforms;
  boost::container::small_vector<TRS, 16> movedTransforms;

  float scale = 1.0f;

private:
  float4x4 pivotTransform;

  Handle handles[4];
  SelectionDisc handleSelectionDiscs[4];
  float4x4 dragStartTransform;
  float4x4 transformNoScale;
  float3 dragStartPoint;
  float3 dragTangentDir;
  float3 dragNormalDir;
  boost::container::small_vector<TRS, 16> startTransforms;

  const float outerRadius = 1.0f;
  const float innerRadius = 0.9f;
  float getInnerRadius() const { return innerRadius * scale; }
  float getOuterRadius() const { return outerRadius * scale; }

public:
  RotationGizmo() {
    for (int i = 0; i < 3; ++i) {
      handles[i].userData = reinterpret_cast<void *>(i);
      handles[i].callbacks = this;
    }

    // special values for 4th handle
    handles[3].userData = reinterpret_cast<void *>(3);
    handles[3].callbacks = this;
  }

  // update from IGizmo, seems to update gizmo based on inputcontext
  // updates mainly the hitbox for the handle and calls updateHandle to check if the handle is selected (via raycasting)
  virtual void update(InputContext &inputContext) {
    movedTransforms.clear();
    pivotTransform = pivot.getMatrix();

    // Rotate the 3 discs for x/y/z-axis according to the current transform of the object
    // TODO: Currently identity matrix
    //   Make this work correctly in local-transform mode
    transformNoScale = linalg::translation_matrix(extractTranslation(pivotTransform));
    for (int i = 0; i < 3; ++i) {
      handleSelectionDiscs[i].torus.normal = transformNoScale[i].xyz();
      handleSelectionDiscs[i].baseX = transformNoScale[(i + 1) % 3].xyz();
      handleSelectionDiscs[i].baseY = transformNoScale[(i + 2) % 3].xyz();
      handleSelectionDiscs[i].torus.outerRadius = getOuterRadius();
      handleSelectionDiscs[i].torus.innerRadius = getInnerRadius();
    }

    // Update 4th handle's rotation based on any changes to camera transform
    float3x3 screenSpaceBaseVec = inputContext.getScreenSpacePlaneAxes();
    handleSelectionDiscs[3].baseX = screenSpaceBaseVec[0];
    handleSelectionDiscs[3].baseY = screenSpaceBaseVec[1];
    handleSelectionDiscs[3].torus.normal = screenSpaceBaseVec[2];
    handleSelectionDiscs[3].torus.outerRadius = getOuterRadius() * 1.1;
    handleSelectionDiscs[3].torus.innerRadius = getInnerRadius() * 1.1;

    // Update the center of all 4 handles, check for intersection with ray and update handle
    for (size_t i = 0; i < 3; i++) {
      auto &handle = handles[i];
      auto &selectionDisc = handleSelectionDiscs[i];

      selectionDisc.torus.center = extractTranslation(transformNoScale);

      // Torus transform
      float4x4 torusRotM = rotationFromZDirection(selectionDisc.torus.normal);
      float4x4 torusTransform = linalg::mul(transformNoScale, torusRotM);
      float torusR = (selectionDisc.torus.innerRadius + selectionDisc.torus.outerRadius) / 2.0f;
      float torusW = (selectionDisc.torus.outerRadius - selectionDisc.torus.innerRadius) / 2.0f;

      // Make hitbox slightly larger
      torusW *= 1.5f;
      float hitDistance = intersectImplicitSurfaceTransformed(
          inputContext.eyeLocation, inputContext.rayDirection, torusTransform,
          [&](float3 p) -> float { return linalg::length(float2(linalg::length(float2(p.x, p.y)) - torusR, p.z)) - torusW; });

      inputContext.updateHandle(handle, GizmoHit(hitDistance));
    }
  }

  size_t getHandleIndex(Handle &inHandle) { return size_t(inHandle.userData); }

  float3 getAxisDirection(size_t index, float4x4 transform) { return handleSelectionDiscs[index].torus.normal; }

  // Called when handle grabbed from IGizmoCallbacks
  virtual void grabbed(InputContext &context, Handle &handle) {
    dragStartTransform = pivotTransform;
    startTransforms = transforms;

    size_t index = getHandleIndex(handle);
    SPDLOG_DEBUG("Handle {} ({}) grabbed", index, getAxisDirection(index, dragStartTransform));

    auto &selectionDisc = handleSelectionDiscs[index];
    float d;
    // Check if ray intersects with plane of the disc (either side)
    // TODO: Check if there is a way to do it without checking both sides of the plane
    if (intersectPlane(context.eyeLocation, context.rayDirection, selectionDisc.torus.center, selectionDisc.torus.normal, d) ||
        intersectPlane(context.eyeLocation, context.rayDirection, selectionDisc.torus.center, -selectionDisc.torus.normal, d)) {
      float3 hitPoint = context.eyeLocation + d * context.rayDirection;
      float3 radiusVec = hitPoint - selectionDisc.torus.center;

      // Construct a plane that is parallel to the camera view projeciton and project the tangent onto this plane
      dragStartPoint = hitPoint;
      dragTangentDir = linalg::normalize(linalg::cross(selectionDisc.torus.normal, radiusVec));

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
    if (intersectPlane(context.eyeLocation, context.rayDirection, selectionDisc.torus.center, dragNormalDir, d) ||
        intersectPlane(context.eyeLocation, context.rayDirection, selectionDisc.torus.center, -dragNormalDir, d)) {
      float3 hitPoint = context.eyeLocation + d * context.rayDirection;
      float3 deltaVec = dragStartPoint - hitPoint;

      float delta = -linalg::dot(deltaVec, dragTangentDir);
      // Use this to check direction vector of tangent and delta for the effective distance moved along the tangent
      // SPDLOG_DEBUG("dragTangentDir {} {} {}", dragTangentDir.x, dragTangentDir.y, dragTangentDir.z);
      // SPDLOG_DEBUG("Delta: {}", delta);

      float3 axis{};
      switch (handleIndex) {
      case 0:
      case 1:
      case 2:
        axis[handleIndex] = 1.0f;
        break;
      case 3:
        float3x3 startRotation = extractRotationMatrix(dragStartTransform);
        axis = linalg::normalize(linalg::mul(linalg::inverse(startRotation), dragNormalDir));
        break;
      }

      float4 qRotation = linalg::rotation_quat(axis, delta);
      for (size_t i = 0; i < startTransforms.size(); i++) {
        movedTransforms.push_back(startTransforms[i].rotatedAround(extractTranslation(dragStartTransform), qRotation));
      }
    }
  }

  // render from IGizmo
  virtual void render(InputContext &inputContext, GizmoRenderer &renderer) {

    // render all 4 selection discs
    for (size_t i = 0; i < 3; i++) {
      auto &handle = handles[i];
      auto &selectionDisc = handleSelectionDiscs[i];
      bool hovering = inputContext.hovering && inputContext.hovering == &handle;

      float4 axisColor = axisColors[i];
      float4 fillColor = float4(axisColor.xyz() * (hovering ? 1.1f : 0.2f), hovering ? 1.0f : 0.2f);
      // Render without culling so it can be seen from either sides
      renderer.getShapeRenderer().addDisc(selectionDisc.torus.center, selectionDisc.baseX, selectionDisc.baseY,
                                          selectionDisc.torus.outerRadius, selectionDisc.torus.innerRadius, fillColor, false);

      renderer.getShapeRenderer().addCircle(selectionDisc.torus.center, selectionDisc.baseX, selectionDisc.baseY,
                                            selectionDisc.torus.innerRadius, axisColor, 2, 64);
      renderer.getShapeRenderer().addCircle(selectionDisc.torus.center, selectionDisc.baseX, selectionDisc.baseY,
                                            selectionDisc.torus.outerRadius, axisColor, 2, 64);
    }
  }
};
} // namespace gizmos
} // namespace gfx

#endif /* AD244946_690B_4770_A0A2_D2AF76FFBBD0 */
