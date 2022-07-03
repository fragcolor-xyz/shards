#include "helpers.hpp"
#include <gfx/helpers/shapes.hpp>
#include "linalg_shim.hpp"
#include <params.hpp>

namespace shards {
namespace Helpers {
using linalg::aliases::float2;
using linalg::aliases::float3;
using linalg::aliases::float4;
using linalg::aliases::float4x4;

float4 colorOrDefault(const SHVar &var) {
  static const float4 defaultColor(1, 1, 1, 1);
  return var.valueType == SHType::None ? defaultColor : toFloat4(var);
}

uint32_t thicknessOrDefault(const SHVar &var) {
  static const uint32_t defaultThickness = 2;
  return var.valueType == SHType::None ? defaultThickness : std::max<uint32_t>(1, var.payload.intValue);
}

struct LineShard : public BaseConsumer {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR("Draws a line in 3d space"); }

  PARAM_PARAMVAR(_a, "A", "Starting position of the line", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_b, "B", "Ending position of the line", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_color, "Color", "Linear color of the line", {CoreInfo::Float4Type, Type::VariableOf(CoreInfo::Float4Type)});
  PARAM_VAR(_thickness, "Thickness", "Width of the line in screen space", {CoreInfo::IntType});
  PARAM_IMPL(LineShard, PARAM_IMPL_FOR(_thickness), PARAM_IMPL_FOR(_a), PARAM_IMPL_FOR(_b), PARAM_IMPL_FOR(_color));

  SHTypeInfo compose(SHInstanceData &data) {
    composeCheckContext(data);
    composeCheckMainThread(data);

    if (_a->valueType == SHType::None)
      throw ComposeError("A is required");
    if (_b->valueType == SHType::None)
      throw ComposeError("B is required");

    return shards::CoreInfo::NoneType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &gizmoRenderer = getContext().gizmoContext.renderer;
    auto &shapeRenderer = gizmoRenderer.getShapeRenderer();

    shapeRenderer.addLine(toFloat3(_a.get()), toFloat3(_b.get()), colorOrDefault(_color.get()), thicknessOrDefault(_thickness));

    return SHVar{};
  }

  void warmup(SHContext *context) {
    baseConsumerWarmup(context);
    PARAM_WARMUP(context);
  }
  void cleanup() {
    baseConsumerCleanup();
    PARAM_CLEANUP();
  }
};

struct CircleShard : public BaseConsumer {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR("Draws a line in 3d space"); }

  PARAM_PARAMVAR(_center, "Center", "Center of the circle", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_xBase, "XBase", "X direction of the plane the circle is on", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_yBase, "YBase", "Y direction of the plane the circle is on", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_radius, "Radius", "Radius", {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_color, "Color", "Linear color of the circle", {CoreInfo::Float4Type, CoreInfo::Float4VarType});
  PARAM_VAR(_thickness, "Thickness", "Width of the circle in screen space", {CoreInfo::IntType});
  PARAM_IMPL(CircleShard, PARAM_IMPL_FOR(_center), PARAM_IMPL_FOR(_xBase), PARAM_IMPL_FOR(_yBase), PARAM_IMPL_FOR(_radius),
             PARAM_IMPL_FOR(_color), PARAM_IMPL_FOR(_thickness), );

  SHTypeInfo compose(SHInstanceData &data) {
    composeCheckContext(data);
    composeCheckMainThread(data);

    if (_center->valueType == SHType::None)
      throw ComposeError("Center is required");
    if (_xBase->valueType == SHType::None)
      throw ComposeError("XBase is required");
    if (_yBase->valueType == SHType::None)
      throw ComposeError("YBase is required");

    return shards::CoreInfo::NoneType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &gizmoRenderer = getContext().gizmoContext.renderer;
    auto &shapeRenderer = gizmoRenderer.getShapeRenderer();

    Var radiusVar(_radius.get());
    float radius = radiusVar.isSet() ? float(radiusVar) : 1.0f;

    shapeRenderer.addCircle(toFloat3(_center.get()), toFloat3(_xBase.get()), toFloat3(_yBase.get()), radius,
                            colorOrDefault(_color.get()), thicknessOrDefault(_thickness), 64);

    return SHVar{};
  }

  void warmup(SHContext *context) {
    baseConsumerWarmup(context);
    PARAM_WARMUP(context);
  }
  void cleanup() {
    baseConsumerCleanup();
    PARAM_CLEANUP();
  }
};

struct RectShard : public BaseConsumer {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR("Draws a rectangle in 3d space"); }

