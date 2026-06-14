#ifndef GFX_SDL_NATIVE_WINDOW
#define GFX_SDL_NATIVE_WINDOW

#include <SDL3/SDL.h>
#include "../core/platform.hpp"

inline void *SDL_GetNativeWindowPtr(SDL_Window *window, bool useWayland = false) {
#if SH_WINDOWS
  return SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
#elif SH_ANDROID
  return SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, NULL);
#elif SH_LINUX
  if (useWayland) {
    return SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL);
  } else {
    // X11 window is a numeric XID (number property), not a pointer.
    return (void *)(uintptr_t)SDL_GetNumberProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
  }
#else
  return nullptr;
#endif
}

#endif // GFX_SDL_NATIVE_WINDOW
