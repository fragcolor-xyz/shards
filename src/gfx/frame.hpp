#pragma once

#include <bgfx/bgfx.h>
#include <unordered_map>
#include <vector>

namespace gfx {

struct FrameInputs {
	float deltaTime;
	float time;
	int frameNumber;
	const std::vector<SDL_Event> &events;

	FrameInputs(float deltaTime = 0.1f, float time = 0.0f, int frameNumber = 0, const std::vector<SDL_Event> &events = std::vector<SDL_Event>())
		: deltaTime(deltaTime), time(time), frameNumber(frameNumber), events(events) {}
};

// Persistent state over the duration of an single update/draw loop iteration
struct Context;
struct IDrawable;
struct FrameRenderer {
	Context &context;
	FrameInputs inputs;

	bgfx::ViewId _nextViewCounter{0};

	uint32_t _frameDrawablesCount{0};
	std::unordered_map<uint32_t, IDrawable *> _frameDrawables;

	bool _picking{false};

	FrameRenderer(Context &context, FrameInputs &&inputs = FrameInputs()) : context(context), inputs(inputs) {}
	FrameRenderer(const FrameRenderer &) = delete;
	FrameRenderer &operator=(const FrameRenderer &) = delete;

	void begin();
	uint32_t end(bool capture = false);

	constexpr bool isPicking() const { return _picking; }
	void setPicking(bool picking) { _picking = picking; }

	bgfx::ViewId nextViewId();
	bgfx::ViewId getCurrentViewId() const;

	static FrameRenderer *get(Context &context);
};

} // namespace gfx
