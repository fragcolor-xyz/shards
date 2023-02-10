#version 460
#section vert
#include "v_fullscreen.glsl"

#section frag
layout(location = 0) out vec4 color;
layout(location = 1) in vec2 f_uv;

#include "lib/const.glsl"

#define MONTECARLO_NUM_SAMPLES (1024*2)
#include "lib/lighting.glsl"
#include "lib/hammersley2d.glsl"

void main() {
	float roughness = f_uv.x;
	float nDotV = f_uv.y;

	vec3 localViewDir;
	localViewDir.x = sqrt(1.0 - nDotV * nDotV); // = sin(acos(nDotV));
	localViewDir.y = 0;
	localViewDir.z = nDotV;

	vec2 result = vec2(0, 0);
	for (int sampleIndex = 0; sampleIndex < MONTECARLO_NUM_SAMPLES; sampleIndex++) {
		vec2 coord = hammersley2d(sampleIndex, MONTECARLO_NUM_SAMPLES);

		ImportanceSample lvs = importanceSampleGGX(coord, roughness);
        vec3 sampleNormal = lvs.localDirection;
		vec3 localLightDir = getReflectionVector(localViewDir, sampleNormal);

		float nDotL = localLightDir.z;
		float nDotH = sampleNormal.z;
		float vDotH = dot(localViewDir, sampleNormal);
        
        if(nDotL > 0) {
            float pdf = visibilityGGX(nDotL, nDotV, roughness) * vDotH * nDotL / nDotH;
            float fc = pow(1.0 - vDotH, 5.0);
            result.x += (1.0 - fc) * pdf;
            result.y += fc * pdf;
        }
	}
	result /= float(MONTECARLO_NUM_SAMPLES);
	color = vec4(result.xy*4, 0, 0);
}