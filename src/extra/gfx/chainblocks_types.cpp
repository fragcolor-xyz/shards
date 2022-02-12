#include "chainblocks_types.hpp"

namespace gfx {
chainblocks::ObjectVar<CBDrawable> DrawableObjectVar{"GFX.Drawable", VendorId, DrawableTypeId};
chainblocks::ObjectVar<MeshPtr> MeshObjectVar{"GFX.Mesh", VendorId, MeshTypeId};
chainblocks::EnumInfo<WindingOrder> CullModeEnumInfo{"WindingOrder", VendorId, WindingOrderTypeId};
chainblocks::ObjectVar<FeaturePtr> FeatureObjectVar{"GFX.Feature", VendorId, FeatureTypeId};
chainblocks::ObjectVar<PipelineStepPtr> PipelineStepObjectVar{"GFX.PipelineStep", VendorId, PipelineStepTypeId};
chainblocks::ObjectVar<CBView> ViewObjectVar{"GFX.View", VendorId, ViewTypeId};
chainblocks::ObjectVar<CBMaterial> MaterialObjectVar{"GFX.Material", VendorId, MaterialTypeId};
} // namespace gfx
