#pragma once
#include <gfx/context_data.hpp>
#include <gfx/fwd.hpp>
#include <gfx/linalg.hpp>
#include <gfx/window.hpp>
#include <imgui.h>

namespace gfx {

inline ImVec2 toImVec2(const float2 &v) { return ImVec2(v.x, v.y); }

inline ImVec2 toImVec2(const int2 &v) { return ImVec2(v.x, v.y); }

struct ImGuiRenderer : public ContextData {
  Context &context;
  ImGuiContext *imguiContext{};
  SDL_Window *sdlWindow{};

  ImGuiRenderer(Context &context);
  ImGuiRenderer(const ImGuiContext &other) = delete;
  ~ImGuiRenderer();

  void beginFrame(const std::vector<SDL_Event> &inputEvents);
  void endFrame();

protected:
  void releaseContextData() override;

private:
  void init();
  void initContextData();
  void cleanup();
  void render();
};

} // namespace gfx
