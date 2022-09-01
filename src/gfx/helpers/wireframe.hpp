#ifndef F0B0C3A4_7AFF_4365_AF86_57E4D7CF0623
#define F0B0C3A4_7AFF_4365_AF86_57E4D7CF0623

#include "../fwd.hpp"
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

#endif /* F0B0C3A4_7AFF_4365_AF86_57E4D7CF0623 */
