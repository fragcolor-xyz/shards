#ifndef GFX_TESTS_PLATFORM_ID
#define GFX_TESTS_PLATFORM_ID

#include "gfx/context.hpp"
#include <string>

namespace gfx {
struct Context;
struct TestPlatformId {
  std::string id;

  operator std::string() const;

  static TestPlatformId get(const Context &context);
  static const TestPlatformId Default;
};
} // namespace gfx

#endif // GFX_TESTS_PLATFORM_ID
