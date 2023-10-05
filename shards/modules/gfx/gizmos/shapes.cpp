#include "context.hpp"
#include <shards/linalg_shim.hpp>
#include <shards/core/params.hpp>
#include <gfx/gizmos/shapes.hpp>
#include <cmath>

namespace shards {
namespace Gizmos {
using linalg::aliases::float2;
using linalg::aliases::float3;
using linalg::aliases::float4;
using linalg::aliases::float4x4;

float4 colorOrDefault(const SHVar &var) {
  static const float4 defaultColor(1, 1, 1, 1);
  return var.valueType == SHType::None ? defaultColor : toFloat4(var);
}

float thicknessOrDefault(const SHVar &var) {
  static const float defaultThickness = 2.0;
  if (var.valueType == SHType::Float) {
    return float(var.payload.floatValue);
  } else {
    return var.valueType == SHType::None ? defaultThickness : std::max<uint32_t>(1, var.payload.intValue);
  }
}

struct LineShard : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR("Draws a line in 3d space"); }

  PARAM_PARAMVAR(_a, "A", "Starting position of the line", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_b, "B", "Ending position of the line", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_color, "Color", "Linear color of the line", {CoreInfo::Float4Type, Type::VariableOf(CoreInfo::Float4Type)});
  PARAM_VAR(_thickness, "Thickness", "Width of the line in screen space", {CoreInfo::IntType, CoreInfo::FloatType});
  PARAM_IMPL(PARAM_IMPL_FOR(_a), PARAM_IMPL_FOR(_b), PARAM_IMPL_FOR(_color), PARAM_IMPL_FOR(_thickness));

  SHTypeInfo compose(SHInstanceData &data) {
    gfx::composeCheckGfxThread(data);

    if (_a->valueType == SHType::None)
      throw ComposeError("A is required");
    if (_b->valueType == SHType::None)
      throw ComposeError("B is required");

    return shards::CoreInfo::NoneType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &gizmoRenderer = _gizmoContext->gfxGizmoContext.renderer;
    auto &shapeRenderer = gizmoRenderer.getShapeRenderer();

    shapeRenderer.addLine(toFloat3(_a.get()), toFloat3(_b.get()), colorOrDefault(_color.get()), thicknessOrDefault(_thickness));

    return SHVar{};
  }

  void warmup(SHContext *context) {
    baseWarmup(context);
    PARAM_WARMUP(context);
  }
  void cleanup() {
    baseCleanup();
    PARAM_CLEANUP();
  }
};

struct CircleShard : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR("Draws a line in 3d space"); }

  PARAM_PARAMVAR(_center, "Center", "Center of the circle", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_xBase, "XBase", "X direction of the plane the circle is on", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_yBase, "YBase", "Y direction of the plane the circle is on", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_radius, "Radius", "Radius", {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_color, "Color", "Linear color of the circle", {CoreInfo::Float4Type, CoreInfo::Float4VarType});
  PARAM_VAR(_thickness, "Thickness", "Width of the circle in screen space", {CoreInfo::IntType, CoreInfo::FloatType});
  PARAM_IMPL(PARAM_IMPL_FOR(_center), PARAM_IMPL_FOR(_xBase), PARAM_IMPL_FOR(_yBase), PARAM_IMPL_FOR(_radius),
             PARAM_IMPL_FOR(_color), PARAM_IMPL_FOR(_thickness));

  SHTypeInfo compose(SHInstanceData &data) {
    gfx::composeCheckGfxThread(data);

    if (_center->valueType == SHType::None)
      throw ComposeError("Center is required");
    if (_xBase->valueType == SHType::None)
      throw ComposeError("XBase is required");
    if (_yBase->valueType == SHType::None)
      throw ComposeError("YBase is required");

    return shards::CoreInfo::NoneType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &gizmoRenderer = _gizmoContext->gfxGizmoContext.renderer;
    auto &shapeRenderer = gizmoRenderer.getShapeRenderer();

    Var radiusVar(_radius.get());
    float radius = radiusVar.isNone() ? 1.0f : float(radiusVar);

    shapeRenderer.addCircle(toFloat3(_center.get()), toFloat3(_xBase.get()), toFloat3(_yBase.get()), radius,
                            colorOrDefault(_color.get()), thicknessOrDefault(_thickness), 64);

    return SHVar{};
  }

  void warmup(SHContext *context) {
    baseWarmup(context);
    PARAM_WARMUP(context);
  }
  void cleanup() {
    baseCleanup();
    PARAM_CLEANUP();
  }
};

