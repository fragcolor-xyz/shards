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
IDrawableProcessor &MeshDrawable::getProcessor() const { return MeshDrawableProcessor::getInstance(); }

void MeshDrawable::pipelineHashCollect(PipelineHashCollector &PipelineHashCollector) const {
  PipelineHashCollector(mesh);
  if (material) {
    PipelineHashCollector(material);
  }
  for (auto &feature : features) {
    PipelineHashCollector(feature);
  }
  parameters.pipelineHashCollect(PipelineHashCollector);
}

} // namespace gfx