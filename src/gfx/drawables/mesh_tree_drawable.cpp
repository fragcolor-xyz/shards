#include "mesh_tree_drawable.hpp"
#include "transform_updater.hpp"
#include <stdexcept>

namespace gfx {

DrawablePtr MeshTreeDrawable::clone() const {
  MeshTreeDrawable::Ptr result = std::make_shared<MeshTreeDrawable>();
  result->label = label;
  result->trs = trs;
  for (auto &drawable : drawables) {
    result->drawables.push_back(std::static_pointer_cast<MeshDrawable>(drawable->clone()));
  }
  for (auto &child : children) {
    result->addChild(std::static_pointer_cast<MeshTreeDrawable>(child->clone()));
  }
  result->id = getNextDrawableId();
  return result;
}

DrawableProcessorConstructor MeshTreeDrawable::getProcessor() const { throw std::logic_error("not implemented"); }

bool MeshTreeDrawable::expand(shards::pmr::vector<const IDrawable *> &outDrawables) const {
  TransformUpdaterCollector collector;
  collector.collector = [&](const DrawablePtr &drawable) { outDrawables.push_back(drawable.get()); };
  collector.update(const_cast<MeshTreeDrawable &>(*this));
  return true;
}

} // namespace gfx