struct RectShard : public Base {
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
  PARAM_VAR(_thickness, "Thickness", "Width of the rectangle in screen space", {CoreInfo::IntType, CoreInfo::FloatType});
  PARAM_IMPL(PARAM_IMPL_FOR(_center), PARAM_IMPL_FOR(_xBase), PARAM_IMPL_FOR(_yBase), PARAM_IMPL_FOR(_size),
             PARAM_IMPL_FOR(_color), PARAM_IMPL_FOR(_thickness));

  SHTypeInfo compose(SHInstanceData &data) {
    gfx::composeCheckGfxThread(data);

    if (_center->valueType == SHType::None)
      throw ComposeError("Center is required");
    if (_xBase->valueType == SHType::None)
      throw ComposeError("XBase is required");
    if (_yBase->valueType == SHType::None)
      throw ComposeError("YBase is required");

    return shards::CoreInfo::NoneType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &gizmoRenderer = _gizmoContext->gfxGizmoContext.renderer;
    auto &shapeRenderer = gizmoRenderer.getShapeRenderer();

    Var sizeVar(_size.get());
    float2 size = sizeVar.isNone() ? float2(1.0f, 1.0f) : toFloat2(sizeVar);

    shapeRenderer.addRect(toFloat3(_center.get()), toFloat3(_xBase.get()), toFloat3(_yBase.get()), size,
                          colorOrDefault(_color.get()), thicknessOrDefault(_thickness));

    return SHVar{};
  }

  void warmup(SHContext *context) {
    baseWarmup(context);
    PARAM_WARMUP(context);
  }
  void cleanup() {
    baseCleanup();
    PARAM_CLEANUP();
  }
};

struct BoxShard : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR("Draws a box in 3d space"); }

  PARAM_PARAMVAR(_center, "Center", "Center of the box", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_size, "Size", "Size of the box", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_transform, "Transform", "Transform applied to the box",
                 {CoreInfo::Float4x4Type, Type::VariableOf(CoreInfo::Float4x4Type)});
  PARAM_PARAMVAR(_color, "Color", "Boxar color of the box", {CoreInfo::Float4Type, CoreInfo::Float4VarType});
  PARAM_VAR(_thickness, "Thickness", "Width of the box in screen space", {CoreInfo::IntType, CoreInfo::FloatType});
  PARAM_IMPL(PARAM_IMPL_FOR(_center), PARAM_IMPL_FOR(_size), PARAM_IMPL_FOR(_transform), PARAM_IMPL_FOR(_color),
             PARAM_IMPL_FOR(_thickness));

  SHTypeInfo compose(SHInstanceData &data) {
    gfx::composeCheckGfxThread(data);

    if (_center->valueType == SHType::None)
      throw ComposeError("A is required");

    return shards::CoreInfo::NoneType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &gizmoRenderer = _gizmoContext->gfxGizmoContext.renderer;
    auto &shapeRenderer = gizmoRenderer.getShapeRenderer();

    Var sizeVar(_size.get());
    float3 size = sizeVar.isNone() ? float3(1.0f, 1.0f, 1.0f) : toFloat3(sizeVar);

    Var transformVar(_transform.get());
    float4x4 transform = transformVar.isNone() ? linalg::identity : toFloat4x4(transformVar);

    shapeRenderer.addBox(transform, toFloat3(_center.get()), size, colorOrDefault(_color.get()), thicknessOrDefault(_thickness));

    return SHVar{};
  }

  void warmup(SHContext *context) {
    baseWarmup(context);
    PARAM_WARMUP(context);
  }
  void cleanup() {
    baseCleanup();
    PARAM_CLEANUP();
  }
};

