#include "test_platform_id.hpp"
#include "context.hpp"

namespace gfx {
TestPlatformId TestPlatformId::get(const Context &context) {
  TestPlatformId id;
  id.platformName = "platform"; // TODO
  id.renderTypeName = "wgpu";   // TODO
  return id;
}

TestPlatformId::operator std::string() const { return platformName + "/" + renderTypeName; }
} // namespace gfx
