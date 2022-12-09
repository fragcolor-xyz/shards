#include "drawables/mesh_drawable.hpp"
#include "drawables/mesh_tree_drawable.hpp"
#include "drawable_processors/mesh_drawable_processor.hpp"
#include "transform_updater.hpp"
#include <cassert>
#include <set>

namespace gfx {

UniqueId getNextDrawableId() {
  static UniqueIdGenerator gen(UniqueIdTag::Drawable);
  return gen.getNext();
}

DrawablePtr MeshTreeDrawable::clone() const {
  MeshTreeDrawable::Ptr result = std::make_shared<MeshTreeDrawable>();
  result->label = label;
  result->transform = transform;
  for (auto &drawable : drawables) {
    result->drawables.push_back(std::static_pointer_cast<MeshDrawable>(drawable->clone()));
  }
  for (auto &child : children) {
    result->children.push_back(std::static_pointer_cast<MeshTreeDrawable>(child->clone()));
  }
  result->id = getNextDrawableId();
  return result;
}

IDrawableProcessor &MeshTreeDrawable::getProcessor() const { throw std::logic_error("TODO"); }
// TODO: Processor
// void DrawQueue::add(DrawableHierarchyPtr hierarchy) {
//   TransformUpdaterCollector collector;
//   collector.collector = [&](DrawablePtr drawable) { add(drawable); };
//   collector.update(hierarchy);
// }

} // namespace gfx