#pragma once

#include <bgfx/bgfx.h>
#include "linalg_shim.hpp"

namespace gfx {
using chainblocks::Mat4;

struct Rect {
	int x{0};
	int y{0};
	int width{0};
	int height{0};

	Rect() = default;
	Rect(int width, int height) : width(width), height(height) {}
	Rect(int x, int y, int width, int height) : x(x), y(y), width(width), height(height) {}

	int getX1() const { return x + width; }
	int getY1() const { return x + height; }

	static Rect fromCorners(int x0, int y0, int x1, int y1) { return Rect(x0, y0, x1 - x0, y1 - y0); }
};

struct alignas(16) ViewInfo {
	bgfx::ViewId id{0};
	bgfx::FrameBufferHandle fb = BGFX_INVALID_HANDLE;

	Rect viewport;

	Mat4 view;
	Mat4 proj;

	ViewInfo() = default;
	ViewInfo(bgfx::ViewId id, const Rect &viewport, bgfx::FrameBufferHandle fb = BGFX_INVALID_HANDLE) : id(id), fb(fb), viewport(viewport) {}

	const Mat4 &invView() const {
		if (unlikely(_invView.x._private[0] == 0)) {
			_invView = linalg::inverse(view);
			_invView.x._private[0] = 1;
		}
		return _invView;
	}

	const Mat4 &invProj() const {
		if (unlikely(_invProj.x._private[0] == 0)) {
			_invProj = linalg::inverse(proj);
			_invProj.x._private[0] = 1;
		}
		return _invProj;
	}

	const Mat4 &viewProj() const {
		if (unlikely(_viewProj.x._private[0] == 0)) {
			_viewProj = linalg::mul(view, proj);
			_viewProj.x._private[0] = 1;
		}
		return _viewProj;
	}

	const Mat4 &invViewProj() const {
		if (unlikely(_invViewProj.x._private[0] == 0)) {
			_invViewProj = linalg::inverse(viewProj());
			_invViewProj.x._private[0] = 1;
		}
		return _invViewProj;
	}

	void invalidate() {
		_invView.x._private[0] = 0;
		_invProj.x._private[0] = 0;
		_viewProj.x._private[0] = 0;
		_invViewProj.x._private[0] = 0;
	}

	mutable Mat4 _invView{};
	mutable Mat4 _invProj{};
	mutable Mat4 _viewProj{};
	mutable Mat4 _invViewProj{};
};
} // namespace gfx
