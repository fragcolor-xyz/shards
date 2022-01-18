#include "renderer.hpp"
#include "context.hpp"

namespace gfx {

struct Pipeline {};

Renderer::Renderer(Context &context)
	: context(context){

	  };
void Renderer::render() {}

std::shared_ptr<Pipeline> Renderer::buildPipeline() {
	// WGPURenderPipelineDescriptor desc;
	// desc.
	// wgpuDeviceCreateRenderPipeline(context.wgpuDevice, &desc);
};

} // namespace gfx
