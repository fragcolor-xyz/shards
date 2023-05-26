#ifndef B78C3249_E9BB_4F1D_B18E_01BD247682B8
#define B78C3249_E9BB_4F1D_B18E_01BD247682B8

#include "../linalg.hpp"

namespace gfx {
namespace gizmos {
// Projects a point onto a 3D line by projecting it
inline float3 projectOntoAxis(float3 point, float3 axisPoint, float3 axisForward) {
  float3 T0 = generateTangent(axisForward);
  float3 T1 = linalg::cross(T0, axisForward);
  return point - (linalg::dot(point, T0) - linalg::dot(axisPoint, T0)) * T0 -
         (linalg::dot(point, T1) - linalg::dot(axisPoint, T1)) * T1;
}

// Intersects a view ray with an axis plane based on eye location
// this is used for draging along axis handles
inline float3 hitOnPlane(float3 eyeLocation, float3 rayDirection, float3 axisPoint, float3 axisForward) {
  float3 planeT0 = axisForward;
  float3 planeNormal = eyeLocation - axisPoint;
  planeNormal -= linalg::dot(planeNormal, planeT0) * planeT0;
  planeNormal = linalg::normalize(planeNormal);

  float d;
  if (intersectPlane(eyeLocation, rayDirection, axisPoint, planeNormal, d)) {
    float3 hitPoint = eyeLocation + d * rayDirection;
    hitPoint = projectOntoAxis(hitPoint, axisPoint, axisForward);
    return hitPoint;
  }

  return axisPoint;
}

// Intersects a view ray with a plane based on eye location
// this is used for draging handles within a single plane
inline float3 hitOnPlaneUnprojected(float3 eyeLocation, float3 rayDirection, float3 planePoint, float3 planeNormal) {
  // float3 planeT0 = planeNormal;
  // float3 planeNormal = eyeLocation - planePoint;
  // planeNormal -= linalg::dot(planeNormal, planeT0) * planeT0;
  // planeNormal = linalg::normalize(planeNormal);

  float d;
  if (intersectPlane(eyeLocation, rayDirection, planePoint, planeNormal, d)) {
    return eyeLocation + d * rayDirection;
  }

  return planePoint;
}

} // namespace gizmos
} // namespace gfx

#endif /* B78C3249_E9BB_4F1D_B18E_01BD247682B8 */
