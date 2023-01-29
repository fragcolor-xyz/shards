#ifndef ED3C732D_AC03_4B15_8EC8_BB15262E26DA
#define ED3C732D_AC03_4B15_8EC8_BB15262E26DA

#include <shards.h>
#include <gfx/egui/egui_types.hpp>

typedef void *EguiHost;

extern "C" {
EguiHost egui_createHost();
void egui_destroyHost(EguiHost);

void egui_hostGetExposedTypeInfo(EguiHost, SHExposedTypesInfo &outTypes);
void egui_hostWarmup(EguiHost, SHContext *shContext);
void egui_hostCleanup(EguiHost);
const char *egui_hostActivate(EguiHost, const egui::Input &eguiInput, const Shards &shards, SHContext *shContext,
                          const SHVar &input, SHVar &output);
const egui::FullOutput *egui_hostGetOutput(const EguiHost);
}

#endif /* ED3C732D_AC03_4B15_8EC8_BB15262E26DA */
