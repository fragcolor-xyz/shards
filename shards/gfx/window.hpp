#ifndef AEC3EB9B_7819_42B0_BB31_40818880ECE2
#define AEC3EB9B_7819_42B0_BB31_40818880ECE2

#include "linalg.hpp"
#include "../core/platform.hpp"
#include <string>
#include <vector>
#include <optional>

namespace gfx {
/// <div rustbindgen opaque></div>
struct WindowCreationOptions {
  int width = 1280;
  int height = 720;
  bool fullscreen = false;
  std::string title;
};

#if SH_APPLE
struct MetalViewContainer {
  SDL_MetalView view{};
  void *layer{};

  MetalViewContainer(SDL_Window *window) {
    view = SDL_Metal_CreateView(window);
    layer = SDL_Metal_GetLayer(view);
  }
  ~MetalViewContainer() { SDL_Metal_DestroyView(view); }
  MetalViewContainer(const MetalViewContainer &other) = delete;
  operator void *() const { return layer; }
};
#endif

} // namespace gfx

#if SH_EMSCRIPTEN
#include "window_em.hpp"
#else
#include <SDL_events.h>

struct SDL_Window;
namespace gfx {
struct Window {
  SDL_Window *window = nullptr;

#if SH_APPLE
  std::optional<MetalViewContainer> metalView;
#endif

  void init(const WindowCreationOptions &options = WindowCreationOptions{});
  void cleanup();

  bool isInitialized() const { return window != nullptr; }

  template <typename T> void pollEventsForEach(T &&callback) {
    SDL_Event event;
    while (pollEvent(event)) {
      callback(event);
    }
  }

  void pollEvents(std::vector<SDL_Event> &events);
  bool pollEvent(SDL_Event &outEvent);

  // Only for platforms that automatically size the output window
  void maybeAutoResize();

  void *getNativeWindowHandle();

  // draw surface size
  int2 getDrawableSize() const;

  // The scale that converts from input coordinates to drawable surface coordinates
  float2 getInputScale() const;

  // window size
  int2 getSize() const;

  void resize(int2 targetSize);

  // window position
  int2 getPosition() const;

  void move(int2 targetPosition);

  // OS UI scaling factor for the display the window is on
  float getUIScale() const;

  // Returns true when the window sizes are specified in pixels (on windows for example)
  static bool isWindowSizeInPixels();

  ~Window();
};
}; // namespace gfx
#endif

#endif /* AEC3EB9B_7819_42B0_BB31_40818880ECE2 */
