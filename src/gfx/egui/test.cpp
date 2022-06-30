#include <gfx/window.hpp>
#include <gfx/context.hpp>
#include <gfx/renderer.hpp>
#include <gfx/loop.hpp>
#include <gfx/drawable.hpp>
#include <gfx/view.hpp>
#include "egui_render_pass.hpp"
#include "rust_interop.hpp"

extern "C" {
void render_egui_test_frame(gfx::Context &context, gfx::DrawQueuePtr &queue, float deltaTime);
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

  egui::Input eguiInput{};
  bool quit = false;

  PipelineSteps pipelineSteps = {
      EguiRenderPass::createPipelineStep(queue),
  };

  while (!quit) {
    if (loop.beginFrame(1.0f / 120.0f, deltaTime)) {
      wnd.pollEvents(events);
      for (auto &event : events) {
        if (event.type == SDL_MOUSEMOTION) {
          eguiInput.cursorPosition[0] = event.motion.x;
          eguiInput.cursorPosition[1] = event.motion.y;
        } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
          eguiInput.cursorPosition[0] = event.button.x;
          eguiInput.cursorPosition[1] = event.button.y;
          if (event.button.state == SDL_PRESSED)
            eguiInput.mouseButton |= SDL_BUTTON(event.button.button);
          else
            eguiInput.mouseButton &= ~SDL_BUTTON(event.button.button);
        } else if (event.type == SDL_QUIT)
          quit = true;
      }

      queue->clear();
      render_egui_test_frame(ctx, queue, deltaTime);

      ctx.resizeMainOutputConditional(wnd.getDrawableSize());
      ctx.beginFrame();
      renderer.beginFrame();
      renderer.render(view, pipelineSteps);
      renderer.endFrame();
      ctx.endFrame();
    }
  }
}
