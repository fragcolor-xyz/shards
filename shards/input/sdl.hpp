#ifndef AF98C269_8338_4ED8_BC88_0EBCE458001D
#define AF98C269_8338_4ED8_BC88_0EBCE458001D

#if SHARDS_GFX_SDL
#include <SDL_keycode.h>
#include <SDL.h>
#else
// Even though our emscripten implementation doesn't use the actual library
// we still use the structures and enums from SDL for convenience
#include <SDL/include/SDL_keycode.h>
#include <SDL/include/SDL_events.h>
#include <SDL/include/SDL_mouse.h>
#include <SDL/include/SDL_touch.h>
#include <SDL/include/SDL_rect.h>
#endif

#endif /* AF98C269_8338_4ED8_BC88_0EBCE458001D */
