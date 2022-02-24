#include "window.hpp"
#include "error_utils.hpp"
#include "platform.hpp"
#include "sdl_native_window.hpp"
#include <SDL.h>
#include <SDL_video.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#if GFX_WINDOWS
#include <Windows.h>
#elif GFX_APPLE
#include <SDL_metal.h>
#elif GFX_EMSCRIPTEN
#include <emscripten/html5.h>
#endif

namespace gfx {
void Window::init(const WindowCreationOptions &options) {
#ifdef _WIN32
  SetProcessDPIAware();
#endif

  auto initErr = SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO);
  if (initErr != 0) {
    throw formatException("SDL_Init failed: {}", SDL_GetError());
  }

  uint32_t flags = SDL_WINDOW_SHOWN;
  flags |= SDL_WINDOW_RESIZABLE | (options.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
#if !GFX_EMSCRIPTEN
  flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif
#if GFX_APPLE
  flags |= SDL_WINDOW_METAL;
#endif

  SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
  window = SDL_CreateWindow(options.title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            options.fullscreen ? 0 : options.width, options.fullscreen ? 0 : options.height, flags);

  if (!window) {
    throw formatException("SDL_CreateWindow failed: {}", SDL_GetError());
  }
}

void Window::cleanup() {
  if (window) {
    SDL_DestroyWindow(window);
    SDL_Quit();
    window = nullptr;
  }
}

void Window::pollEvents(std::vector<SDL_Event> &events) {
  events.clear();
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    events.push_back(event);
  }
}

void *Window::getNativeWindowHandle() {
#if GFX_APPLE
  return nullptr;
#elif GFX_EMSCRIPTEN
  return (void *)("#canvas");
#else
  return (void *)SDL_GetNativeWindowPtr(window);
#endif
}

float2 Window::getDrawScale() const {
  float2 windowSize = float2(getSize());
  float2 drawableSize = float2(getDrawableSize());
  return drawableSize / windowSize;
}

int2 Window::getDrawableSize() const {
  int2 r;
#if GFX_APPLE
  SDL_Metal_GetDrawableSize(window, &r.x, &r.y);
#else
  r = getSize();
#endif
  return r;
}

int2 Window::getSize() const {
  int2 r;
  SDL_GetWindowSize(window, &r.x, &r.y);
  return r;
}

Window::~Window() { cleanup(); }
} // namespace gfx