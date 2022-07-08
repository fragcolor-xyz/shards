#ifndef GFX_WINDOW
#define GFX_WINDOW

#include "linalg.hpp"
#include <string>
#include <vector>

struct SDL_Window;
typedef union SDL_Event SDL_Event;
namespace gfx {
/// <div rustbindgen opaque></div>
struct WindowCreationOptions {
  int width = 1280;
  int height = 720;
  bool fullscreen = false;
  std::string title;
};

struct Window {
  SDL_Window *window = nullptr;

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

  // = draw surface size / draw scale
  float2 getVirtualDrawableSize() const;

  // Returns true when the window size is represented using virtual coordinates (OSX)
  // Otherwise the window size is the same as the drawable size (Windows)
  bool isWindowSizeVirtual() const;

  // Converts from  screen coordinates to virtual coordinates
  float2 toVirtualCoord(float2 screenCoord) const;

  // Converts from virtual coordinates to screen coordinates
  float2 toScreenCoord(float2 virtualCoord) const;

  ~Window();
};
}; // namespace gfx

#endif // GFX_WINDOW
