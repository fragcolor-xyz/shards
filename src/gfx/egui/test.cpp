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
void *new_app();
void render_egui_test_frame(void *app, gfx::Context &context, gfx::DrawQueuePtr &queue, const std::vector<SDL_Event> &events,
                            double time, float deltaTime);
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

  void *rustAppState = new_app();

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

      queue->clear();
      render_egui_test_frame(rustAppState, ctx, queue, events, loop.getAbsoluteTime(), deltaTime);

      ctx.resizeMainOutputConditional(wnd.getDrawableSize());
      ctx.beginFrame();
      renderer.beginFrame();
      renderer.render(view, pipelineSteps);
      renderer.endFrame();
      ctx.endFrame();
    }
  }
}
