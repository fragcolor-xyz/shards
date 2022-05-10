#include "test_platform_id.hpp"
#include "gfx/context.hpp"
#include <SDL_stdinc.h>

namespace gfx {
TestPlatformId TestPlatformId::get(const Context &context) {
  TestPlatformId id;

  const char *idFromEnv = SDL_getenv("GFX_TEST_PLATFORM_ID");
  if (idFromEnv) {
    id.id = idFromEnv;
  } else {
    id.id = "default";
  }

  return id;
}

TestPlatformId::operator std::string() const { return id; }
} // namespace gfx
