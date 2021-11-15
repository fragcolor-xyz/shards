#pragma once

#include "linalg.hpp"
#include <string>
#include <vector>

struct SDL_Window;
typedef union SDL_Event SDL_Event;
namespace gfx {
struct WindowCreationOptions {
  int width = 1280;
  int height = 720;
  bool fullscreen = false;
  std::string title;
};

struct Window {
  SDL_Window *window = nullptr;
#if defined(__APPLE__)
  SDL_MetalView metalView{nullptr};
#endif

  void init(const WindowCreationOptions &options = WindowCreationOptions{});
  void cleanup();
  void pollEvents(std::vector<SDL_Event> &events);
  void *getNativeWindowHandle();

  // draw surface size
  int2 getDrawableSize() const;

  // window size
  int2 getSize() const;

  // = draw surface size / window size
  float2 getDrawScale() const;

  ~Window();
};
}; // namespace gfx
