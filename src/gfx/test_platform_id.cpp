#include "test_platform_id.hpp"
#include "context.hpp"
#include <bx/platform.h>

namespace gfx {
TestPlatformId TestPlatformId::get(const RendererType &renderer) {
	TestPlatformId id;
	id.platformName = BX_PLATFORM_NAME;
	id.renderTypeName = getRendererTypeName(renderer);
	return id;
}

TestPlatformId TestPlatformId::get(const Context &context) { return get(context.getRendererType()); }

TestPlatformId::operator std::string() const { return platformName + "/" + renderTypeName; }
} // namespace gfx
