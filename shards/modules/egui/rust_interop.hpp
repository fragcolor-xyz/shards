#ifndef D51F59C5_BA16_47C5_B59B_0C4D8273CADB
#define D51F59C5_BA16_47C5_B59B_0C4D8273CADB

#include <shards/shards.h>
#include <gfx/rust_interop.hpp>
#include "egui_types.hpp"
#include "renderer.hpp"
#include "input.hpp"

namespace gfx {
struct EguiInputTranslator;
}

namespace egui {
struct Input;
}

extern "C" {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"

// gfx::GraphicsContext::Type
SHTypeInfo *gfx_getGraphicsContextType();
const char *gfx_getGraphicsContextVarName();
SHTypeInfo *gfx_getWindowContextType();
const char *gfx_getWindowContextVarName();
SHTypeInfo *gfx_getInputContextType();
const char *gfx_getInputContextVarName();
SHTypeInfo *gfx_getQueueType();

SHVar gfx_GraphicsContext_getDefaultQueue(const SHVar &graphicsContext);
gfx::Context *gfx_GraphicsContext_getContext(const SHVar &graphicsContext);
gfx::Renderer *gfx_GraphicsContext_getRenderer(const SHVar &graphicsContext);

gfx::DrawQueuePtr *gfx_getDrawQueueFromVar(const SHVar &var);

gfx::int4 gfx_getEguiMappedRegion(const SHVar &windowContext);
gfx::int4 gfx_getViewport(const SHVar &graphicsContext);
const egui::Input *gfx_getEguiWindowInputs(gfx::EguiInputTranslator *translator, const SHVar *graphicsContext,
                                           const SHVar &inputContext, float scalingFactor);
void gfx_applyEguiOutputs(gfx::EguiInputTranslator *translator, const egui::FullOutput &output, const SHVar &inputContext);
}

#endif /* D51F59C5_BA16_47C5_B59B_0C4D8273CADB */
