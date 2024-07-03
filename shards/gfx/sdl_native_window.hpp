#ifndef GFX_SDL_NATIVE_WINDOW
#define GFX_SDL_NATIVE_WINDOW

#include <SDL3/SDL.h>
#include "../core/platform.hpp"

inline void *SDL_GetNativeWindowPtr(SDL_Window *window) {

#if SH_WINDOWS
  return SDL_GetProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
#elif SH_ANDROID
  return SDL_GetProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, NULL);
#elif SH_LINUX
  return SDL_GetProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, NULL);
#else
  return nullptr;
#endif
}

inline void *SDL_GetNativeDisplayPtr(SDL_Window *window) {
#if SH_LINUX
  return SDL_GetProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
#else
  return nullptr;
#endif
}

#endif // GFX_SDL_NATIVE_WINDOW
