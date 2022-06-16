#ifndef SH_EXTRA_GFX_SHARDS_TYPES
#define SH_EXTRA_GFX_SHARDS_TYPES

#include <shards.hpp>
#include <common_types.hpp>
#include <foundation.hpp>
#include <gfx/enums.hpp>
#include <gfx/fwd.hpp>
#include <gfx/pipeline_step.hpp>
#include <gfx/shader/types.hpp>
#include <gfx/shader/entry_point.hpp>
#include <memory>
#include <vector>

namespace gfx {
struct SHBasicShaderParameter {
  std::string key;
  shards::ParamVar var;
  SHBasicShaderParameter(std::string key, shards::ParamVar &&var) : key(key), var(std::move(var)) {}
};

struct MaterialParameters;
struct SHShaderParameters {
  std::vector<SHBasicShaderParameter> basic;

  void updateVariables(MaterialParameters &output);
};

struct SHDrawable {
  DrawablePtr drawable;
  shards::ParamVar transformVar;
  shards::ParamVar materialVar;
  SHShaderParameters shaderParameters;

  void updateVariables();
};

struct SHDrawableHierarchy {
  DrawableHierarchyPtr drawableHierarchy;
  shards::ParamVar transformVar;

  void updateVariables();
};

struct SHView {
  ViewPtr view;
  shards::ParamVar viewTransformVar;

  void updateVariables();
};

struct SHMaterial {
  MaterialPtr material;
  SHShaderParameters shaderParameters;

  void updateVariables();
};

constexpr uint32_t VendorId = 'cgfx';

namespace detail {
using namespace shards;
// NOTE: This needs to be a struct ensure correct initialization order under clang
struct Container {
  static inline Types ShaderParamTypes{{
      CoreInfo::Float4x4Type,
      CoreInfo::Float4Type,
      CoreInfo::Float3Type,
      CoreInfo::Float2Type,
      CoreInfo::FloatType,
  }};
  static inline Types ShaderParamVarTypes{{
      Type::VariableOf(CoreInfo::Float4x4Type),
      Type::VariableOf(CoreInfo::Float4Type),
      Type::VariableOf(CoreInfo::Float3Type),
      Type::VariableOf(CoreInfo::Float2Type),
      Type::VariableOf(CoreInfo::FloatType),
  }};
  static inline Type ShaderParamTable = Type::TableOf(ShaderParamTypes);

#define SH_CONCAT(_a, _b) _a##_b
#define ENUM(_id, _displayName, _definedAs, _type)                                     \
  static constexpr uint32_t SH_CONCAT(_definedAs, TypeId) = uint32_t(_id);             \
  static inline Type _definedAs = Type::Enum(VendorId, SH_CONCAT(_definedAs, TypeId)); \
  static inline EnumInfo<_type> SH_CONCAT(_definedAs, EnumInfo){_displayName, VendorId, SH_CONCAT(_definedAs, TypeId)};

#define OBJECT(_id, _displayName, _definedAs, _type)                                                                            \
  static constexpr uint32_t SH_CONCAT(_definedAs, TypeId) = uint32_t(_id);                                                      \
  static inline Type _definedAs{{SHType::Object, {.object = {.vendorId = VendorId, .typeId = SH_CONCAT(_definedAs, TypeId)}}}}; \
  static inline ObjectVar<_type> SH_CONCAT(_definedAs, ObjectVar){_displayName, VendorId, SH_CONCAT(_definedAs, TypeId)};

  OBJECT('drah', "GFX.DrawableHierarchy", DrawableHierarchy, SHDrawableHierarchy)
  OBJECT('draw', "GFX.Drawable", Drawable, SHDrawable)
  OBJECT('mesh', "GFX.Mesh", Mesh, MeshPtr)

  ENUM('_e0', "WindingOrder", WindingOrder, gfx::WindingOrder)
  ENUM('_e1', "ShaderFieldBaseType", ShaderFieldBaseType, gfx::ShaderFieldBaseType)
  ENUM('_e2', "ProgrammableGraphicsStage", ProgrammableGraphicsStage, gfx::ProgrammableGraphicsStage)
  ENUM('_e3', "DependencyType", DependencyType, shader::DependencyType)

  enum class BlendFactor_ {
    Zero = WGPUBlendFactor_Zero,
    One = WGPUBlendFactor_One,
    Src = WGPUBlendFactor_Src,
    OneMinusSrc = WGPUBlendFactor_OneMinusSrc,
    SrcAlpha = WGPUBlendFactor_SrcAlpha,
    OneMinusSrcAlpha = WGPUBlendFactor_OneMinusSrcAlpha,
    Dst = WGPUBlendFactor_Dst,
    OneMinusDst = WGPUBlendFactor_OneMinusDst,
    DstAlpha = WGPUBlendFactor_DstAlpha,
    OneMinusDstAlpha = WGPUBlendFactor_OneMinusDstAlpha,
    SrcAlphaSaturated = WGPUBlendFactor_SrcAlphaSaturated,
    Constant = WGPUBlendFactor_Constant,
    OneMinusConstant = WGPUBlendFactor_OneMinusConstant,
  };
  ENUM('_e4', "BlendFactor", BlendFactor, BlendFactor_)

  enum class BlendOperation_ {
    Add = WGPUBlendOperation_Add,
    Subtract = WGPUBlendOperation_Subtract,
    ReverseSubtract = WGPUBlendOperation_ReverseSubtract,
    Min = WGPUBlendOperation_Min,
    Max = WGPUBlendOperation_Max,
  };
  ENUM('_e5', "BlendOperation", BlendOperation, BlendOperation_)

  enum class FilterMode_ {
    Nearest = WGPUFilterMode_Nearest,
    Linear = WGPUFilterMode_Linear,
  };
  ENUM('_e6', "FilterMode", FilterModeEnum, FilterMode_)

  enum class CompareFunction_ {
    Undefined = WGPUCompareFunction_Undefined,
    Never = WGPUCompareFunction_Never,
    Less = WGPUCompareFunction_Less,
    LessEqual = WGPUCompareFunction_LessEqual,
    Greater = WGPUCompareFunction_Greater,
    GreaterEqual = WGPUCompareFunction_GreaterEqual,
    Equal = WGPUCompareFunction_Equal,
    NotEqual = WGPUCompareFunction_NotEqual,
    Always = WGPUCompareFunction_Always,
  };
  ENUM('_e7', "CompareFunction", CompareFunction, CompareFunction_)

  enum class ColorMask_ {
    None = WGPUColorWriteMask_None,
    Red = WGPUColorWriteMask_Red,
    Green = WGPUColorWriteMask_Green,
    Blue = WGPUColorWriteMask_Blue,
    Alpha = WGPUColorWriteMask_Alpha,
    All = WGPUColorWriteMask_All,
  };
  ENUM('_e8', "ColorMask", ColorMask, ColorMask_)

  OBJECT('feat', "GFX.Feature", Feature, FeaturePtr)

  OBJECT('pips', "GFX.PipelineStep", PipelineStep, PipelineStepPtr)
  static inline Type PipelineStepSeq = Type::SeqOf(PipelineStep);

  OBJECT('view', "GFX.View", View, SHView)
  static inline Type ViewSeq = Type::SeqOf(View);

  OBJECT('mat_', "GFX.Material", Material, SHMaterial)
#undef ENUM
#undef OBJECT
#undef SH_CONCAT
};
} // namespace detail
using Types = detail::Container;

} // namespace gfx

#endif // SH_EXTRA_GFX_SHARDS_TYPES
