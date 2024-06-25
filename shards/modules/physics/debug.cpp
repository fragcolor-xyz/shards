#include <shards/modules/gfx/shards_types.hpp>
#include <gfx/gizmos/shapes.hpp>
#include <gfx/drawables/mesh_drawable.hpp>
#include <gfx/mesh.hpp>
#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>
#include "core.hpp"
#include <Jolt/Renderer/DebugRenderer.h>

namespace shards::Physics {

struct DebugRenderer : public JPH::DebugRenderer {
  struct GFXBatch : public JPH::RefTargetVirtual {
    gfx::MeshPtr mesh;

    virtual void AddRef() override { refCount.fetch_add(1, std::memory_order::relaxed); }
    virtual void Release() override {
      if (--refCount == 0)
        delete this;
    }

  private:
    std::atomic_uint32_t refCount = 0;
  };

  gfx::ShapeRenderer &shapeRenderer;
  std::vector<gfx::DrawablePtr> drawables;

  DebugRenderer(gfx::ShapeRenderer &sr) : shapeRenderer(sr) { Initialize(); }

  void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override {
    shapeRenderer.addLine(toLinalg(inFrom), toLinalg(inTo), toLinalgLinearColor(inColor), 1.0f);
  }
  void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor,
                    ECastShadow inCastShadow = ECastShadow::Off) override {
    shapeRenderer.addSolidTriangle(toLinalg(inV1), toLinalg(inV2), toLinalg(inV3), toLinalgLinearColor(inColor));
  }
  void DrawText3D(JPH::RVec3Arg inPosition, const std::string_view &inString, JPH::ColorArg inColor = JPH::Color::sWhite,
                  float inHeight = 0.5f) override {
    // NOOP
  }

  Batch CreateTriangleBatch(const Triangle *inTriangles, int inTriangleCount) override {
    gfx::MeshPtr mesh = std::make_shared<gfx::Mesh>();
    gfx::MeshFormat fmt;
    fmt.primitiveType = gfx::PrimitiveType::TriangleList;
    fmt.vertexAttributes = {
        gfx::MeshVertexAttribute("position", 3, gfx::StorageType::Float32),
        gfx::MeshVertexAttribute("normal", 3, gfx::StorageType::Float32),
        gfx::MeshVertexAttribute("texCoord0", 2, gfx::StorageType::Float32),
        gfx::MeshVertexAttribute("color", 4, gfx::StorageType::UNorm8),
    };
    mesh->update(fmt, inTriangles, sizeof(Vertex) * 3 * inTriangleCount, nullptr, 0);

    GFXBatch *batch = new GFXBatch();
    batch->mesh = mesh;
    return batch;
  }

  Batch CreateTriangleBatch(const Vertex *inVertices, int inVertexCount, const uint32_t *inIndices, int inIndexCount) override {
    gfx::MeshPtr mesh = std::make_shared<gfx::Mesh>();
    gfx::MeshFormat fmt;
    fmt.primitiveType = gfx::PrimitiveType::TriangleList;
    fmt.indexFormat = gfx::IndexFormat::UInt32;
    fmt.vertexAttributes = {
        gfx::MeshVertexAttribute("position", 3, gfx::StorageType::Float32),
        gfx::MeshVertexAttribute("normal", 3, gfx::StorageType::Float32),
        gfx::MeshVertexAttribute("texCoord0", 2, gfx::StorageType::Float32),
        gfx::MeshVertexAttribute("color", 4, gfx::StorageType::UNorm8),
    };
    mesh->update(fmt, inVertices, inVertexCount * sizeof(Vertex), inIndices, inIndexCount * sizeof(uint32_t));

    GFXBatch *batch = new GFXBatch();
    batch->mesh = mesh;
    return batch;
  }

  void DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox &inWorldSpaceBounds, float inLODScaleSq,
                    JPH::ColorArg inModelColor, const GeometryRef &inGeometry, ECullMode inCullMode = ECullMode::CullBackFace,
                    ECastShadow inCastShadow = ECastShadow::On, EDrawMode inDrawMode = EDrawMode::Solid) override {
    auto lod = inGeometry->mLODs.front().mTriangleBatch;
    GFXBatch *gfxBatch = dynamic_cast<GFXBatch *>(lod.GetPtr());
    float4x4 transform{
        toLinalg(inModelMatrix.GetColumn4(0)),
        toLinalg(inModelMatrix.GetColumn4(1)),
        toLinalg(inModelMatrix.GetColumn4(2)),
        toLinalg(inModelMatrix.GetColumn4(3)),
    };
    auto drawable = std::make_shared<gfx::MeshDrawable>(gfxBatch->mesh, transform);
    drawables.push_back(drawable);
  }
};

struct DebugDrawShard {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR(""); }

  static inline Type QueueVarType = Type::VariableOf(gfx::ShardsTypes::DrawQueue);

  PARAM_PARAMVAR(_context, "Context", "The context", {ShardsContext::VarType});
  PARAM_PARAMVAR(_queue, "Queue", "Graphics queue", {QueueVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_context), PARAM_IMPL_FOR(_queue));

  gfx::ShapeRenderer _shapeRenderer;
  std::shared_ptr<DebugRenderer> _renderer;

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _renderer = std::make_shared<DebugRenderer>(_shapeRenderer);
  }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &context = varAsObjectChecked<ShardsContext>(_context.get(), ShardsContext::Type);
    auto &queue = varAsObjectChecked<gfx::SHDrawQueue>(_queue.get(), gfx::ShardsTypes::DrawQueue);

    _shapeRenderer.begin();

    auto &sys = context.core->getPhysicsSystem();
    JPH::BodyManager::DrawSettings drawSettings;
    sys.DrawBodies(drawSettings, _renderer.get());

    _shapeRenderer.end(queue.queue);

    for (auto &drawable : _renderer->drawables) {
      queue.queue->add(drawable);
    }
    _renderer->drawables.clear();

    return input;
  }
};

} // namespace shards::Physics

SHARDS_REGISTER_FN(debug) {
  using namespace shards::Physics;
  REGISTER_SHARD("Physics.DebugDraw", DebugDrawShard);
}