#pragma once

#include "SDL_syswm.h"
#include "chainblocks.hpp"

inline void *SDL_GetNativeWindowPtr(SDL_Window *window) {
	SDL_SysWMinfo winInfo{};
	SDL_version sdlVer{};
	SDL_VERSION(&sdlVer);
	winInfo.version = sdlVer;
	if (!SDL_GetWindowWMInfo(window, &winInfo)) {
		throw chainblocks::ActivationError("Failed to call SDL_GetWindowWMInfo");
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
		throw chainblocks::ActivationError("Failed to call SDL_GetWindowWMInfo");
	}
#if defined(__linux__)
	return (void *)winInfo.info.x11.display;
#else
	return nullptr;
#endif
#endif
}
