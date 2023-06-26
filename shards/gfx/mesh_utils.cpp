#include "mesh_utils.hpp"
#include <boost/container/small_vector.hpp>

using namespace boost::container;

namespace gfx {
MeshPtr generateTangentInfo(MeshPtr mesh) {
  auto &format = mesh->getFormat();

  small_vector<size_t, 16> srcAttribs;
  small_vector<size_t, 16> dstAttribs;

  size_t i{};
  std::optional<size_t> tangentIndex;
  auto filterAttrib = [&](const MeshVertexAttribute &attrib) {
    if (attrib.name == "tangent") {
      tangentIndex = i;
      return false;
    }
    if (attrib.name == "binormal" || attrib.name == "bitangent") {
      return false;
    }
    return true;
  };

  for (auto &attrib : format.vertexAttributes) {
    if (filterAttrib(attrib)) {
    }
  }
}
} // namespace gfx