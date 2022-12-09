#include "drawables/mesh_drawable.hpp"
#include "drawable_processors/mesh_drawable_processor.hpp"
#include "../renderer_cache.hpp"
#include "../mesh.hpp"
#include "../feature.hpp"
#include "../material.hpp"
#include <cassert>
#include <set>

namespace gfx {

DrawablePtr MeshDrawable::clone() const {
  return cloneSelfWithId(this, getNextDrawableId());
}
IDrawableProcessor &MeshDrawable::getProcessor() const { return MeshDrawableProcessor::getInstance(); }

void MeshDrawable::staticHashCollect(HashCollector &hashCollector) const {
  hashCollector(mesh->getId());
  hashCollector.addReference(mesh);
  if (material) {
    hashCollector(material->getId());
    hashCollector.addReference(material);
  }
  for (auto &feature : features) {
    hashCollector(feature->getId());
    hashCollector.addReference(feature);
  }
  parameters.staticHashCollect(hashCollector);
  hashCollector(transform);
}

} // namespace gfx