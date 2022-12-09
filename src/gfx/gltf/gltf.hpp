#pragma once

#include <gfx/drawables/mesh_tree_drawable.hpp>

namespace gfx {

MeshTreeDrawable::Ptr loadGltfFromFile(const char *file);
MeshTreeDrawable::Ptr loadGltfFromMemory(const uint8_t *data, size_t dataLength);

} // namespace gfx
