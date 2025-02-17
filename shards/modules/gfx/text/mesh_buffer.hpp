#ifndef AAADCC4F_0596_4543_90E8_8B30232C111D
#define AAADCC4F_0596_4543_90E8_8B30232C111D

#include <gfx/mesh.hpp>
#include <gfx/feature.hpp>
#include <shards/core/pool.hpp>
#include <vector>
#include <unordered_map>
#include "text_placer.hpp"

namespace gfx::text {

// Similar to ShapeRenderer::TextVertex but specialized for text
struct TextVertex {
  float position[3];
  float color[4] = {1, 1, 1, 1};
  float uv[2] = {};

  void setPosition(const float3 &position) { memcpy(this->position, &position.x, sizeof(float) * 3); }
  void setColor(const float4 &color) { memcpy(this->color, &color.x, sizeof(float) * 4); }
  void setUV(const float2 &uv) { memcpy(this->uv, &uv.x, sizeof(float) * 2); }

  static const std::vector<MeshVertexAttribute> &getAttributes();
};

struct MeshTexturePair {
  MeshPtr mesh;
  TexturePtr texture;
};

class MeshBuffer {
public:
  void begin() {
    pageVertices.clear();
    meshTexturePairs.clear();
    meshPool.recycle();
  }

  std::vector<MeshTexturePair> finalizeMeshes();

  // Convert TextPlacer quads into mesh data, matching ShapeRenderer::addText behavior
  struct TextParams {
    float3 offset;
    float3 right;
    float3 up;
    float4 color;
    float scale;
    float2 alignment{};
  };
  void appendText(const TextPlacer &placer, const TextParams &params);

private:
  std::unordered_map<TexturePtr, std::vector<TextVertex>> pageVertices;
  std::vector<MeshTexturePair> meshTexturePairs;
  shards::Pool<MeshPtr> meshPool;

  // Feature for text rendering (alpha blending etc)
  static FeaturePtr getTextFeature();
};

} // namespace gfx::text

#endif /* AAADCC4F_0596_4543_90E8_8B30232C111D */
