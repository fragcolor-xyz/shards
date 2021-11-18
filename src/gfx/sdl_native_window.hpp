#pragma once

#include "SDL_syswm.h"
#include "error_utils.hpp"
#include <SDL_error.h>
#include <stdexcept>

inline void *SDL_GetNativeWindowPtr(SDL_Window *window) {
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
#else
	return nullptr;
#endif
}

inline void *SDL_GetNativeDisplayPtr(SDL_Window *window) {
#ifdef __EMSCRIPTEN__
	return nullptr;
#else
	SDL_SysWMinfo winInfo{};
	SDL_version sdlVer{};
	SDL_VERSION(&sdlVer);
	winInfo.version = sdlVer;
	if (!SDL_GetWindowWMInfo(window, &winInfo)) {
		throw gfx::formatException("Failed to call SDL_GetWindowWMInfo: {}", SDL_GetError());
	}
#if defined(__linux__)
	return (void *)winInfo.info.x11.display;
#else
	return nullptr;
#endif
#endif
}
