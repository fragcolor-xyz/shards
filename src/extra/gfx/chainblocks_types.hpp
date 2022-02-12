#pragma once
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

static inline chainblocks::Types ShaderParamTypes{{
    chainblocks::CoreInfo::Float4x4Type,
    chainblocks::CoreInfo::Float4Type,
    chainblocks::CoreInfo::Float3Type,
    chainblocks::CoreInfo::Float2Type,
    chainblocks::CoreInfo::FloatType,
}};
static inline chainblocks::Types ShaderParamVarTypes{{
    chainblocks::Type::VariableOf(chainblocks::CoreInfo::Float4x4Type),
    chainblocks::Type::VariableOf(chainblocks::CoreInfo::Float4Type),
    chainblocks::Type::VariableOf(chainblocks::CoreInfo::Float3Type),
    chainblocks::Type::VariableOf(chainblocks::CoreInfo::Float2Type),
    chainblocks::Type::VariableOf(chainblocks::CoreInfo::FloatType),
}};
static inline chainblocks::Type ShaderParamTableType = chainblocks::Type::TableOf(ShaderParamTypes);

static constexpr uint32_t DrawableTypeId = 'mesh';
static inline chainblocks::Type DrawableType{{CBType::Object, {.object = {.vendorId = VendorId, .typeId = DrawableTypeId}}}};
extern chainblocks::ObjectVar<CBDrawable> DrawableObjectVar;

static constexpr uint32_t MeshTypeId = 'mesh';
static inline chainblocks::Type MeshType{{CBType::Object, {.object = {.vendorId = VendorId, .typeId = MeshTypeId}}}};
extern chainblocks::ObjectVar<MeshPtr> MeshObjectVar;

static constexpr uint32_t WindingOrderTypeId = 'wind';
static inline chainblocks::Type WindingOrderType = chainblocks::Type::Enum(VendorId, WindingOrderTypeId);
extern chainblocks::EnumInfo<WindingOrder> CullModeEnumInfo;

static constexpr uint32_t FeatureTypeId = 'feat';
static inline chainblocks::Type FeatureType = chainblocks::Type::Enum(VendorId, FeatureTypeId);
extern chainblocks::ObjectVar<FeaturePtr> FeatureObjectVar;

static constexpr uint32_t PipelineStepTypeId = 'pips';
static inline chainblocks::Type PipelineStepType{
    {CBType::Object, {.object = {.vendorId = VendorId, .typeId = PipelineStepTypeId}}}};
extern chainblocks::ObjectVar<PipelineStepPtr> PipelineStepObjectVar;
static inline chainblocks::Type PipelineStepSeqType = chainblocks::Type::SeqOf(PipelineStepType);

static constexpr uint32_t ViewTypeId = 'view';
static inline chainblocks::Type ViewType{{CBType::Object, {.object = {.vendorId = VendorId, .typeId = ViewTypeId}}}};
extern chainblocks::ObjectVar<CBView> ViewObjectVar;
static inline chainblocks::Type ViewSeqType = chainblocks::Type::SeqOf(ViewType);

static constexpr uint32_t MaterialTypeId = 'mat_';
static inline chainblocks::Type MaterialType{{CBType::Object, {.object = {.vendorId = VendorId, .typeId = MaterialTypeId}}}};
extern chainblocks::ObjectVar<CBMaterial> MaterialObjectVar;

} // namespace gfx
