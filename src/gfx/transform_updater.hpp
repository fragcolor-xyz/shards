#ifndef E726D173_792B_489C_AD25_455528CCF0D0
#define E726D173_792B_489C_AD25_455528CCF0D0

#include "drawable.hpp"
#include <cassert>
#include <set>

#ifndef NDEBUG
#define GFX_TRANSFORM_UPDATER_TRACK_VISITED 1
#endif

namespace gfx {
// Updates a transform hierarchy while collecting drawables
struct TransformUpdaterCollector {
  struct Node {
    float4x4 parentTransform;
    DrawableHierarchy *node;
  };
  std::vector<Node> queue;

#if GFX_TRANSFORM_UPDATER_TRACK_VISITED
  std::set<DrawableHierarchy *> visited;
#endif

  std::function<void(DrawablePtr)> collector = [](DrawablePtr) {};

  void update(DrawableHierarchyPtr root) {
    queue.push_back(Node{float4x4(linalg::identity), root.get()});

    while (!queue.empty()) {
      Node node = queue.back();
      queue.pop_back();

#if GFX_TRANSFORM_UPDATER_TRACK_VISITED
      assert(!visited.contains(node.node));
      visited.insert(node.node);
#endif

      assert(node.node->transform != float4x4());
      float4x4 resolvedTransform = linalg::mul(node.parentTransform, node.node->transform);
      for (auto &drawable : node.node->drawables) {
        drawable->transform = resolvedTransform;
        collector(drawable);
      }

      for (auto &child : node.node->children) {
        queue.push_back(Node{resolvedTransform, child.get()});
      }
    }
  }
};
} // namespace gfx

#endif /* E726D173_792B_489C_AD25_455528CCF0D0 */
