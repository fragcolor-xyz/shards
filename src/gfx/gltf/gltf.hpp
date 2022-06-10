#pragma once

#include <gfx/drawable.hpp>

namespace gfx {

DrawableHierarchyPtr loadGltfFromFile(const char *file);
DrawableHierarchyPtr loadGltfFromMemory(const uint8_t* data, size_t dataLength);

} // namespace gfx
