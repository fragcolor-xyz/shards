#if GFX_MAX_DIR_LIGHTS
#define GFX_DIR_LIGHT_DATA_STRIDE 2
uniform vec4 u_dirLightData[GFX_MAX_DIR_LIGHTS * GFX_DIR_LIGHT_DATA_STRIDE];

struct DirectionalLight {
	vec3 color;
	float intensity;
	vec3 direction;
};

vec3 directionalLight(in DirectionalLight light, vec3 viewDirection, vec3 normal) {
	float nDotL = max(0, dot(normal, -light.direction));
	return nDotL * light.color * light.intensity;
}
#endif

#if GFX_MAX_POINT_LIGHTS
#define GFX_POINT_LIGHT_DATA_STRIDE 3
uniform vec4 u_pointLightData[GFX_MAX_POINT_LIGHTS * GFX_POINT_LIGHT_DATA_STRIDE];

struct PointLight {
	vec3 color;
	float intensity;
	vec3 position;
	float innerRadius;
	float outerRadius;
};

vec3 pointLight(in PointLight light, vec3 worldPos, vec3 viewDirection, vec3 normal) {
	float3 lightDir = light.position - worldPos;
	float distToLightSquare = dot(lightDir, lightDir);
	float distToLight = sqrt(distToLightSquare);
	lightDir /= distToLight;

	const float minLightRadius = 0.01;
	float invLightRadius = 1.0 / light.outerRadius;
	float factor = distToLightSquare * invLightRadius * invLightRadius;
	float smoothFactor = max(1.0 - factor * factor, 0.0);
	float atten = (smoothFactor * smoothFactor) / max(distToLightSquare, minLightRadius * minLightRadius);

	float nDotL = max(0, dot(normal, lightDir));
	return nDotL * light.color * light.intensity * atten;
}
#endif

vec3 processLights(vec3 worldPosition, vec3 viewDirection, vec3 normal) {
	vec3 result = vec3_splat(0);

#if GFX_MAX_DIR_LIGHTS
	DirectionalLight light;
	for (int i = 0; i < GFX_MAX_DIR_LIGHTS * GFX_DIR_LIGHT_DATA_STRIDE; i += GFX_DIR_LIGHT_DATA_STRIDE) {
		light.color = u_dirLightData[i].xyz;
		light.direction = u_dirLightData[i + 1].xyz;
		light.intensity = u_dirLightData[i + 1].w;
		result += directionalLight(light, viewDirection, normal);
	}
#endif

#if GFX_MAX_POINT_LIGHTS
	PointLight light;
	for (int i = 0; i < GFX_MAX_POINT_LIGHTS * GFX_POINT_LIGHT_DATA_STRIDE; i += GFX_POINT_LIGHT_DATA_STRIDE) {
		light.color = u_pointLightData[i].xyz;
		light.innerRadius = u_pointLightData[i].w;
		light.position = u_pointLightData[i + 1].xyz;
		light.intensity = u_pointLightData[i + 1].w;
		light.outerRadius = u_pointLightData[i + 2].x;
		result += pointLight(light, worldPosition, viewDirection, normal);
	}
#endif

	return result;
}