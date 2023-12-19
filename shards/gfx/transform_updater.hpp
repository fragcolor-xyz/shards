#ifndef E726D173_792B_489C_AD25_455528CCF0D0
#define E726D173_792B_489C_AD25_455528CCF0D0

#include "drawables/mesh_tree_drawable.hpp"
#include "../core/function.hpp"
#include <cassert>
#include <set>

#ifndef NDEBUG
#define GFX_TRANSFORM_UPDATER_TRACK_VISITED 1
#endif

namespace gfx {

// Updates a MeshTreeDrawable and all it's children while collecting drawables
struct TransformUpdaterCollector {
  struct Node {
    float4x4 parentTransform;
    MeshTreeDrawable *node;
  };
  std::vector<Node> queue;

#if GFX_TRANSFORM_UPDATER_TRACK_VISITED
  std::set<MeshTreeDrawable *> visited;
#endif

  shards::Function<void(const DrawablePtr &)> collector = [](const DrawablePtr &) {};

  void update(MeshTreeDrawable &root) {
    queue.push_back(Node{float4x4(linalg::identity), &root});

    while (!queue.empty()) {
      Node node = queue.back();
      queue.pop_back();

#if GFX_TRANSFORM_UPDATER_TRACK_VISITED
      shassert(!visited.contains(node.node));
      visited.insert(node.node);
#endif

      auto mat = node.node->trs.getMatrix();
      shassert(mat != float4x4());
      node.node->resolvedTransform = linalg::mul(node.parentTransform, mat);
      for (auto &drawable : node.node->drawables) {
        drawable->transform = node.node->resolvedTransform;
        collector(drawable);
      }

      for (auto &child : node.node->getChildren()) {
        queue.push_back(Node{node.node->resolvedTransform, child.get()});
      }
    }
  }
};
} // namespace gfx

#endif /* E726D173_792B_489C_AD25_455528CCF0D0 */
