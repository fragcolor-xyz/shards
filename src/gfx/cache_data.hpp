#pragma once
#include <vector>

namespace gfx {

struct CachedDataHost {};

struct CachedDataOwner {};

struct CachedDataContainer {
	std::vector<void *> data;
};

} // namespace gfx
