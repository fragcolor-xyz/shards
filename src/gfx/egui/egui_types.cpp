#include "rust_interop.hpp"
#include <gfx/mesh.hpp>
using namespace gfx;

namespace egui {
std::vector<MeshVertexAttribute> Vertex::getAttributes() {
  return std::vector<MeshVertexAttribute>{
      MeshVertexAttribute("position", 2),
      MeshVertexAttribute("texCoord0", 2),
      MeshVertexAttribute("color", 4, VertexAttributeType::UNorm8),
  };
}
} // namespace egui