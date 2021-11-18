#pragma once

#include "bgfx/bgfx.h"
#include "opt.hpp"
#include "types.hpp"
#include <bgfx/bgfx.h>

namespace gfx {

struct alignas(16) View {
	const bgfx::ViewId id{0};

  private:
	Rect viewport;
	float4x4 view;
	float4x4 proj;

  public:
	View(bgfx::ViewId id) : id(id) {}
	View(const View &) = delete;

	void setViewport(const Rect &viewport) {
		this->viewport = viewport;
		bgfx::setViewRect(id, viewport.x, viewport.y, viewport.width,
						  viewport.height);
	}
	const Rect &getViewport() const { return viewport; }
	const int2 getSize() const { return int2(viewport.width, viewport.height); }

	void setViewTransform(const float4x4 &view, const float4x4 &proj) {
		this->view = view;
		this->proj = proj;

		bgfx::Transform transform;
		bgfx::allocTransform(&transform, 2);
		float *viewPtr = transform.data;
		float *projPtr = transform.data + 16;
		packFloat4x4(view, viewPtr);
		packFloat4x4(proj, projPtr);

		bgfx::setViewTransform(id, viewPtr, projPtr);
	}
	const float4x4 &getViewMatrix() const { return view; }
	const float4x4 &getProjMatrix() const { return proj; }
};
} // namespace gfx
