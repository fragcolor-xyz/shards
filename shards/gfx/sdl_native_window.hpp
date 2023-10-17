#ifndef GFX_SDL_NATIVE_WINDOW
#define GFX_SDL_NATIVE_WINDOW

#include <SDL.h>

#define NEED_SYSWM (SH_WINDOWS || SH_ANDROID || SH_LINUX)

#include <SDL_syswm.h>

#include "error_utils.hpp"
#include <shards/core/platform.hpp>
#include <stdexcept>

inline void *SDL_GetNativeWindowPtr(SDL_Window *window) {
#if NEED_SYSWM
  SDL_SysWMinfo winInfo{};
  SDL_version sdlVer{};
  SDL_VERSION(&sdlVer);
  winInfo.version = sdlVer;
  if (!SDL_GetWindowWMInfo(window, &winInfo)) {
    throw gfx::formatException("Failed to call SDL_GetWindowWMInfo: {}", SDL_GetError());
  }
#endif

#if SH_WINDOWS
  return winInfo.info.win.window;
#elif SH_ANDROID
  return winInfo.info.android.window;
#elif SH_LINUX
  return (void *)winInfo.info.x11.window;
#else
  return nullptr;
#endif
}

inline void *SDL_GetNativeDisplayPtr(SDL_Window *window) {
#if NEED_SYSWM
  SDL_SysWMinfo winInfo{};
  SDL_version sdlVer{};
  SDL_VERSION(&sdlVer);
  winInfo.version = sdlVer;
  if (!SDL_GetWindowWMInfo(window, &winInfo)) {
    throw gfx::formatException("Failed to call SDL_GetWindowWMInfo: {}", SDL_GetError());
  }
#endif

#if SH_LINUX
  return (void *)winInfo.info.x11.display;
#else
  return nullptr;
#endif
}

#endif // GFX_SDL_NATIVE_WINDOW
