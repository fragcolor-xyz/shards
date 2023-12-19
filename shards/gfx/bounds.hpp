#ifndef A1665234_253E_49DD_B428_FA0D8802DD91
#define A1665234_253E_49DD_B428_FA0D8802DD91

#include "linalg.hpp"
#include "mesh.hpp"
#include <cfloat>

namespace gfx {
struct AABounds {
  float3 min;
  float3 max;

  constexpr AABounds() = default;
  constexpr AABounds(float3 min, float3 max) : min(min), max(max) {}

  static AABounds empty() { return AABounds(float3(FLT_MAX), float3(-FLT_MAX)); }
  void expand(float3 pt) {
    min = linalg::min(min, pt);
    max = linalg::max(max, pt);
  }

  template <typename F> std::enable_if_t<std::is_invocable_r_v<bool, F, float3>> forEachPoint(F func) {
    if (!func(min))
      return;
    if (!func(float3(max.x, min.y, min.z)))
      return;
    if (!func(float3(min.x, max.y, min.z)))
      return;
  }
};

struct OrientedBounds {
  AABounds base;

private:
  float4x4 _transform;
  mutable std::optional<float4x4> _inverseTransform;

public:
  OrientedBounds() = default;
  OrientedBounds(AABounds base, float4x4 transform) : base(base), _transform(transform) {}

  float4x4 transform() const { return _transform; }
  float4x4 inverseTransform() const {
    if (!_inverseTransform.has_value()) {
      _inverseTransform = linalg::inverse(_transform);
    }
    return *_inverseTransform;
  }
};

inline AABounds getMeshBounds(MeshPtr mesh) {
  auto &fmt = mesh->getFormat();
  size_t offset{};
  AABounds bounds = AABounds::empty();

  std::optional<size_t> positionIndex;
  std::optional<size_t> positionOffset;
  size_t stride = fmt.getVertexSize();
  for (size_t i = 0; i < fmt.vertexAttributes.size(); i++) {
    auto &attrib = fmt.vertexAttributes[i];
    if (attrib.name == "position") {
      positionIndex = i;
      positionOffset = offset;
    }
    offset += attrib.numComponents * getStorageTypeSize(attrib.type);
  }

  if (!positionIndex.has_value()) {
    throw std::runtime_error("Mesh has no position attribute");
  }

  const uint8_t *basePtr = mesh->getVertexData().data() + *positionOffset;
  for (size_t i = 0; i < mesh->getNumVertices(); i++) {
    auto &pt = *(float3 *)(basePtr + stride * i);
    bounds.expand(pt);
  }

  return bounds;
}
} // namespace gfx

#endif /* A1665234_253E_49DD_B428_FA0D8802DD91 */
