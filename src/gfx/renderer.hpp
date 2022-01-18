#pragma once
#include <memory>

namespace gfx {

// Instance that caches render pipelines
struct Context;
struct Pipeline;
struct Renderer {
	Context &context;

	Renderer(Context &context) : context(context) {};
	void render();

private:
	std::shared_ptr<Pipeline> buildPipeline();
};

} // namespace gfx
