#include <lib/pbr.sh>

#define MONTECARLO_NUM_SAMPLES 512
#define MONTECARLO_FUNCTION ggx
#define MONTECARLO_SAMPLE_ENVIRONMENT(_dir, _lod) textureCubeLod(u_filterInput, _dir, _lod).xyz
#include <lib/montecarlo.sh>

uniform vec4 u_roughness;

MonteCarloOutput ggx(MonteCarloInput mci) {
	MonteCarloOutput result;

	LightingVectorSample lvs = importanceSampleGGX(mci.coord, u_roughness.x);
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

#include <lib/montecarlo_impl.sh>

void main(inout MaterialInfo info) {
	info.color = vec4(montecarlo(info.texcoord0), 1);
}
