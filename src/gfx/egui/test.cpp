#include <gfx/window.hpp>
#include <gfx/context.hpp>
#include <gfx/renderer.hpp>
#include <gfx/loop.hpp>
#include <gfx/drawable.hpp>
#include <gfx/view.hpp>
#include <spdlog/spdlog.h>
#include "egui_render_pass.hpp"
#include "input.hpp"
#include "rust_interop.hpp"

extern "C" {
void render_egui_test_frame(gfx::Context &context, gfx::DrawQueuePtr &queue, gfx::EguiInputTranslator& inputTranslator, const egui::Input *input);
}

using namespace gfx;
int main() {
  Window wnd;
  wnd.init();

  Context ctx;
  ctx.init(wnd);

  // Just for clearing
  Renderer renderer(ctx);
  DrawQueuePtr queue = std::make_shared<DrawQueue>();
  ViewPtr view = std::make_shared<View>();

  Loop loop;
  float deltaTime;
  std::vector<SDL_Event> events;

  EguiInputTranslator inputTranslator;

  bool quit = false;

  PipelineSteps pipelineSteps = {
      EguiRenderPass::createPipelineStep(queue),
  };

  spdlog::set_level(spdlog::level::debug);

  while (!quit) {
    if (loop.beginFrame(1.0f / 120.0f, deltaTime)) {
      wnd.pollEvents(events);
      for (auto &event : events) {
        if (event.type == SDL_QUIT)
          quit = true;
      }

      const egui::Input *eguiInput = inputTranslator.translateFromInputEvents(events, wnd, loop.getAbsoluteTime(), deltaTime);

      queue->clear();
      render_egui_test_frame(ctx, queue, inputTranslator, eguiInput);

      ctx.resizeMainOutputConditional(wnd.getDrawableSize());
      ctx.beginFrame();
      renderer.beginFrame();
      renderer.render(view, pipelineSteps);
      renderer.endFrame();
      ctx.endFrame();
    }
  }
}
