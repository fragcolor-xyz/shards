#ifndef D9CD6261_7BAB_402B_BC43_8BC66551E95D
#define D9CD6261_7BAB_402B_BC43_8BC66551E95D

#include "egui_types.hpp"
#include <gfx/linalg.hpp>
#include <gfx/fwd.hpp>
#include <memory>

namespace gfx {
struct Window;
struct EguiRendererImpl;

/// <div rustbindgen opaque></div>
struct EguiRenderer {
  std::shared_ptr<EguiRendererImpl> impl;

  EguiRenderer();

  // clipGeometry controls settings of screen space clip rectangles based on egui
  void render(const egui::FullOutput &output, const float4x4 &rootTransform, const gfx::DrawQueuePtr &drawQueue,
              bool clipGeometry = true);
  void renderNoTransform(const egui::FullOutput &output, const gfx::DrawQueuePtr &drawQueue);

  static EguiRenderer *create();
  static void destroy(EguiRenderer *renderer);

  static float getDrawScale(Window &window);
};

} // namespace gfx

#endif /* D9CD6261_7BAB_402B_BC43_8BC66551E95D */
