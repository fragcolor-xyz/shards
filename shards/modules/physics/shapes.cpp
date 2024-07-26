#include "../gfx/shards_types.hpp"
#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>
#include "physics.hpp"

#include <gfx/worker_memory.hpp>
#include <gfx/transform_updater.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

#include <shards/core/pmr/shared_temp_allocator.hpp>

namespace shards::Physics {

static auto logger = getLogger();

struct PointsBuilder {
  JPH::Array<JPH::Vec3> points;
  void addPoint(const JPH::Vec3 &point) { points.push_back(point); }

  void addDrawable(const gfx::SHDrawable &drawable) {
    std::visit(
        [&](auto &&arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, gfx::MeshDrawable::Ptr>) {
            this->addDrawable(arg, linalg::identity);
          } else if constexpr (std::is_same_v<T, gfx::MeshTreeDrawable::Ptr>) {
            this->addDrawable(arg);
          } else {
            throw SHException("Invalid input type");
          }
        },
        drawable.drawable);
  }

  void addDrawable(const gfx::MeshTreeDrawable::Ptr &drawable) {
    pmr::SharedTempAllocator tempAllocator;
    gfx::TransformUpdaterCollector collector(tempAllocator.getAllocator());
    collector.ignoreRootTransform = true;
    collector.collector = [&](const gfx::DrawablePtr &drawable, const float4x4 &worldTransform) {
      if (gfx::MeshDrawable::Ptr ptr = std::dynamic_pointer_cast<gfx::MeshDrawable>(drawable)) {
        this->addDrawable(ptr, worldTransform);
      }
    };
    collector.updateNoModify(*drawable);
  }

  void addDrawable(const gfx::MeshDrawable::Ptr &drawable, const float4x4 &worldTransform) {
    auto &mesh = drawable->mesh;
    if (!mesh) {
      SPDLOG_LOGGER_WARN(logger, "Ignoring drawable with no mesh");
      return;
    }

    auto &srcFormat = mesh->getFormat();

    auto &vertexData = mesh->getVertexData();

    std::optional<size_t> positionIndex;
    std::optional<size_t> positionOffset;

    size_t srcStride = srcFormat.getVertexSize();
    size_t offset{};
    for (size_t i = 0; i < srcFormat.vertexAttributes.size(); i++) {
      auto &attrib = srcFormat.vertexAttributes[i];
      if (attrib.name == "position") {
        positionIndex = i;
        positionOffset = offset;
        break;
      }
      offset += attrib.numComponents * getStorageTypeSize(attrib.type);
    }

    // Validate required attributes
    {
      if (!positionIndex.has_value()) {
        throw std::runtime_error("Mesh does not have a position attribute");
      }
      auto positionAttrib = srcFormat.vertexAttributes[positionIndex.value()];
      if (positionAttrib.type != gfx::StorageType::Float32 && positionAttrib.numComponents != 3) {
        throw std::runtime_error("position attribute must be float3");
      }
    }

    {
      const uint8_t *basePtr = vertexData.data() + *positionOffset;
      for (size_t i = 0; i < mesh->getNumVertices(); i++) {
        auto &pt = *(float3 *)(basePtr + srcStride * i);
        float3 transformedP = linalg::mul(worldTransform, float4(pt, 1.0)).xyz();
        points.push_back(JPH::Vec3(transformedP.x, transformedP.y, transformedP.z));
      }
    }
  }
};

struct MeshHullShapeShard {
  static SHTypesInfo inputTypes() { return gfx::ShardsTypes::Drawable; }
  static SHTypesInfo outputTypes() { return SHShape::Type; }
  static SHOptionalString help() { return SHCCSTR(""); }

  PARAM_PARAMVAR(_maxConvexRadius, "MaxConvexRadius", "AddDescriptionHere",
                 {shards::CoreInfo::FloatType, shards::CoreInfo::FloatVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_maxConvexRadius));

  OwnedVar _output;
  PointsBuilder _builder;

  MeshHullShapeShard() { _maxConvexRadius = Var(JPH::cDefaultConvexRadius); }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &drawable = varAsObjectChecked<gfx::SHDrawable>(input, gfx::ShardsTypes::Drawable);

    _builder.points.clear();
    _builder.addDrawable(drawable);

    auto [shape, var] = SHShape::ObjectVar.NewOwnedVar();

    JPH::Shape::ShapeResult result;
    JPH::ConvexHullShapeSettings settings(_builder.points, _maxConvexRadius.get().payload.floatValue);
    JPH::Ref<JPH::ConvexHullShape> hullShape = new JPH::ConvexHullShape(settings, result);

    if (result.HasError()) {
      throw SHException(fmt::format("Failed to create convex hull shape: {}", result.GetError().c_str()));
    }
    shape.shape = hullShape;

    return (_output = std::move(var));
  }
};

struct BoxShapeShard {
  static SHTypesInfo inputTypes() { return CoreInfo::Float3Type; }
  static SHTypesInfo outputTypes() { return SHShape::Type; }
  static SHOptionalString help() { return SHCCSTR("Create a box collision shape, given half it's extent"); }

  PARAM_IMPL();

  OwnedVar _output;

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto [shape, var] = SHShape::ObjectVar.NewOwnedVar();

    JPH::Shape::ShapeResult result;
    JPH::Vec3 v3 = toJPHVec3(input.payload.float3Value);
    JPH::BoxShapeSettings settings(v3);
    JPH::Ref<JPH::BoxShape> hullShape = new JPH::BoxShape(settings, result);

    if (result.HasError()) {
      throw SHException(fmt::format("Failed to create convex hull shape: {}", result.GetError().c_str()));
    }
    shape.shape = hullShape;

    return (_output = std::move(var));
  }
};

// Sphere shape
struct SphereShapeShard {
  static SHTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static SHTypesInfo outputTypes() { return SHShape::Type; }
  static SHOptionalString help() { return SHCCSTR("Create a sphere collision shape, given it's radius"); }

  PARAM_IMPL();

  OwnedVar _output;

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto [shape, var] = SHShape::ObjectVar.NewOwnedVar();

    JPH::Shape::ShapeResult result;
    JPH::SphereShapeSettings settings(input.payload.floatValue);
    JPH::Ref<JPH::SphereShape> hullShape = new JPH::SphereShape(settings, result);

    if (result.HasError()) {
      throw SHException(fmt::format("Failed to create convex hull shape: {}", result.GetError().c_str()));
    }
    shape.shape = hullShape;

    return (_output = std::move(var));
  }
};

} // namespace shards::Physics

SHARDS_REGISTER_FN(shapes) {
  using namespace shards::Physics;

  REGISTER_SHARD("Physics.HullShape", MeshHullShapeShard);
  REGISTER_SHARD("Physics.BoxShape", BoxShapeShard);
  REGISTER_SHARD("Physics.SphereShape", SphereShapeShard);
}
