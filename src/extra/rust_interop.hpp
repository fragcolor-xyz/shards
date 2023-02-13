#ifndef E325D8E3_F64E_413D_965B_DF275CF18AC4
#define E325D8E3_F64E_413D_965B_DF275CF18AC4

#include "shards.h"
#include "../gfx/egui/rust_interop.hpp"
#include "gfx/fwd.hpp"

namespace gfx {
struct EguiInputTranslator;
}
namespace egui {
struct Input;
}

extern "C" {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"

const SHEnumInfo *util_findEnumInfo(int32_t vendorId, int32_t typeId);
const SHObjectInfo *util_findObjectInfo(int32_t vendorId, int32_t typeId);
SHString const util_type2Name(SHType type);

// gfx::GraphicsContext::Type
SHTypeInfo *gfx_getGraphicsContextType();
const char *gfx_getGraphicsContextVarName();
SHTypeInfo *gfx_getInputContextType();
const char *gfx_getInputContextVarName();
SHTypeInfo *gfx_getQueueType();

SHVar gfx_GraphicsContext_getDefaultQueue(const SHVar &graphicsContext);
gfx::Context *gfx_GraphicsContext_getContext(const SHVar &graphicsContext);
gfx::Renderer *gfx_GraphicsContext_getRenderer(const SHVar &graphicsContext);

gfx::DrawQueuePtr *gfx_getDrawQueueFromVar(const SHVar &var);

gfx::int4 gfx_getEguiMappedRegion(const SHVar &inputContext);
gfx::int4 gfx_getViewport(const SHVar &graphicsContext);
const egui::Input *gfx_getEguiWindowInputs(gfx::EguiInputTranslator *translator, const SHVar &graphicsContext,
                                           const SHVar &inputContext, float scalingFactor);

SHTypeInfo *spatial_getSpatialContextType();
const char *spatial_getSpatialContextVarName();
}

#endif /* E325D8E3_F64E_413D_965B_DF275CF18AC4 */
