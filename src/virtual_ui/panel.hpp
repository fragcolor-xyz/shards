#ifndef CA03D35E_D054_4C56_9F4B_949787F0B26F
#define CA03D35E_D054_4C56_9F4B_949787F0B26F

#include <gfx/linalg.hpp>
#include <gfx/fwd.hpp>
#include <gfx/egui/egui_types.hpp>

namespace shards::vui {
struct PanelGeometry {
  gfx::float3 anchor;
  gfx::float3 center;
  gfx::float3 up;
  gfx::float3 right;
  gfx::float2 size;

  gfx::float3 getTopLeft() const { return center - right * size.x * 0.5f + up * size.y * 0.5f; }
};

struct Panel {
  virtual ~Panel() = default;
  virtual const egui::FullOutput& render(const egui::Input &inputs) = 0;
  virtual PanelGeometry getGeometry() const = 0;
};
} // namespace shards::vui

#endif /* CA03D35E_D054_4C56_9F4B_949787F0B26F */
