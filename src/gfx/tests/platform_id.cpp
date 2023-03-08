#include "./platform_id.hpp"
#include "gfx/context.hpp"
#include <SDL3/SDL_stdinc.h>

namespace gfx {
TestPlatformId::operator std::string() const { return id; }

TestPlatformId TestPlatformId::get(const Context &context) {
  TestPlatformId id;

  const char *idFromEnv = SDL_getenv("GFX_TEST_PLATFORM_ID");
  if (idFromEnv) {
    id.id = idFromEnv;
  } else {
    return TestPlatformId::Default;
  }

  return id;
}

const TestPlatformId TestPlatformId::Default = TestPlatformId{
    .id = "default",
};
} // namespace gfx
