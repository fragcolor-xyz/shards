#ifndef CA03D35E_D054_4C56_9F4B_949787F0B26F
#define CA03D35E_D054_4C56_9F4B_949787F0B26F

#include <gfx/egui/egui_types.hpp>
#include "rust_interop.hpp"

namespace shards::spatial {
struct Panel {
  virtual ~Panel() = default;
  virtual const egui::FullOutput &render(const egui::Input &inputs) = 0;
  virtual PanelGeometry getGeometry() const = 0;
};
} // namespace shards::spatial

#endif /* CA03D35E_D054_4C56_9F4B_949787F0B26F */
