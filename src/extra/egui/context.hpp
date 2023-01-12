#ifndef ED3C732D_AC03_4B15_8EC8_BB15262E26DA
#define ED3C732D_AC03_4B15_8EC8_BB15262E26DA

#include <shards.h>
#include <gfx/egui/egui_types.hpp>

typedef void *EguiContext;

extern "C" {
EguiContext egui_createContext();
void egui_destroyContext(EguiContext);
void egui_getExposedTypeInfo(EguiContext, SHExposedTypesInfo &outTypes);
void egui_warmup(EguiContext, SHContext *shContext);
void egui_cleanup(EguiContext);
const char *egui_activate(EguiContext eguiContext, const egui::Input &eguiInput, const Shards &shards, SHContext *shContext,
                          const SHVar &input, SHVar &output);
const egui::FullOutput *egui_getOutput(const EguiContext eguiContext);
}

#endif /* ED3C732D_AC03_4B15_8EC8_BB15262E26DA */
