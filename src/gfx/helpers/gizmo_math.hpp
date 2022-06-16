#ifndef B78C3249_E9BB_4F1D_B18E_01BD247682B8
#define B78C3249_E9BB_4F1D_B18E_01BD247682B8

#include "../linalg.hpp"

namespace gfx {
namespace gizmos {
// Projects a point onto a 3D line by projecting it
inline float3 projectOntoAxis(float3 point, float3 axisPoint, float3 axisForward) {
  float3 T0 = float3(-axisForward.y, -axisForward.z, axisForward.x);
  float3 T1 = linalg::cross(T0, axisForward);
  return point - (linalg::dot(point, T0) - linalg::dot(axisPoint, T0)) * T0 -
         (linalg::dot(point, T1) - linalg::dot(axisPoint, T1)) * T1;
}

// Intersects a view ray with an axis plane based on eye location
// this is used for draging along axis handles
inline float3 hitOnPlane(float3 eyeLocation, float3 rayDirection, float3 axisPoint, float3 axisForward) {
  float3 planeT0 = axisForward;
  float3 planeNormal = axisPoint - eyeLocation;
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
} // namespace gizmos
} // namespace gfx

#endif /* B78C3249_E9BB_4F1D_B18E_01BD247682B8 */
