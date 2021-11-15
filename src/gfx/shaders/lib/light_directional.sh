#if GFX_MAX_DIR_LIGHTS
#define GFX_DIR_LIGHT_DATA_STRIDE 2
uniform vec4 u_dirLightData[GFX_MAX_DIR_LIGHTS * GFX_DIR_LIGHT_DATA_STRIDE];

struct DirectionalLight {
	vec3 color;
	float intensity;
	vec3 direction;
};

vec3 directionalLight(in DirectionalLight light, vec3 viewDirection, vec3 normal) {
	float nDotL = max(0.0, dot(normal, -light.direction));
	return nDotL * light.color * light.intensity;
}
#endif
