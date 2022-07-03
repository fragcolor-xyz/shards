#ifndef GFX_WIRE_MESH
#define GFX_WIRE_MESH

#include "fwd.hpp"
#include <map>

namespace gfx {
struct WireframeMeshGenerator {
  MeshPtr mesh;

  WireframeMeshGenerator() {}
  MeshPtr generate();
};

struct WireframeRenderer {
private:
  FeaturePtr wireframeFeature;

  struct MeshCacheEntry {
    MeshPtr wireMesh;
    std::weak_ptr<Mesh> weakMesh;
  };
  std::map<Mesh *, MeshCacheEntry> meshCache;

public:
  WireframeRenderer(bool showBackfaces = false);
  void overlayWireframe(DrawQueue& queue, DrawablePtr drawable);
  void overlayWireframe(DrawQueue& queue, DrawableHierarchyPtr drawableHierarchy);
};
} // namespace gfx

#endif // GFX_WIRE_MESH
