#include <gfx/window.hpp>
#include <gfx/context.hpp>
#include <gfx/renderer.hpp>
#include <gfx/loop.hpp>
#include "egui_interop.hpp"

extern "C" {
void render_egui_test_frame(egui::EguiRenderer* renderer, float deltaTime);
}

using namespace gfx;
int main() {
  Window wnd;
  wnd.init();

  Context ctx;
  ctx.init(wnd);

  // Just for clearing
  Renderer renderer(ctx);

  egui::EguiRenderer eguiRenderer(renderer);

  Loop loop;
  float deltaTime;
  std::vector<SDL_Event> events;

  egui::Input eguiInput{};
  bool quit = false;

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
      ctx.resizeMainOutputConditional(wnd.getDrawableSize());

      ctx.beginFrame();
      renderer.beginFrame();
      render_egui_test_frame(&eguiRenderer, deltaTime);
      renderer.endFrame();
      ctx.endFrame();
    }
  }
}
