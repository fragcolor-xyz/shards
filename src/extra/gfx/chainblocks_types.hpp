#ifndef CB_EXTRA_GFX_CHAINBLOCKS_TYPES
#define CB_EXTRA_GFX_CHAINBLOCKS_TYPES

#include <chainblocks.hpp>
#include <common_types.hpp>
#include <foundation.hpp>
#include <gfx/enums.hpp>
#include <gfx/fwd.hpp>
#include <gfx/pipeline_step.hpp>
#include <memory>
#include <vector>

namespace gfx {
struct CBBasicShaderParameter {
  std::string key;
  chainblocks::ParamVar var;
  CBBasicShaderParameter(std::string key, chainblocks::ParamVar &&var) : key(key), var(std::move(var)) {}
};

struct MaterialParameters;
struct CBShaderParameters {
  std::vector<CBBasicShaderParameter> basic;

  void updateVariables(MaterialParameters &output);
};

struct CBDrawable {
  DrawablePtr drawable;
  chainblocks::ParamVar transformVar;
  chainblocks::ParamVar materialVar;
  CBShaderParameters shaderParameters;

  void updateVariables();
};

struct CBView {
  ViewPtr view;
  chainblocks::ParamVar viewTransformVar;

  void updateVariables();
};

struct CBMaterial {
  MaterialPtr material;
  CBShaderParameters shaderParameters;

  void updateVariables();
};

constexpr uint32_t VendorId = 'cgfx';

namespace detail {
using namespace chainblocks;
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

  static constexpr uint32_t DrawableTypeId = 'mesh';
  static inline Type Drawable{{CBType::Object, {.object = {.vendorId = VendorId, .typeId = DrawableTypeId}}}};
  static inline ObjectVar<CBDrawable> DrawableObjectVar{"GFX.Drawable", VendorId, DrawableTypeId};

  static constexpr uint32_t MeshTypeId = 'mesh';
  static inline Type Mesh{{CBType::Object, {.object = {.vendorId = VendorId, .typeId = MeshTypeId}}}};
  static inline ObjectVar<MeshPtr> MeshObjectVar{"GFX.Mesh", VendorId, MeshTypeId};

  static constexpr uint32_t WindingOrderTypeId = 'wind';
  static inline Type WindingOrder = Type::Enum(VendorId, WindingOrderTypeId);
  static inline EnumInfo<gfx::WindingOrder> WindingOrderEnumInfo{"WindingOrder", VendorId, WindingOrderTypeId};

  static constexpr uint32_t FeatureTypeId = 'feat';
  static inline Type Feature = Type::Enum(VendorId, FeatureTypeId);
  static inline ObjectVar<FeaturePtr> FeatureObjectVar{"GFX.Feature", VendorId, FeatureTypeId};

  static constexpr uint32_t PipelineStepTypeId = 'pips';
  static inline Type PipelineStep{{CBType::Object, {.object = {.vendorId = VendorId, .typeId = PipelineStepTypeId}}}};
  static inline ObjectVar<PipelineStepPtr> PipelineStepObjectVar{"GFX.PipelineStep", VendorId, PipelineStepTypeId};
  static inline Type PipelineStepSeq = Type::SeqOf(PipelineStep);

  static constexpr uint32_t ViewTypeId = 'view';
  static inline Type View{{CBType::Object, {.object = {.vendorId = VendorId, .typeId = ViewTypeId}}}};
  static inline ObjectVar<CBView> ViewObjectVar{"GFX.View", VendorId, ViewTypeId};
  static inline Type ViewSeq = Type::SeqOf(View);

  static constexpr uint32_t MaterialTypeId = 'mat_';
  static inline Type Material{{CBType::Object, {.object = {.vendorId = VendorId, .typeId = MaterialTypeId}}}};
  static inline ObjectVar<CBMaterial> MaterialObjectVar{"GFX.Material", VendorId, MaterialTypeId};
};
} // namespace detail
using Types = detail::Container;

} // namespace gfx

#endif // CB_EXTRA_GFX_CHAINBLOCKS_TYPES
