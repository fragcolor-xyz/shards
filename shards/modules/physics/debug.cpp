#include <shards/modules/gfx/shards_types.hpp>
#include <shards/modules/gfx/gfx.hpp>
#include <shards/modules/gfx/gizmos/context.hpp>
#include <shards/linalg_shim.hpp>
#include <gfx/gizmos/shapes.hpp>
#include <gfx/drawables/mesh_drawable.hpp>
#include <gfx/mesh.hpp>
#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>
#include "core.hpp"
#include <Jolt/Renderer/DebugRenderer.h>
#include <cmath>

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

  gfx::GizmoRenderer *gizmoRenderer;
  gfx::ShapeRenderer *shapeRenderer;
  gfx::WireframeRenderer *wireframeRenderer;
  std::vector<gfx::DrawablePtr> drawables;

  DebugRenderer() { Initialize(); }

  void setGizmoRenderer(gfx::GizmoRenderer &gizmoRenderer) {
    this->gizmoRenderer = &gizmoRenderer;
    this->shapeRenderer = &gizmoRenderer.getShapeRenderer();
  }

  void setWireframeRenderer(gfx::WireframeRenderer &wireframeRenderer) { this->wireframeRenderer = &wireframeRenderer; }

  void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override {
    shapeRenderer->addLine(toLinalg(inFrom), toLinalg(inTo), toLinalgLinearColor(inColor), 1.0f);
  }
  void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor,
                    ECastShadow inCastShadow = ECastShadow::Off) override {
    shapeRenderer->addSolidTriangle(toLinalg(inV1), toLinalg(inV2), toLinalg(inV3), toLinalgLinearColor(inColor));
  }
  void DrawText3D(JPH::RVec3Arg inPosition, const std::string_view &inString, JPH::ColorArg inColor = JPH::Color::sWhite,
                  float inHeight = 0.5f) override {
    float autoSize = gizmoRenderer->getConstantScreenSize(toLinalg(inPosition), inHeight * 14.0f * 5.0f);
    gizmoRenderer->addTextBillboard(toLinalg(inPosition), inString, toLinalgLinearColor(inColor), autoSize, true);
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
    float scale = gizmoRenderer->getConstantScreenSize(toLinalg(inWorldSpaceBounds.GetCenter()), 50.0f);
    float flod = std::clamp(1.0f - scale / float(inGeometry->mLODs.size() + 1), 0.0f, 1.0f);
    int lodIdx = std::floor(flod * (inGeometry->mLODs.size() - 1));

    auto lod = inGeometry->mLODs[lodIdx].mTriangleBatch;
    GFXBatch *gfxBatch = dynamic_cast<GFXBatch *>(lod.GetPtr());
    float4x4 transform{
        toLinalg(inModelMatrix.GetColumn4(0)),
        toLinalg(inModelMatrix.GetColumn4(1)),
        toLinalg(inModelMatrix.GetColumn4(2)),
        toLinalg(inModelMatrix.GetColumn4(3)),
    };
    if (inDrawMode == EDrawMode::Wireframe) {
      auto drawable = wireframeRenderer->getWireframeDrawable(gfxBatch->mesh, toLinalgLinearColor(inModelColor));
      drawable->transform = transform;
      drawables.push_back(drawable);
    } else {
      auto drawable = std::make_shared<gfx::MeshDrawable>(gfxBatch->mesh, transform);
      drawables.push_back(drawable);
    }
  }
};

DECL_ENUM_INFO(JPH::BodyManager::EShapeColor, PhysicsDebugShapeColor, 'phDc');
DECL_ENUM_INFO(JPH::ESoftBodyConstraintColor, PhysicsDebugSoftBodyConstraintColor, 'phSd');

