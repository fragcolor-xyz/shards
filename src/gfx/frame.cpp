#include "frame.hpp"

#include "context.hpp"
#include "mesh.hpp"

#include <spdlog/spdlog.h>

namespace gfx {
void FrameRenderer::begin(FrameInputs &&inputs) {
  this->inputs.emplace(inputs);

  context.beginFrame(this);

  float time[4] = {inputs.deltaTime, inputs.time, float(inputs.frameNumber), 0.0};
  // TODO: Set time uniform
}

void FrameRenderer::end() {
  context.endFrame(this);
  wgpuSwapChainPresent(context.wgpuSwapchain);
}

FrameRenderer *FrameRenderer::get(Context &context) { return context.getFrameRenderer(); }
} // namespace gfx
