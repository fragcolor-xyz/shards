#ifndef E325D8E3_F64E_413D_965B_DF275CF18AC4
#define E325D8E3_F64E_413D_965B_DF275CF18AC4

#include "shards.h"
#include "../gfx/egui/rust_interop.hpp"
#include "gfx/fwd.hpp"

namespace gfx {
// gfx::MainWindowGlobals::Type
SHTypeInfo *getMainWindowGlobalsType();
const char *getMainWindowGlobalsVarName();
SHTypeInfo *getQueueType();

SHVar MainWindowGlobals_getDefaultQueue(const SHVar &mainWindowGlobals);
Context *MainWindowGlobals_getContext(const SHVar &mainWindowGlobals);
Renderer *MainWindowGlobals_getRenderer(const SHVar &mainWindowGlobals);

DrawQueuePtr *getDrawQueueFromVar(const SHVar &var);

const egui::Input* getEguiWindowInputs(gfx::EguiInputTranslator* translator, const SHVar &mainWindowGlobals, float scalingFactor);
} // namespace gfx

#endif /* E325D8E3_F64E_413D_965B_DF275CF18AC4 */
