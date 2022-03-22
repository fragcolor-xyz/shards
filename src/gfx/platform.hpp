#pragma once

#if defined(_WIN32)
#define GFX_WINDOWS 1
#elif defined(__EMSCRIPTEN__)
#define GFX_EMSCRIPTEN 1
#elif defined(__APPLE__)
#define GFX_APPLE 1
#elif defined(__linux__) && !defined(__EMSCRIPTEN__)
#define GFX_LINUX 1
#else
#define GFX_UNKNOWN 1
#endif

#ifndef GFX_APPLE
#define GFX_APPLE 0
#endif

#ifndef GFX_WINDOWS
#define GFX_WINDOWS 0
#endif

#ifndef GFX_LINUX
#define GFX_LINUX 0
#endif

#ifndef GFX_EMSCRIPTEN
#define GFX_EMSCRIPTEN 0
#endif
