#include "test_platform_id.hpp"
#include "gfx/context.hpp"

namespace gfx {
TestPlatformId TestPlatformId::get(const Context &context) {
  TestPlatformId id;
  return id;
}

TestPlatformId::operator std::string() const { return "wgpu"; }
} // namespace gfx
