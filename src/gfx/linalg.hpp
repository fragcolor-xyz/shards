#ifndef GFX_LINALG
#define GFX_LINALG

#include <linalg/linalg.h>
#include <cstring>

namespace gfx {
using namespace linalg::aliases;

// tighlty packs the matrix into dst with sizeof(float)*16
inline void packFloat4x4(const float4x4 &mat, float *dst) {
  for (int i = 0; i < 4; i++) {
    memcpy(&dst[i * 4], &mat[i].x, sizeof(float) * 4);
  }
}

// compute the near and far intersections of the cube (stored in the x and y components) using the slab method
// no intersection means vec.x > vec.y (really tNear > tFar)
inline float2 intersectAABB(const float3 &rayOrigin, const float3 &rayDir, float3 boxMin, float3 boxMax) {
  // https://gist.github.com/DomNomNom/46bb1ce47f68d255fd5d
  float3 tMin = (boxMin - rayOrigin) / rayDir;
  float3 tMax = (boxMax - rayOrigin) / rayDir;
  float3 t1 = linalg::min(tMin, tMax);
  float3 t2 = linalg::max(tMin, tMax);
  float tNear = linalg::max(linalg::max(t1.x, t1.y), t1.z);
  float tFar = linalg::min(linalg::min(t2.x, t2.y), t2.z);
  return float2(tNear, tFar);
};

// Compute intersection of a ray with a plane
// Returns distance in t
inline bool intersectPlane(const float3 &rayOrigin, const float3 &rayDir, const float3 &planePoint, const float3 &planeNormal,
                           float &outDistance) {
  // https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-plane-and-ray-disk-intersection
  const float epsilon = 1e-6;
  float denom = linalg::dot(planeNormal, rayDir);
  if (denom > epsilon) {
    float3 toPlane = planePoint - rayOrigin;
    outDistance = linalg::dot(toPlane, planeNormal) / denom;
    return (outDistance >= 0);
  }

  return false;
}

inline float3 extractTranslation(const float4x4 &transform) { return transform.w.xyz(); }

inline void decomposeTRS(const float4x4 &transform, float3 &translation, float3 scale, float3x3 &rotationMatrix) {
  translation = extractTranslation(transform);
  float3 x = transform.x.xyz();
  float3 y = transform.y.xyz();
  float3 z = transform.z.xyz();
  scale.x = linalg::length(x);
  scale.y = linalg::length(y);
  scale.z = linalg::length(z);
  rotationMatrix = float3x3(x / scale.x, y / scale.y, z / scale.z);
}

} // namespace gfx

#endif // GFX_LINALG
