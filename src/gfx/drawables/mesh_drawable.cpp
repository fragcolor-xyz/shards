#include "drawables/mesh_drawable.hpp"
#include "drawable_processors/mesh_drawable_processor.hpp"
#include "../renderer_cache.hpp"
#include "../mesh.hpp"
#include "../feature.hpp"
#include "../material.hpp"
#include <cassert>
#include <set>

namespace gfx {

DrawablePtr MeshDrawable::clone() const { return cloneSelfWithId(this, getNextDrawableId()); }
DrawableProcessorConstructor MeshDrawable::getProcessor() const {
  static DrawableProcessorConstructor fn = [](Context &context) -> detail::DrawableProcessorPtr {
    return std::make_shared<detail::MeshDrawableProcessor>(context);
  };
  return fn;
}

} // namespace gfx
