#pragma once

#include "context.hpp"
#include "view.hpp"

namespace gfx {

struct FrameInputs {
	float deltaTime;
	float time;
	int frameNumber;
	const std::vector<SDL_Event> &events;

	FrameInputs(float deltaTime = 0.1f, float time = 0.0f, int frameNumber = 0,
				const std::vector<SDL_Event> &events = std::vector<SDL_Event>())
		: deltaTime(deltaTime), time(time), frameNumber(frameNumber),
		  events(events) {}
};

// Persistent state over the duration of an single update/draw loop iteration
struct Context;
struct FrameRenderer {
	Context &context;
	FrameInputs inputs;

	std::vector<SDL_Event> events;

	std::deque<std::shared_ptr<View>> views;
	bgfx::ViewId _nextViewCounter{0};

	uint32_t _frameDrawablesCount{0};
	std::unordered_map<uint32_t, IDrawable *> _frameDrawables;

	bool _picking{false};

	FrameRenderer(Context &context, FrameInputs &&inputs = FrameInputs())
		: context(context), inputs(inputs) {}
	FrameRenderer(const FrameRenderer &) = delete;
	FrameRenderer &operator=(const FrameRenderer &) = delete;

	void begin();
	void end(bool capture = false);

	constexpr bool isPicking() const { return _picking; }
	void setPicking(bool picking) { _picking = picking; }

	uint32_t addFrameDrawable(IDrawable *drawable);
	IDrawable *getFrameDrawable(uint32_t id);

	View &getCurrentView();
	View &pushView();
	View &pushMainOutputView();
	void popView();
	size_t viewIndex() const { return views.size(); }
	bgfx::ViewId nextViewId();

	void draw(const Primitive &primitive);

	static FrameRenderer *get(Context &context);
};
} // namespace gfx