struct DebugDrawShard {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR(""); }

  static inline Type QueueVarType = Type::VariableOf(gfx::ShardsTypes::DrawQueue);

  PARAM_PARAMVAR(_context, "Context", "The context", {ShardsContext::VarType});
  PARAM_PARAMVAR(_drawConstraints, "DrawConstraints", "Draw constraints", {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_drawConstraintLimits, "DrawConstraintLimits", "Draw constraint limits",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_drawConstraintReferenceFrames, "DrawConstraintReferenceFrames", "Draw constraint reference frames",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});

  PARAM_PARAMVAR(_drawBodies, "DrawBodies", "Draw bodies", {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_ds_getSupportFunction, "DrawBodyGetSupportFunction",
                 "Draw the GetSupport() function, used for convex collision detection",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_ds_supportDirection, "DrawBodySupportDirection",
                 "When drawing the support function, also draw which direction mapped to a specific support point",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_ds_getSupportingFace, "DrawBodyGetSupportingFace",
                 "Draw the faces that were found colliding during collision detection",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_ds_shape, "DrawBodyShape", "Draw the shapes of all bodies", {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_ds_shapeWireframe, "DrawBodyShapeWireframe",
                 "When mDrawShape is true and this is true, the shapes will be drawn in wireframe instead of solid.",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_ds_shapeColor, "DrawBodyShapeColor", "Coloring scheme to use for shapes",
                 {PhysicsDebugShapeColorEnumInfo::Type, Type::VariableOf(PhysicsDebugShapeColorEnumInfo::Type)});
  PARAM_PARAMVAR(_ds_boundingBox, "DrawBodyBoundingBox", "Draw a bounding box per body", {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_ds_centerOfMassTransform, "DrawBodyCenterOfMassTransform", "Draw the center of mass for each body",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_ds_worldTransform, "DrawBodyWorldTransform",
                 "Draw the world transform (which can be different than the center of mass) for each body",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_ds_velocity, "DrawBodyVelocity", "Draw the velocity vector for each body",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_ds_massAndInertia, "DrawBodyMassAndInertia", "Draw the mass and inertia (as the box equivalent) for each body",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_ds_sleepStats, "DrawBodySleepStats", "Draw stats regarding the sleeping algorithm of each body",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_ds_softBodyVertices, "DrawSoftBodyVertices", "Draw the vertices of soft bodies",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_ds_softBodyVertexVelocities, "DrawSoftBodyVertexVelocities",
                 "Draw the velocities of the vertices of soft bodies", {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_ds_softBodyEdgeConstraints, "DrawSoftBodyEdgeConstraints", "Draw the edge constraints of soft bodies",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_ds_softBodyBendConstraints, "DrawSoftBodyBendConstraints", "Draw the bend constraints of soft bodies",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_ds_softBodyVolumeConstraints, "DrawSoftBodyVolumeConstraints", "Draw the volume constraints of soft bodies",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_ds_softBodySkinConstraints, "DrawSoftBodySkinConstraints", "Draw the skin constraints of soft bodies",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_ds_softBodyLRAConstraints, "DrawSoftBodyLRAConstraints", "Draw the LRA constraints of soft bodies",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_ds_softBodyPredictedBounds, "DrawSoftBodyPredictedBounds", "Draw the predicted bounds of soft bodies",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_ds_softBodyConstraintColor, "DrawSoftBodyConstraintColor", "Coloring scheme to use for soft body constraints",
                 {PhysicsDebugSoftBodyConstraintColorEnumInfo::Type,
                  Type::VariableOf(PhysicsDebugSoftBodyConstraintColorEnumInfo::Type)});

  PARAM_IMPL(PARAM_IMPL_FOR(_context), PARAM_IMPL_FOR(_drawConstraints), PARAM_IMPL_FOR(_drawConstraintLimits),
             PARAM_IMPL_FOR(_drawConstraintReferenceFrames), PARAM_IMPL_FOR(_drawBodies), PARAM_IMPL_FOR(_ds_getSupportFunction),
             PARAM_IMPL_FOR(_ds_supportDirection), PARAM_IMPL_FOR(_ds_getSupportingFace), PARAM_IMPL_FOR(_ds_shape),
             PARAM_IMPL_FOR(_ds_shapeWireframe), PARAM_IMPL_FOR(_ds_shapeColor), PARAM_IMPL_FOR(_ds_boundingBox),
             PARAM_IMPL_FOR(_ds_centerOfMassTransform), PARAM_IMPL_FOR(_ds_worldTransform), PARAM_IMPL_FOR(_ds_velocity),
             PARAM_IMPL_FOR(_ds_massAndInertia), PARAM_IMPL_FOR(_ds_sleepStats), PARAM_IMPL_FOR(_ds_softBodyVertices),
             PARAM_IMPL_FOR(_ds_softBodyVertexVelocities), PARAM_IMPL_FOR(_ds_softBodyEdgeConstraints),
             PARAM_IMPL_FOR(_ds_softBodyBendConstraints), PARAM_IMPL_FOR(_ds_softBodyVolumeConstraints),
             PARAM_IMPL_FOR(_ds_softBodySkinConstraints), PARAM_IMPL_FOR(_ds_softBodyLRAConstraints),
             PARAM_IMPL_FOR(_ds_softBodyPredictedBounds), PARAM_IMPL_FOR(_ds_softBodyConstraintColor));

  shards::Gizmos::RequiredGizmoContext _gizmoContext;
  std::shared_ptr<DebugRenderer> _renderer;

  DebugDrawShard() {
    _drawConstraints = Var(true);
    _drawConstraintLimits = Var(false);
    _drawConstraintReferenceFrames = Var(false);
    _drawBodies = Var(true);
    _ds_getSupportFunction = Var(false);
    _ds_supportDirection = Var(false);
    _ds_getSupportingFace = Var(false);
    _ds_shape = Var(true);
    _ds_shapeWireframe = Var(false);
    _ds_shapeColor = Var::Enum(JPH::BodyManager::EShapeColor::MotionTypeColor, PhysicsDebugShapeColorEnumInfo::Type);
    _ds_boundingBox = Var(false);
    _ds_centerOfMassTransform = Var(false);
    _ds_worldTransform = Var(false);
    _ds_velocity = Var(false);
    _ds_massAndInertia = Var(false);
    _ds_sleepStats = Var(false);
    _ds_softBodyVertices = Var(false);
    _ds_softBodyVertexVelocities = Var(false);
    _ds_softBodyEdgeConstraints = Var(false);
    _ds_softBodyBendConstraints = Var(false);
    _ds_softBodyVolumeConstraints = Var(false);
    _ds_softBodySkinConstraints = Var(false);
    _ds_softBodyLRAConstraints = Var(false);
    _ds_softBodyPredictedBounds = Var(false);
    _ds_softBodyConstraintColor =
        Var::Enum(JPH::ESoftBodyConstraintColor::ConstraintType, PhysicsDebugSoftBodyConstraintColorEnumInfo::Type);
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _gizmoContext.warmup(context);
    _renderer = std::make_shared<DebugRenderer>();
  }
  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    _gizmoContext.cleanup();
    _renderer.reset();
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    _gizmoContext.compose(data, _requiredVariables);
    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &context = varAsObjectChecked<ShardsContext>(_context.get(), ShardsContext::Type);

    auto &gizmoRenderer = _gizmoContext->gfxGizmoContext.renderer;
    _renderer->setGizmoRenderer(gizmoRenderer);
    _renderer->setWireframeRenderer(_gizmoContext->wireframeRenderer);

    auto &sys = context.core->getPhysicsSystem();
    if (_drawBodies.get().payload.boolValue) {
      JPH::BodyManager::DrawSettings drawSettings;
      drawSettings.mDrawGetSupportFunction = _ds_getSupportFunction.get().payload.boolValue;
      drawSettings.mDrawSupportDirection = _ds_supportDirection.get().payload.boolValue;
      drawSettings.mDrawGetSupportingFace = _ds_getSupportingFace.get().payload.boolValue;
      drawSettings.mDrawShape = _ds_shape.get().payload.boolValue;
      drawSettings.mDrawShapeWireframe = _ds_shapeWireframe.get().payload.boolValue;
      drawSettings.mDrawShapeColor = JPH::BodyManager::EShapeColor(_ds_shapeColor.get().payload.enumValue);
      drawSettings.mDrawBoundingBox = _ds_boundingBox.get().payload.boolValue;
      drawSettings.mDrawCenterOfMassTransform = _ds_centerOfMassTransform.get().payload.boolValue;
      drawSettings.mDrawWorldTransform = _ds_worldTransform.get().payload.boolValue;
      drawSettings.mDrawVelocity = _ds_velocity.get().payload.boolValue;
      drawSettings.mDrawMassAndInertia = _ds_massAndInertia.get().payload.boolValue;
      drawSettings.mDrawSleepStats = _ds_sleepStats.get().payload.boolValue;
      drawSettings.mDrawSoftBodyVertices = _ds_softBodyVertices.get().payload.boolValue;
      drawSettings.mDrawSoftBodyVertexVelocities = _ds_softBodyVertexVelocities.get().payload.boolValue;
      drawSettings.mDrawSoftBodyEdgeConstraints = _ds_softBodyEdgeConstraints.get().payload.boolValue;
      drawSettings.mDrawSoftBodyBendConstraints = _ds_softBodyBendConstraints.get().payload.boolValue;
      drawSettings.mDrawSoftBodyVolumeConstraints = _ds_softBodyVolumeConstraints.get().payload.boolValue;
      drawSettings.mDrawSoftBodySkinConstraints = _ds_softBodySkinConstraints.get().payload.boolValue;
      drawSettings.mDrawSoftBodyLRAConstraints = _ds_softBodyLRAConstraints.get().payload.boolValue;
      drawSettings.mDrawSoftBodyPredictedBounds = _ds_softBodyPredictedBounds.get().payload.boolValue;
      drawSettings.mDrawSoftBodyConstraintColor =
          JPH::ESoftBodyConstraintColor(_ds_softBodyConstraintColor.get().payload.enumValue);
      sys.DrawBodies(drawSettings, _renderer.get());
    }
    if (_drawConstraints.get().payload.boolValue) {
      sys.DrawConstraints(_renderer.get());
    }
    if (_drawConstraintLimits.get().payload.boolValue) {
      sys.DrawConstraintLimits(_renderer.get());
    }
    if (_drawConstraintReferenceFrames.get().payload.boolValue) {
      sys.DrawConstraintReferenceFrame(_renderer.get());
    }

    for (auto &drawable : _renderer->drawables) {
      gizmoRenderer.getShapeRenderer().addCustom(drawable);
    }
    _renderer->drawables.clear();

    return input;
  }
};

} // namespace shards::Physics

SHARDS_REGISTER_FN(debug) {
  using namespace shards::Physics;
  REGISTER_ENUM(PhysicsDebugShapeColorEnumInfo);
  REGISTER_ENUM(PhysicsDebugSoftBodyConstraintColorEnumInfo);
  REGISTER_SHARD("Physics.DebugDraw", DebugDrawShard);
}