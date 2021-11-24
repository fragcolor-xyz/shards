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
	assert(views.size() == 0);
	context.endFrame(this);
	return bgfx::frame(capture);
}

uint32_t FrameRenderer::addFrameDrawable(IDrawable *drawable) {
	// 0 idx = empty
	uint32_t id = ++_frameDrawablesCount;
	_frameDrawables[id] = drawable;
	return id;
}

IDrawable *FrameRenderer::getFrameDrawable(uint32_t id) {
	if (id == 0) {
		return nullptr;
	}
	return _frameDrawables[id];
}

View &FrameRenderer::getCurrentView() {
	assert(views.size() > 0);
	return *views.back().get();
}

View &FrameRenderer::pushView() {
	View &view = *views.emplace_back(std::make_shared<View>(nextViewId())).get();
	bgfx::resetView(view.id);
	bgfx::touch(view.id);

	return view;
}

View &FrameRenderer::pushMainOutputView() {
	int2 mainOutputSize = context.getMainOutputSize();
	View &view = pushView();

	Rect viewport(mainOutputSize.x, mainOutputSize.y);
	view.setViewport(viewport);

	return view;
}

void FrameRenderer::popView() {
	assert(views.size() > 0);
	views.pop_back();
}

bgfx::ViewId FrameRenderer::nextViewId() {
	assert(_nextViewCounter < UINT16_MAX);
	return _nextViewCounter++;
}

FrameRenderer *FrameRenderer::get(Context &context) { return context.getFrameRenderer(); }
} // namespace gfx
