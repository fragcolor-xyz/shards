#version 460
#section vert
#include "v_fullscreen.glsl"

#section frag
layout(location = 0) out vec4 color;
layout(location = 1) in vec2 f_uv;

#include "lib/const.glsl"

uniform sampler2D u_inputTexture;

#define MONTECARLO_NUM_SAMPLES 512
#define MONTECARLO_FUNCTION lambert
#include "lib/montecarlo.glsl"
#include "lib/lighting.glsl"

IntegrateOutput lambert(IntegrateInput mci) {
	IntegrateOutput result;
	ImportanceSample lvs = importanceSampleLambert(mci.coord);
	result.localDirection = lvs.localDirection;
	result.pdf = lvs.pdf;
	result.sampleWeight = 1.0;
	result.sampleScale = 1.0;
	return result;
}

#include "lib/montecarlo_impl.glsl"

void main() {
	color = vec4(montecarlo(f_uv, u_inputTexture) * EXPOSURE, 1);
}