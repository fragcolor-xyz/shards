#ifndef SH_EXTRA_GFX_SHARDS_TYPES
#define SH_EXTRA_GFX_SHARDS_TYPES

#include <common_types.hpp>
#include <foundation.hpp>
#include <gfx/enums.hpp>
#include <gfx/fwd.hpp>
#include <gfx/pipeline_step.hpp>
#include <gfx/shader/entry_point.hpp>
#include <gfx/shader/types.hpp>
#include <gfx/drawables/mesh_drawable.hpp>
#include <gfx/drawables/mesh_tree_drawable.hpp>
#include <memory>
#include <shards.hpp>
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
  std::vector<SHBasicShaderParameter> textures;

  void updateVariables(MaterialParameters &output);
};

struct SHDrawable {
  MeshDrawable drawable;
  shards::ParamVar transformVar;
  shards::ParamVar materialVar;
  SHShaderParameters shaderParameters;

  void updateVariables();
};

struct SHTreeDrawable {
  MeshTreeDrawable::Ptr drawable;
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

struct SHRenderTarget {
  RenderTargetPtr renderTarget;
};

struct SHDrawQueue {
  DrawQueuePtr queue;
};

constexpr uint32_t VendorId = shards::CoreCC;

namespace detail {
using namespace shards;
// NOTE: This needs to be a struct ensure correct initialization order under clang
struct Container {
#define OBJECT(_id, _displayName, _definedAs, _type)                                                                            \
  static constexpr uint32_t SH_CONCAT(_definedAs, TypeId) = uint32_t(_id);                                                      \
  static inline Type _definedAs{{SHType::Object, {.object = {.vendorId = VendorId, .typeId = SH_CONCAT(_definedAs, TypeId)}}}}; \
  static inline ObjectVar<_type> SH_CONCAT(_definedAs, ObjectVar){_displayName, VendorId, SH_CONCAT(_definedAs, TypeId)};

  OBJECT('drah', "GFX.TreeDrawable", TreeDrawable, SHTreeDrawable)
  OBJECT('draw', "GFX.Drawable", Drawable, SHDrawable)
  OBJECT('mesh', "GFX.Mesh", Mesh, MeshPtr)
  OBJECT('dque', "GFX.DrawQueue", DrawQueue, SHDrawQueue)
  OBJECT('tex_', "GFX.Texture", Texture, TexturePtr)
  OBJECT('rtex', "GFX.RenderTarget", RenderTarget, SHRenderTarget)

  DECL_ENUM_INFO(gfx::WindingOrder, WindingOrder, '_e0');
  DECL_ENUM_INFO(gfx::ShaderFieldBaseType, ShaderFieldBaseType, '_e1');
  DECL_ENUM_INFO(gfx::ProgrammableGraphicsStage, ProgrammableGraphicsStage, '_e2');
  DECL_ENUM_INFO(shader::DependencyType, DependencyType, '_e3');

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
  DECL_ENUM_INFO(BlendFactor_, BlendFactor, '_e4');

  enum class BlendOperation_ {
    Add = WGPUBlendOperation_Add,
    Subtract = WGPUBlendOperation_Subtract,
    ReverseSubtract = WGPUBlendOperation_ReverseSubtract,
    Min = WGPUBlendOperation_Min,
    Max = WGPUBlendOperation_Max,
  };
  DECL_ENUM_INFO(BlendOperation_, BlendOperation, '_e5');

  enum class FilterMode_ {
    Nearest = WGPUFilterMode_Nearest,
    Linear = WGPUFilterMode_Linear,
  };
  DECL_ENUM_INFO(FilterMode_, FilterMode, '_e6');

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
  DECL_ENUM_INFO(CompareFunction_, CompareFunction, '_e7');

  enum class ColorMask_ {
    None = WGPUColorWriteMask_None,
    Red = WGPUColorWriteMask_Red,
    Green = WGPUColorWriteMask_Green,
    Blue = WGPUColorWriteMask_Blue,
    Alpha = WGPUColorWriteMask_Alpha,
    All = WGPUColorWriteMask_All,
  };
  DECL_ENUM_INFO(ColorMask_, ColorMask, '_e8');

  enum class TextureType_ {
    Default = 0,
    Int,
    UInt,
    UNorm,
    UNormSRGB,
    SNorm,
    Float,
  };
  DECL_ENUM_INFO(TextureType_, TextureType, '_e9');

  OBJECT('feat', "GFX.Feature", Feature, FeaturePtr)
  static inline Type FeatureSeq = Type::SeqOf(Feature);

  OBJECT('pips', "GFX.PipelineStep", PipelineStep, PipelineStepPtr)
  static inline Type PipelineStepSeq = Type::SeqOf(PipelineStep);

  OBJECT('view', "GFX.View", View, SHView)
  static inline Type ViewSeq = Type::SeqOf(View);

  OBJECT('mat_', "GFX.Material", Material, SHMaterial)

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

  // Valid types for shader :Params
  static inline Type ShaderParamTable = Type::TableOf(ShaderParamTypes);

  static inline Types TextureTypes = {{
      Texture,
  }};

  static inline Types TextureVarTypes = {{
      Type::VariableOf(Texture),
  }};

  // Valid types for shader :Textures
  static inline Type TexturesTable = Type::TableOf(TextureTypes);

  static inline std::map<std::string, Type> DrawableInputTableTypes = {
      std::make_pair("Transform", CoreInfo::Float4x4Type),
      std::make_pair("Mesh", Mesh),
      std::make_pair("Params", ShaderParamTable),
      std::make_pair("Textures", TexturesTable),
      std::make_pair("Material", Material),
      std::make_pair("Features", FeatureSeq),
  };

#undef ENUM
#undef OBJECT
#undef SH_CONCAT
};
} // namespace detail
using Types = detail::Container;

} // namespace gfx

#endif // SH_EXTRA_GFX_SHARDS_TYPES
