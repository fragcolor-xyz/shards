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

typedef void (*PanelRenderCallback)(void *Context, const egui::FullOutput &output);

struct Panel {
  gfx::float2 size = gfx::float2(1024, 1024);
  gfx::float2 alignment = gfx::float2(0.5f);
  gfx::float4x4 transform = linalg::identity;

  virtual ~Panel() = default;
  virtual void render(void* context, const egui::Input &inputs, PanelRenderCallback render) = 0;

  PanelGeometry getGeometry() const;
};
} // namespace shards::vui

#endif /* CA03D35E_D054_4C56_9F4B_949787F0B26F */
