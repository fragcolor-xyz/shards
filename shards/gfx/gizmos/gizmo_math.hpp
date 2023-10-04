#ifndef B78C3249_E9BB_4F1D_B18E_01BD247682B8
#define B78C3249_E9BB_4F1D_B18E_01BD247682B8

#include "../linalg.hpp"
#include "../math.hpp"

namespace gfx {
namespace gizmos {

struct Box {
  float3 min{};
  float3 max{};
  float4x4 transform{};

  void fix() {
    auto newMin = linalg::min(min, max);
    max = linalg::max(min, max);
    min = newMin;
  }
  float3 getCenter() const { return (min + max) / 2.0f; }
  float3 getSize() const { return max - min; }
};

struct Disc {
  float3 center{};
  float3 xBase{};
  float3 yBase{};
  float3 normal{};
  float outerRadius{};
  float innerRadius{};
};

struct Torus {
  float3 center{};
  float3 normal{};
  float innerRadius{};
  float outerRadius{};
};

inline float4x4 removeTransformScale(float4x4 mat) {
  const float3 x = mat.x.xyz();
  const float3 y = mat.y.xyz();
  const float3 z = mat.z.xyz();
  mat.x.xyz() = x / linalg::length(x);
  mat.y.xyz() = y / linalg::length(y);
  mat.z.xyz() = z / linalg::length(z);
  return mat;
}

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
  if (intersectPlane(eyeLocation, rayDirection, disc.center, disc.normal, d)) {
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

inline float3 transformPosition(float4x4 m, float3 pos) {
  float4 p = linalg::mul(m, float4(pos, 1.0f));
  return p.xyz() / p.w;
}

inline float3 transformNormal(float4x4 m, float3 pos) {
  // auto mit = linalg::transpose(linalg::inverse(m));
  float4 p = linalg::mul(m, float4(pos, 0.0f));
  return linalg::normalize(p.xyz());
}

template <typename F, typename J = std::enable_if_t<std::is_invocable_r<float, F, float3>::value>>
inline float intersectImplicitSurface(const float3 &eyeLocation, const float3 &rayDirection, F f, size_t numIterations = 64,
                                      float epsilon = 0.001f) {
  const float maxStepIncreaseEarlyOut = 1.0f;
  const float maxDistanceEarlyOut = 100.0f;
  float t = 0.0f;
  float d = std::numeric_limits<float>::max();
  for (size_t i = 0; i < numIterations; i++) {
    float3 p = eyeLocation + rayDirection * t;
    float d2 = f(p);
    if (d2 > d && ((t > maxDistanceEarlyOut) || (d2 - d) > maxStepIncreaseEarlyOut))
      break; // Passed surface, early-out
    d = d2;
    if (d < epsilon) {
      return linalg::length(p - eyeLocation);
    }
    t += d * 0.85f;
  }
  return std::numeric_limits<float>::infinity();
}

template <typename F, typename J = std::enable_if_t<std::is_invocable_r<float, F, float3>::value>>
inline float intersectImplicitSurfaceTransformed(float3 eyeLocation, float3 rayDirection, const float4x4 &transform, F f,
                                                 size_t numIterations = 64, float epsilon = 0.001f) {
  float4x4 invTransform = linalg::inverse(transform);
  eyeLocation = transformPosition(invTransform, eyeLocation);
  rayDirection = transformNormal(invTransform, rayDirection);
  return intersectImplicitSurface(eyeLocation, rayDirection, f, numIterations, epsilon);
}

// Intersects a view ray with a plane based on eye location
// this is used for draging handles within a single plane
inline float3 hitOnPlaneUnprojected(float3 eyeLocation, float3 rayDirection, float3 planePoint, float3 planeNormal) {
  float d;
  if (intersectPlane(eyeLocation, rayDirection, planePoint, planeNormal, d)) {
    return eyeLocation + d * rayDirection;
  }

  return planePoint;
}

} // namespace gizmos
} // namespace gfx

#endif /* B78C3249_E9BB_4F1D_B18E_01BD247682B8 */
