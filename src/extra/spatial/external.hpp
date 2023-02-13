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

const egui::FullOutput *spatial_render_external_panel(ExtPanelPtr, const egui::Input &);
shards::spatial::PanelGeometry spatial_get_geometry(const ExtPanelPtr);

void spatial_add_panel(ExtPanelPtr, const SHVar &);
void spatial_remove_panel(ExtPanelPtr, const SHVar &);

// FIXME: temp because I don't want to deal with linalg from rust side for now
shards::spatial::PanelGeometry spatial_temp_get_geometry_from_transform(const SHVar &, const SHVar &);
}

namespace shards::Spatial {

struct ExternalPanel : public shards::spatial::Panel {
  ExtPanelPtr impl;

  ExternalPanel(ExtPanelPtr ptr) : impl(ptr) {}
  virtual const egui::FullOutput &render(const egui::Input &inputs) { return *spatial_render_external_panel(this->impl, inputs); }
  virtual spatial::PanelGeometry getGeometry() const { return spatial_get_geometry(this->impl); }
};

} // namespace shards::Spatial

#endif /* D376CFF4_416F_42C3_A139_B545528917AB */
