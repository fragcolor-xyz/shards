#include <lib/pbr.sh>

#define MONTECARLO_NUM_SAMPLES 512
#define MONTECARLO_FUNCTION lambert
#define MONTECARLO_SAMPLE_ENVIRONMENT(_dir, _lod) textureCubeLod(u_filterInput, _dir, _lod).xyz
#include <lib/montecarlo.sh>

MonteCarloOutput lambert(MonteCarloInput mci) {
	MonteCarloOutput result;
	LightingVectorSample lvs = importanceSampleLambert(mci.coord);
	result.localDirection = lvs.localDirection;
	result.pdf = lvs.pdf;
	result.sampleWeight = 1.0;
	result.sampleScale = 1.0;
	return result;
}

#include <lib/montecarlo_impl.sh>

void main(inout MaterialInfo info) {
	info.color = vec4(montecarlo(info.texcoord0), 1);
}
