#ifndef ENV_TEXTURE_SH
#define ENV_TEXTURE_SH

// Conversion to spherical coordinates with r=1
vec2 directionToLongLat(vec3 dir) {
	vec2 result;
	if (dir.x >= 0) {
		result.x = atan(dir.z / dir.x);
	} else {
		result.x = atan(dir.z / dir.x) + PI;
	}
	result.y = acos(dir.y);
	return result;
}

vec2 directionToLongLatUv(vec3 dir) {
	vec2 polar = directionToLongLat(dir);
	vec2 uv = vec2((polar.x / PI2), // azimuthal angle
				   (polar.y / PI)	// polar angle
	);
	return uv;
}

// Conversion from long-lat uv to direction vector
vec3 longLatUvToDirection(vec2 uv) {
	float theta = uv.x * PI2; // azimuthal angle
	float phi = uv.y * PI; // polar angle

	vec3 result;
	result.x = cos(theta) * sin(phi);
	result.z = sin(theta) * sin(phi);
	result.y = cos(phi);
	return result;
}

#define longLatTextureLod(_sampler, _dir, _lod) texture2DLod(_sampler, directionToLongLatUv(_dir), _lod)
#define longLatTexture(_sampler, _dir) longLatTextureLod(_sampler, _dir, 0)

#endif
