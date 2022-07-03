#include "drawable.hpp"
#include "transform_updater.hpp"
#include <cassert>
#include <set>

namespace gfx {

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