#ifndef D376CFF4_416F_42C3_A139_B545528917AB
#define D376CFF4_416F_42C3_A139_B545528917AB

#include <common_types.hpp>
#include <foundation.hpp>
#include <shards.hpp>
#include <spatial/panel.hpp>

typedef void *ExtPanelPtr;

extern "C" {  
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"

const egui::FullOutput *spatial_render_external_panel(ExtPanelPtr, const egui::Input &inputs);
shards::spatial::PanelGeometry spatial_get_geometry(const ExtPanelPtr);
}

namespace shards::Spatial {
struct ExternalPanel : public shards::spatial::Panel {
  ExtPanelPtr impl;

  ExternalPanel(ExtPanelPtr ptr) : impl(ptr) {}
  const egui::FullOutput &render(const egui::Input &inputs) { return *spatial_render_external_panel(this->impl, inputs); }
  spatial::PanelGeometry getGeometry() const{ return spatial_get_geometry(this->impl); }
};
} // namespace shards::Spatial


#endif /* D376CFF4_416F_42C3_A139_B545528917AB */
