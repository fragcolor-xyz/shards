// #define MONTECARLO_NUM_SAMPLES
// #define MONTECARLO_FUNCTION
// #define MONTECARLO_SAMPLE_ENVIRONMENT(dir, lod)
//
// MONTECARLO_FUNCTION(data) Is passed the MonteCarloInput struct that is passed in the header
// 	should fill and return the MonteCarloOutput struct

#include "lib/hammersley2d.sh"
#include "lib/env_texture.sh"

uniform vec4 u_filterInputDimensions;

float getWeightedLod(float pdf) {
	// https://cgg.mff.cuni.cz/~jaroslav/papers/2007-sketch-fis/Final_sap_0073.pdf
	vec2 inputDims = u_filterInputDimensions.xy;
	return 0.5 * log2(6.0 * float(inputDims.x) * float(inputDims.x) / (float(MONTECARLO_NUM_SAMPLES) * pdf));
}

mat3 generateFrameFromZDirection(vec3 normal) {
	vec3 bitangent = vec3(0.0, 1.0, 0.0);

	float NdotUp = dot(normal, vec3(0.0, 1.0, 0.0));
	float epsilon = 0.00001;
	if (1.0 - abs(NdotUp) <= epsilon) {
		// Sampling +Y or -Y, so we need a more robust bitangent.
		if (NdotUp > 0.0) {
			bitangent = vec3(0.0, 0.0, 1.0);
		}
		else {
			bitangent = vec3(0.0, 0.0, -1.0);
		}
	}

	vec3 tangent = normalize(cross(bitangent, normal));
	bitangent = cross(normal, tangent);

	return mtxFromCols(tangent, bitangent, normal);
}

vec3 montecarlo(in vec2 inputUV) {
	vec3 baseDir = longLatUvToDirection(inputUV.xy);
	mat3 tbn = generateFrameFromZDirection(baseDir);

	float weight = 0.0;
	vec3 result = vec3(0, 0, 0);
	for (int sampleIndex = 0; sampleIndex < MONTECARLO_NUM_SAMPLES; sampleIndex++) {
		MonteCarloInput mci;
		mci.baseDirection = baseDir;
		mci.coord = hammersley2d(sampleIndex, MONTECARLO_NUM_SAMPLES);
		mci.uvCoord = inputUV;

		MonteCarloOutput mco = MONTECARLO_FUNCTION(mci);

		float lod = getWeightedLod(mco.pdf);

		vec3 direction = mul(tbn, mco.localDirection);
		vec3 s = MONTECARLO_SAMPLE_ENVIRONMENT(direction, lod);
		result += s * mco.sampleScale;
		weight += mco.sampleWeight;
	}
	result /= weight;

	return result;
}