#pragma once

#include "lib/const.glsl"

// Conversion to spherical coordinates with r=1
vec2 directionToLongLat(vec3 dir) {
	vec2 result;
	if (dir.x >= 0) {
		result.x = atan(dir.z / dir.x);
	}
	else {
		result.x = atan(dir.z / dir.x) + PI;
	}
	result.y = acos(dir.y);
	return result;
}

// Conversion from long-lat uv to direction vector
vec3 uvToDirection(vec2 uv) {
	float theta = uv.x * PI2; // azimuthal angle
	float phi = uv.y * PI; // polar angle
	
	vec3 result;
	result.x = cos(theta) * sin(phi);
	result.z = sin(theta) * sin(phi);
	result.y = cos(phi);
	return result;
}

vec3 sampleEnvironmentLod(in sampler2D inTexture, vec3 dir, float lod) {
	vec2 polar = directionToLongLat(dir);
	vec2 uv = vec2(
		(polar.x / PI2), // azimuthal angle
		(polar.y / PI) // polar angle 
	);
	return textureLod(inTexture, uv, lod).xyz;
}

vec3 sampleEnvironment(in sampler2D inTexture, vec3 dir) {
	return sampleEnvironmentLod(inTexture, dir, 8.0);
}
