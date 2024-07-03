#ifndef AF98C269_8338_4ED8_BC88_0EBCE458001D
#define AF98C269_8338_4ED8_BC88_0EBCE458001D

#if SHARDS_GFX_SDL
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL.h>
#else
// Even though our emscripten implementation doesn't use the actual library
// we still use the structures and enums from SDL for convenience
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_touch.h>
#include <SDL3/SDL_rect.h>
#endif

#endif /* AF98C269_8338_4ED8_BC88_0EBCE458001D */
