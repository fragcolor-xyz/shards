#pragma once

#include "linalg.hpp"
#include "types.hpp"

namespace gfx {

struct Light {
	virtual ~Light() = default;
};

struct PointLight : public Light {
	Colorf color = Colorf(1.0f);
	float intensity = 1.0f;
	float3 position;
	float innerRadius = 0.5f;
	float outerRadius = 10.0f;
};

struct DirectionalLight : public Light {
	Colorf color = Colorf(1.0f);
	float intensity = 1.0f;
	float3 direction = float3(0, 0, 1);
};

} // namespace gfx