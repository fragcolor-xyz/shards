#include "window.hpp"
#include "error_utils.hpp"
#include "../core/platform.hpp"
#include "sdl_native_window.hpp"
#include <SDL.h>
#include <SDL_video.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include "fmt.hpp"

#if SH_WINDOWS
#include <Windows.h>
#elif SH_APPLE
#include <SDL_metal.h>
#elif SH_EMSCRIPTEN
#include <emscripten/html5.h>
#elif SH_ANDROID
#include <android/native_window.h>
#endif

namespace gfx {

struct EmscriptenInternal {
  std::string canvasContainerTag;

  static EmscriptenInternal &get() {
    static EmscriptenInternal instance;
    return instance;
  }

  int2 getCanvasContainerSize() {
    if (canvasContainerTag.empty())
      throw std::logic_error("Emscripten canvas container tag not set");

    double cw{}, ch{};
    emscripten_get_element_css_size(canvasContainerTag.c_str(), &cw, &ch);
    return int2(cw, ch);
  }
};

void EmscriptenWindow::setCanvasContainer(const char *tag) { EmscriptenInternal::get().canvasContainerTag = tag; }

void Window::init(const WindowCreationOptions &options) {
  if (window)
    throw std::logic_error("Already initialized");

#ifdef _WIN32
  SetProcessDPIAware();
#endif

  auto initErr = SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO);
  if (initErr != 0) {
    throw formatException("SDL_Init failed: {}", SDL_GetError());
  }

  uint32_t flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;

  int width{options.width}, height{options.height};

// Base OS flags
#if SH_IOS || SH_ANDROID
  flags |= SDL_WINDOW_FULLSCREEN;
#else
  flags |= (options.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
#endif

  if ((flags & SDL_WINDOW_FULLSCREEN) != 0) {
    width = 0;
    height = 0;
  }

#if !SH_EMSCRIPTEN
  flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif
#if SH_APPLE
  flags |= SDL_WINDOW_METAL;
#endif

  SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

#if SH_EMSCRIPTEN
  int2 canvasContainerSize = EmscriptenInternal::get().getCanvasContainerSize();
  width = canvasContainerSize.x;
  height = canvasContainerSize.y;
#endif

  SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
  SDL_SetHint(SDL_HINT_VIDEO_EXTERNAL_CONTEXT, "1");
  window = SDL_CreateWindow(options.title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, flags);

  if (!window) {
    throw formatException("SDL_CreateWindow failed: {}", SDL_GetError());
  }

#if SH_APPLE
  metalView.emplace(window);
#endif
}

void Window::cleanup() {
  if (window) {
#if SH_APPLE
    metalView.reset();
#endif

    SDL_DestroyWindow(window);
    SDL_Quit();
    window = nullptr;
  }
}

void Window::pollEvents(std::vector<SDL_Event> &events) {
  events.clear();
  SDL_Event event;
  while (pollEvent(event)) {
    events.push_back(event);
  }
}

bool Window::pollEvent(SDL_Event &outEvent) { return SDL_PollEvent(&outEvent); }

void *Window::getNativeWindowHandle() {
#if SH_APPLE
  return nullptr;
#elif SH_EMSCRIPTEN
  return (void *)("#canvas");
#else
  return (void *)SDL_GetNativeWindowPtr(window);
#endif
}

int2 Window::getDrawableSize() const {
  int2 r;
#if SH_APPLE
  SDL_Metal_GetDrawableSize(window, &r.x, &r.y);
#elif SH_ANDROID
  ANativeWindow *nativeWindow = (ANativeWindow *)SDL_GetNativeWindowPtr(window);
  r.x = ANativeWindow_getWidth(nativeWindow);
  r.y = ANativeWindow_getHeight(nativeWindow);
#else
  r = getSize();
#endif
  return r;
}

float2 Window::getInputScale() const { return float2(getDrawableSize()) / float2(getSize()); }

int2 Window::getSize() const {
  int2 r;
  SDL_GetWindowSize(window, &r.x, &r.y);
  return r;
}

void Window::resize(int2 targetSize) { SDL_SetWindowSize(window, targetSize.x, targetSize.y); }

// window position
int2 Window::getPosition() const {
  int2 r;
  SDL_GetWindowPosition(window, &r.x, &r.y);
  return r;
}

void Window::move(int2 targetPosition) { SDL_SetWindowPosition(window, targetPosition.x, targetPosition.y); }

static float2 getUIScaleFromDisplayDPI(int displayIndex) {
  float2 dpi;
  float diagonalDpi;
  if (SDL_GetDisplayDPI(displayIndex, &diagonalDpi, &dpi.x, &dpi.y) != 0) {
    return float2(1.0f);
  }

#if SH_WINDOWS
  // DPI for 100% on windows
  return dpi / 96;
#else
  const float referenceDpi = 440;
  const float referenceScale = 4.0;
  return dpi / referenceDpi * referenceScale;
#endif
}

float Window::getUIScale() const {
  float2 scale{};
#if SH_APPLE
  // On apple, derive display scale from drawable size / window size
  scale = float2(getDrawableSize()) / float2(getSize());
#else
  scale = getUIScaleFromDisplayDPI(SDL_GetWindowDisplayIndex(window));
#endif
  return std::max<float>(scale.x, scale.y);
}

bool Window::isWindowSizeInPixels() {
#if SH_APPLE
  return false;
#else
  return true;
#endif
}

Window::~Window() { cleanup(); }
} // namespace gfx
