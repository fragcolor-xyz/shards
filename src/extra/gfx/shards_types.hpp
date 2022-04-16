#ifndef SH_EXTRA_GFX_SHARDS_TYPES
#define SH_EXTRA_GFX_SHARDS_TYPES

#include <shards.hpp>
#include <common_types.hpp>
#include <foundation.hpp>
#include <gfx/enums.hpp>
#include <gfx/fwd.hpp>
#include <gfx/pipeline_step.hpp>
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

  static constexpr uint32_t DrawableTypeId = 'draw';
  static inline Type Drawable{{SHType::Object, {.object = {.vendorId = VendorId, .typeId = DrawableTypeId}}}};
  static inline ObjectVar<SHDrawable> DrawableObjectVar{"GFX.Drawable", VendorId, DrawableTypeId};

  static constexpr uint32_t MeshTypeId = 'mesh';
  static inline Type Mesh{{SHType::Object, {.object = {.vendorId = VendorId, .typeId = MeshTypeId}}}};
  static inline ObjectVar<MeshPtr> MeshObjectVar{"GFX.Mesh", VendorId, MeshTypeId};

  static constexpr uint32_t WindingOrderTypeId = 'wind';
  static inline Type WindingOrder = Type::Enum(VendorId, WindingOrderTypeId);
  static inline EnumInfo<gfx::WindingOrder> WindingOrderEnumInfo{"WindingOrder", VendorId, WindingOrderTypeId};

  static constexpr uint32_t FeatureTypeId = 'feat';
  static inline Type Feature = Type::Enum(VendorId, FeatureTypeId);
  static inline ObjectVar<FeaturePtr> FeatureObjectVar{"GFX.Feature", VendorId, FeatureTypeId};

  static constexpr uint32_t ShaderFieldBaseTypeId = 'typ0';
  static inline Type ShaderFieldBaseType = Type::Enum(VendorId, ShaderFieldBaseTypeId);
  static inline EnumInfo<gfx::ShaderFieldBaseType> ShaderFieldBaseTypeEnumInfo{"ShaderType", VendorId, ShaderFieldBaseTypeId};

  static constexpr uint32_t PipelineStepTypeId = 'pips';
  static inline Type PipelineStep{{SHType::Object, {.object = {.vendorId = VendorId, .typeId = PipelineStepTypeId}}}};
  static inline ObjectVar<PipelineStepPtr> PipelineStepObjectVar{"GFX.PipelineStep", VendorId, PipelineStepTypeId};
  static inline Type PipelineStepSeq = Type::SeqOf(PipelineStep);

  static constexpr uint32_t ViewTypeId = 'view';
  static inline Type View{{SHType::Object, {.object = {.vendorId = VendorId, .typeId = ViewTypeId}}}};
  static inline ObjectVar<SHView> ViewObjectVar{"GFX.View", VendorId, ViewTypeId};
  static inline Type ViewSeq = Type::SeqOf(View);

  static constexpr uint32_t MaterialTypeId = 'mat_';
  static inline Type Material{{SHType::Object, {.object = {.vendorId = VendorId, .typeId = MaterialTypeId}}}};
  static inline ObjectVar<SHMaterial> MaterialObjectVar{"GFX.Material", VendorId, MaterialTypeId};
};
} // namespace detail
using Types = detail::Container;

} // namespace gfx

#endif // SH_EXTRA_GFX_SHARDS_TYPES
