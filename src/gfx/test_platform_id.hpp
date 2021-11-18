#pragma once
#include "gfx/context.hpp"
#include <string>

namespace gfx {
struct Context;
enum class RendererType : uint8_t;
struct TestPlatformId {
	std::string platformName;
	std::string renderTypeName;

	static TestPlatformId get(const RendererType &renderer);
	static TestPlatformId get(const Context& context);
	operator std::string() const;
};
} // namespace gfx
