#ifndef E726D173_792B_489C_AD25_455528CCF0D0
#define E726D173_792B_489C_AD25_455528CCF0D0

#include "drawables/mesh_tree_drawable.hpp"
#include "../core/function.hpp"
#include <cassert>
#include <set>
#include <boost/container/small_vector.hpp>

#ifndef NDEBUG
#define GFX_TRANSFORM_UPDATER_TRACK_VISITED 1
#endif

namespace gfx {

// Updates a MeshTreeDrawable and all it's children while collecting drawables
struct TransformUpdaterCollector {
  struct Node {
    float4x4 parentTransform;
    MeshTreeDrawable *node;
    bool updated;
  };
  boost::container::small_vector<Node, 32> queue;

#if GFX_TRANSFORM_UPDATER_TRACK_VISITED
  std::set<MeshTreeDrawable *> visited;
#endif

  shards::Function<void(const DrawablePtr &)> collector = [](const DrawablePtr &) {};

  void update(MeshTreeDrawable &root) {
    queue.push_back(Node{float4x4(linalg::identity), &root});

    while (!queue.empty()) {
      Node node = queue.back();
      queue.pop_back();

      bool nodeUpdated = {};
      node.updated = true;
      if (node.node->version != node.node->lastUpdateVersion) {
        node.node->lastUpdateVersion = node.node->version;
      }

#if GFX_TRANSFORM_UPDATER_TRACK_VISITED
      shassert(!visited.contains(node.node));
      visited.insert(node.node);
#endif

      if (node.updated) {
        auto mat = node.node->trs.getMatrix();
        shassert(mat != float4x4());
        node.node->resolvedTransform = linalg::mul(node.parentTransform, mat);
      }

      for (auto &drawable : node.node->drawables) {
        if (node.updated) {
          drawable->transform = node.node->resolvedTransform;
          drawable->update();
        }
        collector(drawable);
      }

      for (auto &child : node.node->getChildren()) {
        queue.push_back(Node{
            node.node->resolvedTransform,
            child.get(),
            nodeUpdated,
        });
      }
    }
  }
};
} // namespace gfx

#endif /* E726D173_792B_489C_AD25_455528CCF0D0 */
