#include "material.hpp"
#include "drawables/mesh_drawable.hpp"
#include "feature.hpp"
#include "mesh.hpp"

// This file groups all the pipeline hash functions so that they can be compiled -O3 even in debug

namespace gfx {
void MaterialParameters::pipelineHashCollect(PipelineHashCollector &PipelineHashCollector) const {
  for (auto &pair : texture) {
    PipelineHashCollector(pair.first);
    if (pair.second.texture) {
      PipelineHashCollector(1u);
      PipelineHashCollector(pair.second.defaultTexcoordBinding);
    } else {
      PipelineHashCollector(0u);
    }
  }
}

void Material::pipelineHashCollect(PipelineHashCollector &PipelineHashCollector) const {
  for (auto &feature : features) {
    PipelineHashCollector(feature);
  }
  parameters.pipelineHashCollect(PipelineHashCollector);
}

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

void Feature::pipelineHashCollect(PipelineHashCollector &collector) const { collector(id); }

void Mesh::pipelineHashCollect(struct PipelineHashCollector &collector) const { collector(format); }

} // namespace gfx