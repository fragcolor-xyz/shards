#pragma once
#include <chainblocks.hpp>
#include <foundation.hpp>
#include <gfx/enums.hpp>
#include <gfx/pipeline_step.hpp>
#include <memory>

namespace gfx {

struct Drawable;
struct Mesh;
struct Feature;
struct View;
typedef std::shared_ptr<Drawable> DrawablePtr;
typedef std::shared_ptr<Mesh> MeshPtr;
typedef std::shared_ptr<Feature> FeaturePtr;
typedef std::shared_ptr<View> ViewPtr;

struct CBDrawable {
	DrawablePtr drawable;
	chainblocks::ParamVar transformVar;

	void updateVariables();
};

struct CBView {
	ViewPtr view;
	chainblocks::ParamVar viewTransformVar;

	void updateVariables();
};

constexpr uint32_t VendorId = 'cgfx';

static constexpr uint32_t DrawableTypeId = 'mesh';
static inline chainblocks::Type DrawableType{{CBType::Object, {.object = {.vendorId = VendorId, .typeId = DrawableTypeId}}}};
static chainblocks::ObjectVar<CBDrawable> DrawableObjectVar{"GFX.Drawable", VendorId, DrawableTypeId};

static constexpr uint32_t MeshTypeId = 'mesh';
static inline chainblocks::Type MeshType{{CBType::Object, {.object = {.vendorId = VendorId, .typeId = MeshTypeId}}}};
static chainblocks::ObjectVar<MeshPtr> MeshObjectVar{"GFX.Mesh", VendorId, MeshTypeId};

static constexpr uint32_t WindingOrderTypeId = 'wind';
static inline chainblocks::Type WindingOrderType = chainblocks::Type::Enum(VendorId, WindingOrderTypeId);
static inline chainblocks::EnumInfo<WindingOrder> CullModeEnumInfo{"WindingOrder", VendorId, WindingOrderTypeId};

static constexpr uint32_t FeatureTypeId = 'feat';
static inline chainblocks::Type FeatureType = chainblocks::Type::Enum(VendorId, FeatureTypeId);
static chainblocks::ObjectVar<FeaturePtr> FeatureObjectVar{"GFX.Feature", VendorId, FeatureTypeId};

static constexpr uint32_t PipelineStepTypeId = 'pips';
static inline chainblocks::Type PipelineStepType{{CBType::Object, {.object = {.vendorId = VendorId, .typeId = PipelineStepTypeId}}}};
static inline chainblocks::ObjectVar<PipelineStepPtr> PipelineStepObjectVar{"GFX.PipelineStep", VendorId, PipelineStepTypeId};
static inline chainblocks::Type PipelineStepSeqType = chainblocks::Type::SeqOf(PipelineStepType);

static constexpr uint32_t ViewTypeId = 'view';
static inline chainblocks::Type ViewType{{CBType::Object, {.object = {.vendorId = VendorId, .typeId = ViewTypeId}}}};
static inline chainblocks::ObjectVar<CBView> ViewObjectVar{"GFX.View", VendorId, ViewTypeId};
static inline chainblocks::Type ViewSeqType = chainblocks::Type::SeqOf(ViewType);

} // namespace gfx