#ifndef B78C3249_E9BB_4F1D_B18E_01BD247682B8
#define B78C3249_E9BB_4F1D_B18E_01BD247682B8

#include "../linalg.hpp"

namespace gfx {
namespace gizmos {

struct Box {
  float3 min{};
  float3 max{};
  float4x4 transform{};
};

struct Disc {
  float3 center{};
  float3 normal{};
  float outerRadius{};
  float innerRadius{};
};

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

// Returns the straight-line distance from the eye to the intersection point
// Returns infinity if the ray does not intersect the box
inline float intersectBox(const float3 &eyeLocation, const float3 &rayDirection, const Box &box) {
  float4x4 invHandleTransform = linalg::inverse(box.transform);
  float3 rayLoc1 = linalg::mul(invHandleTransform, float4(eyeLocation, 1)).xyz();
  float3 rayDir1 = linalg::mul(invHandleTransform, float4(rayDirection, 0)).xyz();
  float2 hit = intersectAABB(rayLoc1, rayDir1, box.min, box.max);

  // If there is an intersection with the box
  if (hit.x <= hit.y) {
    return hit.x;
  }

  return std::numeric_limits<float>::infinity();
}

// Returns the straight-line distance from the eye to the intersection point of the disc's plane.
// Returns infinity if the ray does not intersect the plane.
inline float intersectDisc(const float3 &eyeLocation, const float3 &rayDirection, const Disc &disc) {
  float d;
  if (intersectPlane(eyeLocation, rayDirection, disc.center, disc.normal, d) || 
    intersectPlane(eyeLocation, rayDirection, disc.center, -disc.normal, d)) {
    float3 hitPoint = eyeLocation + d * rayDirection;
    float3 hitVector = hitPoint - disc.center;
    float hitDist = linalg::length(hitVector);
    // if hit is within the disc
    if (hitDist <= disc.outerRadius && hitDist >= disc.innerRadius) {
      return d;
    }
  }
  return std::numeric_limits<float>::infinity();
}
} // namespace gizmos
} // namespace gfx

#endif /* B78C3249_E9BB_4F1D_B18E_01BD247682B8 */
