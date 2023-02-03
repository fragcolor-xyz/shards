#pragma once

// Theta is the polar angle
// Phi is the azimuthal angle
// Result is Z=up at theta=0
vec3 localHemisphereDirectionHelper(float cosTheta, float sinTheta, float phi) {
	return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

#define saturate(x) clamp(x, 0, 1)

float max3(vec3 v) {
	return max(max(v.x, v.y), v.z);
}
