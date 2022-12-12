#ifndef GFX_LINALG
#define GFX_LINALG

#include <linalg/linalg.h>
#include <cstring>
#include "math.hpp"

namespace gfx {
using namespace linalg::aliases;

// Returns the byte size of a tighlty packed matrix based on input parameter
template<typename T, int M, int N>
inline constexpr size_t getPackedMatrixSize(const linalg::mat<T, M, N> &_unused) {
  return sizeof(T) * M * N;
}

// tighlty packs the matrix into dst
template<typename T, int M, int N>
inline void packMatrix(const linalg::mat<T, M, N> &mat, T *dst) {
  for (int i = 0; i < M; i++) {
    memcpy(&dst[i * M], &mat[i].x, sizeof(T) * N);
  }
}

// compute the near and far intersections of the cube (stored in the x and y components) using the slab method
// no intersection means vec.x > vec.y (really tNear > tFar)
inline float2 intersectAABB(const float3 &rayOrigin, const float3 &rayDir, const float3 &boxMin, const float3 &boxMax) {
  // https://gist.github.com/DomNomNom/46bb1ce47f68d255fd5d
  const float3 tMin = (boxMin - rayOrigin) / rayDir;
  const float3 tMax = (boxMax - rayOrigin) / rayDir;
  const float3 t1 = linalg::min(tMin, tMax);
  const float3 t2 = linalg::max(tMin, tMax);
  const float tNear = linalg::max(linalg::max(t1.x, t1.y), t1.z);
  const float tFar = linalg::min(linalg::min(t2.x, t2.y), t2.z);
  return float2(tNear, tFar);
};

// Compute intersection of a ray with a plane
// Returns distance between rayOrigin and the intersection along rayDir in outDistance
inline bool intersectPlane(const float3 &rayOrigin, const float3 &rayDir, const float3 &pointOnPlane, const float3 &planeNormal,
                           float &outDistance) {
  // https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-plane-and-ray-disk-intersection
  const float epsilon = 1e-6;
  float denom = linalg::dot(-planeNormal, rayDir);
  if (denom > epsilon) {
    float3 toPlane = pointOnPlane - rayOrigin;
    outDistance = linalg::dot(toPlane, -planeNormal) / denom;
    return (outDistance >= 0);
  }

  return false;
}

inline float3 extractTranslation(const float4x4 &transform) { return transform.w.xyz(); }

inline void decomposeTRS(const float4x4 &transform, float3 &translation, float3 &scale, float3x3 &rotationMatrix) {
  translation = extractTranslation(transform);
  const float3 x = transform.x.xyz();
  const float3 y = transform.y.xyz();
  const float3 z = transform.z.xyz();
  scale.x = linalg::length(x);
  scale.y = linalg::length(y);
  scale.z = linalg::length(z);
  rotationMatrix = float3x3(x / scale.x, y / scale.y, z / scale.z);
}

inline float3 generateTangent(const float3 &direction) {
  const float tolerance = 0.05f;
  float3 tangent;
  if (isRoughlyEqual(std::abs(direction.y), 1.0f, tolerance)) {
    tangent = linalg::normalize(float3(-direction.y, direction.x, direction.z));
  } else {
    tangent = linalg::normalize(float3(-direction.z, direction.y, direction.x));
  }
  return tangent;
}

inline float4x4 rotationFromXDirection(const float3 &direction) {
  const float3 right = direction;
  const float3 fwd = generateTangent(right);
  const float3 up = linalg::normalize(linalg::cross(fwd, right));
  return float4x4(float4(right, 0), float4(up, 0), float4(fwd, 0), float4(0, 0, 0, 1));
}

} // namespace gfx

#endif // GFX_LINALG
