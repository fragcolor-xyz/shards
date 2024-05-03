#include "../core/platform.hpp"
#if !SH_EMSCRIPTEN
#include <SDL_events.h>
#include <SDL_video.h>
#endif

#include <memory>
#include <type_traits>
#include <variant>

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "gfx_wgpu.hpp"
