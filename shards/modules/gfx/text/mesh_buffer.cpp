#include "mesh_buffer.hpp"
#include "text_placer.hpp"
#include <gfx/feature.hpp>
#include <gfx/drawables/mesh_drawable.hpp>

namespace gfx::text {
const std::vector<MeshVertexAttribute> &TextVertex::getAttributes() {
  static std::vector<MeshVertexAttribute> attribs = []() {
    std::vector<MeshVertexAttribute> attribs;
    attribs.emplace_back("position", 3, StorageType::Float32);
    attribs.emplace_back("color", 4, StorageType::Float32);
    attribs.emplace_back("texCoord0", 2, StorageType::Float32);
    return attribs;
  }();
  return attribs;
}

FeaturePtr MeshBuffer::getTextFeature() {
  static auto feature = []() {
    auto r = std::make_shared<Feature>();
    r->state.set_blend(BlendState{.color = BlendComponent::Alpha, .alpha = BlendComponent::Opaque});
    return r;
  }();
  return feature;
}

std::vector<MeshTexturePair> MeshBuffer::finalizeMeshes() {
  std::vector<MeshTexturePair> result;

  for (auto &[texture, vertices] : pageVertices) {
    if (vertices.empty())
      continue;

    auto meshDrawable = meshPool.newValue();

    MeshFormat fmt = {
        .primitiveType = PrimitiveType::TriangleList,
        .windingOrder = WindingOrder::CW,
        .vertexAttributes = TextVertex::getAttributes(),
    };

    meshDrawable->update(fmt, vertices.data(), vertices.size() * sizeof(TextVertex), nullptr, 0);

    result.push_back({meshDrawable, texture});
  }

  return result;
}

void MeshBuffer::appendText(const TextPlacer &placer, const TextParams &params) {
  auto &[offset, right, up, color, scale, alignment] = params;
  float3 pos = offset;

  // Handle centering if needed
  if (alignment.x != 0.0f) {
    float alignX = alignment.x * placer.getSize().x;
    pos += -alignX * right;
  }
  if (alignment.y != 0.0f) {
    float alignY = alignment.y * placer.getSize().y;
    pos += alignY * up;
  }

  // Convert each quad into triangles using the same vertex pattern as ShapeRenderer
  for (const auto &quad : placer.textQuads) {
    float3 a = pos + quad.quad.x * right + quad.quad.y * -up;
    float3 b = pos + quad.quad.z * right + quad.quad.y * -up; // +X
    float3 c = pos + quad.quad.z * right + quad.quad.w * -up; // +XY
    float3 d = pos + quad.quad.x * right + quad.quad.w * -up; // +Y

    float2 ta = {quad.uv.x, quad.uv.y};
    float2 tb = {quad.uv.z, quad.uv.y};
    float2 tc = {quad.uv.z, quad.uv.w};
    float2 td = {quad.uv.x, quad.uv.w};

    // Add vertices in the same order as ShapeRenderer
    TextVertex v;
    v.setColor(color);

    v.setPosition(a);
    v.setUV(ta);
    pageVertices[quad.texture].push_back(v);

    v.setPosition(b);
    v.setUV(tb);
    pageVertices[quad.texture].push_back(v);

    v.setPosition(c);
    v.setUV(tc);
    pageVertices[quad.texture].push_back(v);

    v.setPosition(d);
    v.setUV(td);
    pageVertices[quad.texture].push_back(v);

    v.setPosition(a);
    v.setUV(ta);
    pageVertices[quad.texture].push_back(v);

    v.setPosition(c);
    v.setUV(tc);
    pageVertices[quad.texture].push_back(v);
  }
}

} // namespace gfx::text