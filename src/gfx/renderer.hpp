#pragma once
#include <memory>

namespace gfx {

struct Context;
struct Pipeline;
struct DrawQueue;

struct Drawable;
typedef std::shared_ptr<Drawable> DrawablePtr;

struct View;
typedef std::shared_ptr<View> ViewPtr;

struct RendererImpl;

// Instance that caches render pipelines
struct Renderer {
	std::shared_ptr<RendererImpl> impl;

public:
	Renderer(Context &context);
	void swapBuffers();
	void render(const DrawQueue &drawQueue, ViewPtr view);
};

} // namespace gfx
