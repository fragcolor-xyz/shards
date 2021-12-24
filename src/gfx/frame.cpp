#include "frame.hpp"
#include "bgfx/bgfx.h"
#include "context.hpp"
#include "mesh.hpp"
#include <bgfx/bgfx.h>
#include <spdlog/spdlog.h>

namespace gfx {
void FrameRenderer::begin() {
	context.beginFrame(this);

	float time[4] = {inputs.deltaTime, inputs.time, float(inputs.frameNumber), 0.0};
	bgfx::setUniform(context.timeUniformHandle, time, 1);
}

uint32_t FrameRenderer::end(bool capture) {
	context.endFrame(this);
	return bgfx::frame(capture);
}

bgfx::ViewId FrameRenderer::nextViewId() {
	assert(_nextViewCounter < 256);
	return _nextViewCounter++;
}

bgfx::ViewId FrameRenderer::getCurrentViewId() const {
	if (_nextViewCounter == 0)
		return 0;
	return (_nextViewCounter - 1);
}

FrameRenderer *FrameRenderer::get(Context &context) { return context.getFrameRenderer(); }
} // namespace gfx
