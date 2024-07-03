#include "../core/platform.hpp"
#if !SH_EMSCRIPTEN
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>
#endif

#include <memory>
#include <type_traits>
#include <variant>

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "gfx_wgpu.hpp"
