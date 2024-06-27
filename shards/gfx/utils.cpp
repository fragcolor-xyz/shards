#include "utils.hpp"
#if SHARDS_GFX_SDL
#include <SDL3/SDL_stdinc.h>
#endif

namespace gfx {

bool getEnvFlag(const char *name) {
#if SHARDS_GFX_SDL
  const char *value = SDL_getenv(name);
  if (!value)
    return false;

  int intValue = std::atoi(value);
  return intValue != 0;
#else
  return false;
#endif
}

} // namespace gfx
