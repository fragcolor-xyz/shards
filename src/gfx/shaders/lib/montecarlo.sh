#ifndef MONTECARLO_SH
#define MONTECARLO_SH

struct MonteCarloInput {
	vec3 baseDirection; // Direction which is being sampled
	vec2 coord;			// Uniform coord in the unit rectangle
    vec2 uvCoord; // raw uv coordinate
};

struct MonteCarloOutput {
	vec3 localDirection;
	float pdf;
	float sampleWeight;
    float sampleScale;
};

#endif
