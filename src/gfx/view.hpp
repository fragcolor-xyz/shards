#pragma once

#include "bgfx/bgfx.h"
#include "opt.hpp"
#include "texture.hpp"
#include "types.hpp"
#include <bgfx/bgfx.h>
#include <optional>
#include <variant>

#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif

namespace gfx {

enum class FovDirection {
	Horizontal,
	Vertical,
};

struct ViewPerspectiveProjection {
	float fov;
	FovDirection fovType = FovDirection::Vertical;
	float near = 0.1f;
	float far = 800.0f;
};

enum class OrthographicSizeType {
	Horizontal,
	Vertical,
	PixelScale,
};
struct ViewOrthographicProjection {
	float size;
	OrthographicSizeType sizeType = OrthographicSizeType::Horizontal;
	float near = 0.0f;
	float far = 1000.0f;
};

struct alignas(16) View {
public:
	float4x4 view;
	std::optional<Rect> viewport;
	std::variant<std::monostate, ViewPerspectiveProjection, ViewOrthographicProjection, float4x4> proj;

public:
	View();
	View(const View &) = delete;

	float4x4 getProjectionMatrix(const int2 &viewSize) const;
};
typedef std::shared_ptr<View> ViewPtr;

} // namespace gfx
