#include "renderer_types.hpp"
#include "pipeline_builder.hpp"

namespace gfx::detail {

const BufferBinding& CachedPipeline::resolveBufferBindingRef(BufferBindingRef ref) const {
  if (ref.bindGroup == PipelineBuilder::getDrawBindGroupIndex())
    return drawBufferBindings[ref.bufferIndex];
  else if (ref.bindGroup == PipelineBuilder::getViewBindGroupIndex())
    return drawBufferBindings[ref.bufferIndex];
  else
    throw std::logic_error("invalid");
}
} // namespace gfx::detail