struct PointShard : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR("Draws a point in 3d space"); }

  PARAM_PARAMVAR(_center, "Center", "Center of the point", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_color, "Color", "Pointar color of the point", {CoreInfo::Float4Type, CoreInfo::Float4VarType});
  PARAM_VAR(_thickness, "Thickness", "Size of the point in screen space", {CoreInfo::IntType, CoreInfo::FloatType});
  PARAM_IMPL(PARAM_IMPL_FOR(_center), PARAM_IMPL_FOR(_color), PARAM_IMPL_FOR(_thickness));

  SHTypeInfo compose(SHInstanceData &data) {
    gfx::composeCheckGfxThread(data);

    if (_center->valueType == SHType::None)
      throw ComposeError("Center is required");

    return shards::CoreInfo::NoneType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &gizmoRenderer = _gizmoContext->gfxGizmoContext.renderer;
    auto &shapeRenderer = gizmoRenderer.getShapeRenderer();

    shapeRenderer.addPoint(toFloat3(_center.get()), colorOrDefault(_color.get()), thicknessOrDefault(_thickness));

    return SHVar{};
  }

  void warmup(SHContext *context) {
    baseWarmup(context);
    PARAM_WARMUP(context);
  }
  void cleanup() {
    baseCleanup();
    PARAM_CLEANUP();
  }
};

