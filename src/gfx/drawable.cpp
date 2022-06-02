#include "drawable.hpp"
#include <cassert>
#include <set>

namespace gfx {

#ifndef NDEBUG
#define GFX_TRANSFORM_UPDATER_TRACK_VISITED 1
#endif

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

DrawablePtr Drawable::clone() const {
  DrawablePtr result = std::make_shared<Drawable>(*this);
  return result;
}

DrawableHierarchyPtr DrawableHierarchy::clone() const {
  DrawableHierarchyPtr result = std::make_shared<DrawableHierarchy>();
  result->label = label;
  result->transform = transform;
  for (auto &drawable : drawables) {
    result->drawables.push_back(drawable->clone());
  }
  for (auto &child : children) {
    result->children.push_back(child->clone());
  }
  return result;
}

void DrawQueue::add(DrawableHierarchyPtr hierarchy) {
  TransformUpdaterCollector collector;
  collector.collector = [&](DrawablePtr drawable) { add(drawable); };
  collector.update(hierarchy);
}

} // namespace gfx