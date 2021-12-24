#pragma once

#include <bx/platform.h>

#include <SDL.h>
#if BX_PLATFORM_WINDOWS || BX_PLATFORM_LINUX
#define HAVE_SDL_SYSWM
#include <SDL_syswm.h>
#endif

#include "error_utils.hpp"
#include <stdexcept>

inline void *SDL_GetNativeWindowPtr(SDL_Window *window) {
#ifdef HAVE_SDL_SYSWM
	SDL_SysWMinfo winInfo{};
	SDL_version sdlVer{};
	SDL_VERSION(&sdlVer);
	winInfo.version = sdlVer;
	if (!SDL_GetWindowWMInfo(window, &winInfo)) {
		throw gfx::formatException("Failed to call SDL_GetWindowWMInfo: {}", SDL_GetError());
	}
#if defined(_WIN32)
	return winInfo.info.win.window;
#elif defined(__linux__)
	return (void *)winInfo.info.x11.window;
#endif
#else
	return nullptr;
#endif
}

inline void *SDL_GetNativeDisplayPtr(SDL_Window *window) {
#if BX_PLATFORM_LINUX
	SDL_SysWMinfo winInfo{};
	SDL_version sdlVer{};
	SDL_VERSION(&sdlVer);
	winInfo.version = sdlVer;
	if (!SDL_GetWindowWMInfo(window, &winInfo)) {
		throw gfx::formatException("Failed to call SDL_GetWindowWMInfo: {}", SDL_GetError());
	}

	return (void *)winInfo.info.x11.display;
#else
	return nullptr;
#endif
}