struct SolidRectShard : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR("Draws a filled rectangle in 3d space"); }

  PARAM_PARAMVAR(_center, "Center", "Starting position of the rectangle", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_xBase, "XBase", "X direction of the plane the rectangle is on",
                 {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_yBase, "YBase", "Y direction of the plane the rectangle is on",
                 {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_size, "Size", "Size of the rectange", {CoreInfo::Float2Type, CoreInfo::Float2VarType});
  PARAM_PARAMVAR(_color, "Color", "Rectanglear color of the rectangle", {CoreInfo::Float4Type, CoreInfo::Float4VarType});
  PARAM_PARAMVAR(_culling, "Culling", "Back-face culling of the rectangle", {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_center), PARAM_IMPL_FOR(_xBase), PARAM_IMPL_FOR(_yBase), PARAM_IMPL_FOR(_size),
             PARAM_IMPL_FOR(_color), PARAM_IMPL_FOR(_culling));

  SHTypeInfo compose(SHInstanceData &data) {
    gfx::composeCheckGfxThread(data);

    if (_center->valueType == SHType::None)
      throw ComposeError("Center is required");
    if (_xBase->valueType == SHType::None)
      throw ComposeError("XBase is required");
    if (_yBase->valueType == SHType::None)
      throw ComposeError("YBase is required");

    return shards::CoreInfo::NoneType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &gizmoRenderer = _gizmoContext->gfxGizmoContext.renderer;
    auto &shapeRenderer = gizmoRenderer.getShapeRenderer();

    Var sizeVar(_size.get());
    float2 size = sizeVar.isNone() ? float2(1.0f, 1.0f) : toFloat2(sizeVar);
    Var cullingVar(_culling.get());
    bool culling = cullingVar.isNone() ? true : bool(cullingVar);

    shapeRenderer.addSolidRect(toFloat3(_center.get()), toFloat3(_xBase.get()), toFloat3(_yBase.get()), size,
                               colorOrDefault(_color.get()), culling);

    return SHVar{};
  }

  void warmup(SHContext *context) {
    baseWarmup(context);
    PARAM_WARMUP(context);
  }
  void cleanup() {
    baseCleanup();
    PARAM_CLEANUP();
  }
};

struct DiscShard : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR("Draws a filled disc in 3d space"); }

  PARAM_PARAMVAR(_center, "Center", "Center of the disc", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_xBase, "XBase", "X direction of the plane the disc is on", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_yBase, "YBase", "Y direction of the plane the disc is on", {CoreInfo::Float3Type, CoreInfo::Float3VarType})
  PARAM_PARAMVAR(_outerRadius, "OuterRadius", "Radius of the outer circle of the disc",
                 {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_innerRadius, "InnerRadius", "Radius of the inner circle of the disc",
                 {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_color, "Color", "Linear color of the disc", {CoreInfo::Float4Type, CoreInfo::Float4VarType});
  PARAM_PARAMVAR(_culling, "Culling", "Back-face culling of the disc", {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_center), PARAM_IMPL_FOR(_xBase), PARAM_IMPL_FOR(_yBase), PARAM_IMPL_FOR(_outerRadius),
             PARAM_IMPL_FOR(_innerRadius), PARAM_IMPL_FOR(_color), PARAM_IMPL_FOR(_culling));

  SHTypeInfo compose(SHInstanceData &data) {
    gfx::composeCheckGfxThread(data);

    if (_center->valueType == SHType::None)
      throw ComposeError("Center is required");
    if (_xBase->valueType == SHType::None)
      throw ComposeError("XBase is required");
    if (_yBase->valueType == SHType::None)
      throw ComposeError("YBase is required");

    return shards::CoreInfo::NoneType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &gizmoRenderer = _gizmoContext->gfxGizmoContext.renderer;
    auto &shapeRenderer = gizmoRenderer.getShapeRenderer();

    Var outerRadiusVar(_outerRadius.get());
    Var innerRadiusVar(_innerRadius.get());
    float outerRadius = outerRadiusVar.isNone() ? 1.0f : float(outerRadiusVar);
    float innerRadius = innerRadiusVar.isNone() ? outerRadius / 2 : float(innerRadiusVar);
    Var cullingVar(_culling.get());
    bool culling = cullingVar.isNone() ? true : bool(cullingVar);

    // currently ensures there will never be an issue where inneradius is larger than outer radius
    // may want to handle differently in the future
    if (innerRadius > outerRadius) {
      std::swap(innerRadius, outerRadius);
    }

    shapeRenderer.addDisc(toFloat3(_center.get()), toFloat3(_xBase.get()), toFloat3(_yBase.get()), outerRadius, innerRadius,
                          colorOrDefault(_color.get()), culling, 64);

    return SHVar{};
  }

  void warmup(SHContext *context) {
    baseWarmup(context);
    PARAM_WARMUP(context);
  }
  void cleanup() {
    baseCleanup();
    PARAM_CLEANUP();
  }
};

struct GridShard : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR("Draws a grid"); }

  PARAM_PARAMVAR(_center, "Center", "Center of the disc", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_xBase, "XBase", "X direction of the grid", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_yBase, "YBase", "Y direction of the grid", {CoreInfo::Float3Type, CoreInfo::Float3VarType})
  PARAM_VAR(_thickness, "Thickness", "Width of the line in screen space", {CoreInfo::NoneType, CoreInfo::IntType});
  PARAM_PARAMVAR(_stepSize, "StepSize", "Step size of the grid lines", {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_size, "Size", "Number of grid lines", {CoreInfo::IntType, CoreInfo::IntVarType});
  PARAM_PARAMVAR(_color, "Color", "Linear color of the grid lines",
                 {CoreInfo::NoneType, CoreInfo::Float4Type, CoreInfo::Float4VarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_center), PARAM_IMPL_FOR(_xBase), PARAM_IMPL_FOR(_yBase), PARAM_IMPL_FOR(_thickness),
             PARAM_IMPL_FOR(_stepSize), PARAM_IMPL_FOR(_size), PARAM_IMPL_FOR(_color));

  SHTypeInfo compose(SHInstanceData &data) {
    gfx::composeCheckGfxThread(data);

    if (_center->valueType == SHType::None)
      throw ComposeError("Center is required");
    if (_xBase->valueType == SHType::None)
      throw ComposeError("XBase is required");
    if (_yBase->valueType == SHType::None)
      throw ComposeError("YBase is required");

    return shards::CoreInfo::NoneType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &gizmoRenderer = _gizmoContext->gfxGizmoContext.renderer;
    auto &shapeRenderer = gizmoRenderer.getShapeRenderer();

    int thickness = _thickness->isNone() ? 1 : int32_t(*_thickness);

    Var &stepSizeVar = (Var &)_stepSize.get();
    float stepSize = stepSizeVar.isNone() ? 1.0f : float(stepSizeVar);

    float4 color = colorOrDefault(_color.get());

    Var &sizeVar = (Var &)_size.get();
    gfx::int2 gridSize{sizeVar.isNone() ? 8 : int(sizeVar)};

    if (gridSize.x < 0 || gridSize.y < 0) {
      throw ActivationError("Grid size must be positive");
    }

    for (int y = -gridSize.y; y <= gridSize.y; y++) {
      float3 yOffs = toFloat3(_yBase.get()) * float(y) * stepSize;
      shapeRenderer.addLine(toFloat3(_center.get()) + toFloat3(_xBase.get()) * float(gridSize.x) * stepSize + yOffs,
                            toFloat3(_center.get()) - toFloat3(_xBase.get()) * float(gridSize.x) * stepSize + yOffs, color,
                            thickness);
    }

    for (int x = -gridSize.x; x <= gridSize.x; x++) {
      float3 xOffs = toFloat3(_xBase.get()) * float(x) * stepSize;
      shapeRenderer.addLine(toFloat3(_center.get()) + toFloat3(_yBase.get()) * float(gridSize.y) * stepSize + xOffs,
                            toFloat3(_center.get()) - toFloat3(_yBase.get()) * float(gridSize.y) * stepSize + xOffs, color,
                            thickness);
    }

    return SHVar{};
  }

  void warmup(SHContext *context) {
    baseWarmup(context);
    PARAM_WARMUP(context);
  }
  void cleanup() {
    baseCleanup();
    PARAM_CLEANUP();
  }
};

