#version 460
#section vert
#include "v_fullscreen.glsl"

#section frag
layout(location = 0) out vec4 color;
layout(location = 1) in vec2 f_uv;

#include "lib/const.glsl"

uniform sampler2D u_inputTexture;

#define MONTECARLO_NUM_SAMPLES 512
#define MONTECARLO_FUNCTION ggx
#include "lib/montecarlo.glsl"
#include "lib/lighting.glsl"

layout(std140) uniform GGXParameters {
	float roughness;
}
ggxParameters;

IntegrateOutput ggx(IntegrateInput mci) {
	IntegrateOutput result;

	ImportanceSample lvs = importanceSampleGGX(mci.coord, ggxParameters.roughness);
	result.localDirection = lvs.localDirection;
	result.pdf = lvs.pdf;

	// Use N = V for precomputed map
	vec3 localViewDir = vec3(0, 0, 1);
	vec3 localLightDir = getReflectionVector(localViewDir, result.localDirection);
	float nDotL = dot(localLightDir, result.localDirection);

	result.sampleWeight = nDotL;
	result.sampleScale = nDotL;

	return result;
}

#include "lib/montecarlo_impl.glsl"

void main() {
	color = vec4(montecarlo(f_uv, u_inputTexture), 1);
}