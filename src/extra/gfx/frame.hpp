#pragma once

#include "context.hpp"
#include "view.hpp"
#include <boost/align/aligned_allocator.hpp>

struct CBContext;
namespace gfx {

struct FrameInputs {
	float deltaTime;
	float time;
	int frameNumber;
	const std::vector<SDL_Event>& events;

	FrameInputs(float deltaTime, float time, int frameNumber, const std::vector<SDL_Event>& events) :
		deltaTime(deltaTime), time(time), frameNumber(frameNumber), events(events) {}
};

// Persistent state over the duration of an single update/draw loop iteration
struct Context;
struct FrameRenderer {
	Context& context;
	CBContext* cbContext;
	FrameInputs inputs;

	std::vector<SDL_Event> events;

	std::deque<ViewInfo, boost::alignment::aligned_allocator<ViewInfo>> _viewsStack;
	bgfx::ViewId _nextViewCounter{1};

	uint32_t _frameDrawablesCount{0};
	std::unordered_map<uint32_t, IDrawable*> _frameDrawables;

	bool _picking{false};

	FrameRenderer(Context& context, FrameInputs&& inputs) : context(context), inputs(inputs) {}
	FrameRenderer(const FrameRenderer&) = delete;
	FrameRenderer& operator=(const FrameRenderer&) = delete;

	void begin(CBContext& cbContext);
	void end(CBContext& cbContext);

	constexpr bool isPicking() const { return _picking; }
	void setPicking(bool picking) { _picking = picking; }

	uint32_t addFrameDrawable(IDrawable* drawable);
	IDrawable* getFrameDrawable(uint32_t id);

	ViewInfo& getCurrentView();
	ViewInfo& pushView(ViewInfo view);
	ViewInfo& pushMainView();
	void popView();
	size_t viewIndex() const { return _viewsStack.size(); }
	bgfx::ViewId nextViewId();

	void draw(const Primitive& primitive);

	static FrameRenderer* get(Context& context);
};
} // namespace gfx