struct RefSpaceGridOverlayShard : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR("Draws a grid"); }

  PARAM_PARAMVAR(_center, "Center", "Center of the disc", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_xBase, "XBase", "X direction of the plane the disc is on", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_yBase, "YBase", "Y direction of the plane the disc is on", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_VAR(_thickness, "Thickness", "Width of the line in screen space", {CoreInfo::NoneType, CoreInfo::IntType});
  PARAM_PARAMVAR(_stepSize, "StepSize", "Step size of the grid lines", {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_color, "Color", "Linear color of the grid lines",
                 {CoreInfo::NoneType, CoreInfo::Float4Type, CoreInfo::Float4VarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_center), PARAM_IMPL_FOR(_xBase), PARAM_IMPL_FOR(_yBase), PARAM_IMPL_FOR(_thickness),
             PARAM_IMPL_FOR(_stepSize), PARAM_IMPL_FOR(_color));

  SHTypeInfo compose(SHInstanceData &data) {
    gfx::composeCheckGfxThread(data);

    if (_center->valueType == SHType::None)
      throw ComposeError("Center is required");
    if (_xBase->valueType == SHType::None)
      throw ComposeError("XBase is required");
    if (_yBase->valueType == SHType::None)
      throw ComposeError("YBase is required");

    return shards::CoreInfo::NoneType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &gizmoRenderer = _gizmoContext->gfxGizmoContext.renderer;
    auto &shapeRenderer = gizmoRenderer.getShapeRenderer();

    int thickness = _thickness->isNone() ? 1 : int32_t(*_thickness);

    Var &stepSizeVar = (Var &)_stepSize.get();
    float stepSize = stepSizeVar.isNone() ? 1.0f : float(stepSizeVar);

    float4 baseColor = colorOrDefault(_color.get());

    const int xl = 3;
    const int yl = 3;

    float3 axes[] = {
        float3(1, 0, 0),
        float3(0, 1, 0),
        float3(0, 0, 1),
    };

    float3 gridX = toFloat3(_xBase.get());
    float3 gridY = toFloat3(_yBase.get());
    int axisMap[2];
    float signMap[2];

    auto mapAxis = [](float3 v, float &outSign) {
      int r = 0;
      float m = std::abs(v.x);
      if (std::abs(v.y) > m) {
        r = 1;
        m = std::abs(v.y);
      }
      if (std::abs(v.z) > m) {
        r = 2;
      }

      outSign = std::signbit(v[r]) ? -1.0f : 1.0f;
      return r;
    };

    axisMap[0] = mapAxis(gridX, signMap[0]);
    axisMap[1] = mapAxis(gridY, signMap[1]);

    for (int a = 0; a < 2; a++) {
      float3 axis = axes[axisMap[a]];
      float3 otherAxis = axes[axisMap[(a + 1) % 2]];
      float3 oaOffset = otherAxis * 0.01f;
      shapeRenderer.addLine(toFloat3(_center.get()) + axis * float(xl) * stepSize + oaOffset, toFloat3(_center.get()) + oaOffset,
                            float4(axis, 1.f) * baseColor, thickness);
    }

    return SHVar{};
  }

  void warmup(SHContext *context) {
    baseWarmup(context);
    PARAM_WARMUP(context);
  }
  void cleanup() {
    baseCleanup();
    PARAM_CLEANUP();
  }
};

void registerShapeShards() {
  REGISTER_SHARD("Gizmos.Line", LineShard);
  REGISTER_SHARD("Gizmos.Circle", CircleShard);
  REGISTER_SHARD("Gizmos.Rect", RectShard);
  REGISTER_SHARD("Gizmos.Box", BoxShard);
  REGISTER_SHARD("Gizmos.Point", PointShard);
  REGISTER_SHARD("Gizmos.SolidRect", SolidRectShard);
  REGISTER_SHARD("Gizmos.Disc", DiscShard);
  REGISTER_SHARD("Gizmos.Grid", GridShard);
  REGISTER_SHARD("Gizmos.RefspaceGridOverlay", RefSpaceGridOverlayShard);
}
} // namespace Gizmos
} // namespace shards
