#ifndef B78C3249_E9BB_4F1D_B18E_01BD247682B8
#define B78C3249_E9BB_4F1D_B18E_01BD247682B8

#include "../linalg.hpp"

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

// Returns the straight-line distance from the eye to the intersection point of the disc's plane.
// Returns infinity if the ray does not intersect the plane.
inline float intersectTorus(const float3 &eyeLocation, const float3 &rayDirection, const Torus &torus) {
  float3 oc = eyeLocation - torus.center;
  float a = linalg::dot(rayDirection, rayDirection);
  float b = 2.0f * linalg::dot(oc, rayDirection);
  float c = linalg::dot(oc, oc) - torus.outerRadius * torus.outerRadius - torus.innerRadius * torus.innerRadius;
  float discriminant = b * b - 4 * a * c;

  if (discriminant < 0) {
    return std::numeric_limits<float>::infinity(); // No intersection
  } else {
    float t1 = (-b + sqrt(discriminant)) / (2.0f * a);
    float t2 = (-b - sqrt(discriminant)) / (2.0f * a);

    // Check if t1 and t2 are within the bounds of the torus
    float3 intersection1 = eyeLocation + t1 * rayDirection;
    float3 intersection2 = eyeLocation + t2 * rayDirection;

    float distance1 = linalg::length(intersection1 - torus.center);
    float distance2 = linalg::length(intersection2 - torus.center);

    if (distance1 >= torus.innerRadius && distance1 <= torus.outerRadius) {
      return t1; // Intersection at t1
    } else if (distance2 >= torus.innerRadius && distance2 <= torus.outerRadius) {
      return t2; // Intersection at t2
    } else {
      return std::numeric_limits<float>::infinity(); // No intersection within torus bounds
    }
  }
}

template <typename F, typename J = std::is_invocable_r<float, F, float3>::type>
inline float intersectImplicitSurface(const float3 &eyeLocation, const float3 &rayDirection, F f, size_t numIterations = 64,
                                      float epsilon = 0.001f) {
  float3 p = eyeLocation;
  float d = FLT_MAX;
  for (size_t i = 0; i < numIterations; i++) {
    float d2 = f(p);
    if (d2 > d && (d2 - d) > 0.15f)
      break; // Passed surface, early-out
    d = d2;
    if (d < epsilon) {
      return linalg::length(p - eyeLocation);
    }
    p += d * 0.85f * rayDirection;
  }
  return std::numeric_limits<float>::infinity();
}

template <typename F, typename J = std::is_invocable_r<float, F, float3>::type>
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

inline float3 transformPosition(float4x4 m, float3 pos) {
  float4 p = linalg::mul(m, float4(pos, 1.0f));
  return p.xyz() / p.w;
}

inline float3 transformNormal(float4x4 m, float3 pos) {
  // auto mit = linalg::transpose(linalg::inverse(m));
  float4 p = linalg::mul(m, float4(pos, 0.0f));
  return linalg::normalize(p.xyz());
}

} // namespace gizmos
} // namespace gfx

#endif /* B78C3249_E9BB_4F1D_B18E_01BD247682B8 */
