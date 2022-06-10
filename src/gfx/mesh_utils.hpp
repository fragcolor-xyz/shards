#ifndef DC6B12F5_63B6_4FBD_BC92_DFF99E0E3323
#define DC6B12F5_63B6_4FBD_BC92_DFF99E0E3323

#include "fwd.hpp"
#include "mesh.hpp"
#include <vector>

namespace gfx {
template <typename TVert, typename TIndex>
MeshPtr createMesh(const std::vector<TVert> &verts, const std::vector<TIndex> &indices) {
  static_assert(sizeof(TIndex) == 2 || sizeof(TIndex) == 4, "Invalid index format");

  MeshPtr mesh = std::make_shared<Mesh>();
  MeshFormat format{
      .indexFormat = (sizeof(TIndex) == 2) ? IndexFormat::UInt16 : IndexFormat::UInt32,
      .vertexAttributes = TVert::getAttributes(),
  };

  mesh->update(format, verts.data(), verts.size() * sizeof(TVert), indices.data(), indices.size() * sizeof(TIndex));
  return mesh;
}
} // namespace gfx

#endif /* DC6B12F5_63B6_4FBD_BC92_DFF99E0E3323 */
