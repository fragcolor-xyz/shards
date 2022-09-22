#ifndef E325D8E3_F64E_413D_965B_DF275CF18AC4
#define E325D8E3_F64E_413D_965B_DF275CF18AC4

#include "shards.h"
#include "../gfx/egui/rust_interop.hpp"
#include "gfx/fwd.hpp"

extern "C" {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"

// gfx::MainWindowGlobals::Type
SHTypeInfo *gfx_getMainWindowGlobalsType();
const char *gfx_getMainWindowGlobalsVarName();
SHTypeInfo *gfx_getQueueType();

SHVar gfx_MainWindowGlobals_getDefaultQueue(const SHVar &mainWindowGlobals);
gfx::Context *gfx_MainWindowGlobals_getContext(const SHVar &mainWindowGlobals);
gfx::Renderer *gfx_MainWindowGlobals_getRenderer(const SHVar &mainWindowGlobals);

gfx::DrawQueuePtr *gfx_getDrawQueueFromVar(const SHVar &var);

gfx::int4 gfx_getEguiMappedRegion(const SHVar &mainWindowGlobals);
const egui::Input *gfx_getEguiWindowInputs(gfx::EguiInputTranslator *translator, const SHVar &mainWindowGlobals,
                                           float scalingFactor);
}

#endif /* E325D8E3_F64E_413D_965B_DF275CF18AC4 */
