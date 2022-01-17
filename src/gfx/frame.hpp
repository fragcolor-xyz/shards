#pragma once

#include <unordered_map>
#include <vector>
#include <optional>

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
	std::optional<FrameInputs> inputs;

	FrameRenderer(Context &context) : context(context) {}
	FrameRenderer(const FrameRenderer &) = delete;
	FrameRenderer &operator=(const FrameRenderer &) = delete;

	void begin(FrameInputs &&inputs = FrameInputs());
	void end();

	static FrameRenderer *get(Context &context);
};

} // namespace gfx
