#ifndef D9CD6261_7BAB_402B_BC43_8BC66551E95D
#define D9CD6261_7BAB_402B_BC43_8BC66551E95D

#include "egui_types.hpp"
#include "../fwd.hpp"
#include <memory>

namespace gfx {
struct EguiRendererImpl;

/// <div rustbindgen opaque></div>
struct EguiRenderer {
  std::shared_ptr<EguiRendererImpl> impl;

  EguiRenderer();

  void render(const egui::FullOutput &output, const gfx::DrawQueuePtr &drawQueue);

  static EguiRenderer *create();
  static void destroy(EguiRenderer *renderer);
};

} // namespace gfx

#endif /* D9CD6261_7BAB_402B_BC43_8BC66551E95D */