  PARAM_PARAMVAR(_center, "Center", "Starting position of the rectangle", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_xBase, "XBase", "X direction of the plane the rectangle is on",
                 {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_yBase, "YBase", "Y direction of the plane the rectangle is on",
                 {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_size, "Size", "Size of the rectange", {CoreInfo::Float2Type, CoreInfo::Float2VarType});
  PARAM_PARAMVAR(_color, "Color", "Rectanglear color of the rectangle", {CoreInfo::Float4Type, CoreInfo::Float4VarType});
  PARAM_VAR(_thickness, "Thickness", "Width of the rectangle in screen space", {CoreInfo::IntType});
  PARAM_IMPL(RectShard, PARAM_IMPL_FOR(_center), PARAM_IMPL_FOR(_xBase), PARAM_IMPL_FOR(_yBase), PARAM_IMPL_FOR(_size),
             PARAM_IMPL_FOR(_color), PARAM_IMPL_FOR(_thickness), );

  SHTypeInfo compose(SHInstanceData &data) {
    composeCheckContext(data);
    composeCheckMainThread(data);

    if (_center->valueType == SHType::None)
      throw ComposeError("Center is required");
    if (_xBase->valueType == SHType::None)
      throw ComposeError("XBase is required");
    if (_yBase->valueType == SHType::None)
      throw ComposeError("YBase is required");

    return shards::CoreInfo::NoneType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &gizmoRenderer = getContext().gizmoContext.renderer;
    auto &shapeRenderer = gizmoRenderer.getShapeRenderer();

    Var sizeVar(_size.get());
    float2 size = sizeVar.isSet() ? toFloat2(sizeVar) : float2(1.0f, 1.0f);

    shapeRenderer.addRect(toFloat3(_center.get()), toFloat3(_xBase.get()), toFloat3(_yBase.get()), size,
                          colorOrDefault(_color.get()), thicknessOrDefault(_thickness));

    return SHVar{};
  }

  void warmup(SHContext *context) {
    baseConsumerWarmup(context);
    PARAM_WARMUP(context);
  }
  void cleanup() {
    baseConsumerCleanup();
    PARAM_CLEANUP();
  }
};

struct BoxShard : public BaseConsumer {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR("Draws a box in 3d space"); }

  PARAM_PARAMVAR(_center, "Center", "Center of the box", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_size, "Size", "Size of the box", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_transform, "Transform", "Transform applied to the box",
                 {CoreInfo::Float4x4Type, Type::VariableOf(CoreInfo::Float4x4Type)});
  PARAM_PARAMVAR(_color, "Color", "Boxar color of the box", {CoreInfo::Float4Type, CoreInfo::Float4VarType});
  PARAM_VAR(_thickness, "Thickness", "Width of the box in screen space", {CoreInfo::IntType});
  PARAM_IMPL(BoxShard, PARAM_IMPL_FOR(_center), PARAM_IMPL_FOR(_size), PARAM_IMPL_FOR(_transform), PARAM_IMPL_FOR(_color),
             PARAM_IMPL_FOR(_thickness));

  SHTypeInfo compose(SHInstanceData &data) {
    composeCheckContext(data);
    composeCheckMainThread(data);

    if (_center->valueType == SHType::None)
      throw ComposeError("A is required");

    return shards::CoreInfo::NoneType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &gizmoRenderer = getContext().gizmoContext.renderer;
    auto &shapeRenderer = gizmoRenderer.getShapeRenderer();

    Var sizeVar(_size.get());
    float3 size = sizeVar.isSet() ? toFloat3(sizeVar) : float3(1.0f, 1.0f, 1.0f);

    Var transformVar(_transform.get());
    float4x4 transform = transformVar.isSet() ? toFloat4x4(transformVar) : linalg::identity;

    shapeRenderer.addBox(transform, toFloat3(_center.get()), size, colorOrDefault(_color.get()), thicknessOrDefault(_thickness));

    return SHVar{};
  }

  void warmup(SHContext *context) {
    baseConsumerWarmup(context);
    PARAM_WARMUP(context);
  }
  void cleanup() {
    baseConsumerCleanup();
    PARAM_CLEANUP();
  }
};

struct PointShard : public BaseConsumer {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR("Draws a point in 3d space"); }

  PARAM_PARAMVAR(_center, "Center", "Center of the point", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_color, "Color", "Pointar color of the point", {CoreInfo::Float4Type, CoreInfo::Float4VarType});
  PARAM_VAR(_thickness, "Thickness", "Size of the point in screen space", {CoreInfo::IntType});
  PARAM_IMPL(PointShard, PARAM_IMPL_FOR(_center), PARAM_IMPL_FOR(_color), PARAM_IMPL_FOR(_thickness));

  SHTypeInfo compose(SHInstanceData &data) {
    composeCheckContext(data);
    composeCheckMainThread(data);

    if (_center->valueType == SHType::None)
      throw ComposeError("Center is required");

    return shards::CoreInfo::NoneType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &gizmoRenderer = getContext().gizmoContext.renderer;
    auto &shapeRenderer = gizmoRenderer.getShapeRenderer();

    shapeRenderer.addPoint(toFloat3(_center.get()), colorOrDefault(_color.get()), thicknessOrDefault(_thickness));

    return SHVar{};
  }

  void warmup(SHContext *context) {
    baseConsumerWarmup(context);
    PARAM_WARMUP(context);
  }
  void cleanup() {
    baseConsumerCleanup();
    PARAM_CLEANUP();
  }
};

void registerShapeShards() {
  REGISTER_SHARD("Helpers.Line", LineShard);
  REGISTER_SHARD("Helpers.Circle", CircleShard);
  REGISTER_SHARD("Helpers.Rect", RectShard);
  REGISTER_SHARD("Helpers.Box", BoxShard);
  REGISTER_SHARD("Helpers.Point", PointShard);
}
} // namespace Helpers
} // namespace shards