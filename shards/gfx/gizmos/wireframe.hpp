#ifndef F0B0C3A4_7AFF_4365_AF86_57E4D7CF0623
#define F0B0C3A4_7AFF_4365_AF86_57E4D7CF0623

#include "../fwd.hpp"
#include "../../core/pool.hpp"
#include "../linalg.hpp"
#include <map>

namespace gfx {
struct MeshDrawable;
namespace detail {
struct WorkerMemory;
}
struct WireframeMeshGenerator {
  MeshPtr mesh;

  WireframeMeshGenerator() {}
  MeshPtr generate();
};

struct WireFrameDrawablePoolTraits {
  using T = std::shared_ptr<MeshDrawable>;
  T newItem();
  void release(T &) {}
  bool canRecycle(T &v) { return true; }
  void recycled(T &v) {}
};

struct WireframeRenderer {
private:
  FeaturePtr wireframeFeature;

  std::shared_ptr<detail::WorkerMemory> allocator;

  struct MeshCacheEntry {
    size_t lastTouched{};
    MeshPtr wireMesh;
    std::weak_ptr<Mesh> weakMesh;
    shards::Pool<std::shared_ptr<MeshDrawable>, WireFrameDrawablePoolTraits> drawablePool;
  };
  std::map<Mesh *, MeshCacheEntry> meshCache;
  size_t currentFrameCounter{};

public:
  WireframeRenderer(bool showBackfaces = false);
  void reset(size_t frameCounter);
  void overlayWireframe(DrawQueue &queue, IDrawable &drawable, float4 color);
  std::shared_ptr<MeshDrawable> getWireframeDrawable(const MeshPtr &mesh, float4 color);
};
} // namespace gfx

#endif /* F0B0C3A4_7AFF_4365_AF86_57E4D7CF0623 */
