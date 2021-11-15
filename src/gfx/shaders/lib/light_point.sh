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
	vec3 lightDir = light.position - worldPos;
	float distToLightSquare = dot(lightDir, lightDir);
	float distToLight = sqrt(distToLightSquare);
	lightDir /= distToLight;

	const float minLightRadius = 0.01;
	float invLightRadius = 1.0 / light.outerRadius;
	float factor = distToLightSquare * invLightRadius * invLightRadius;
	float smoothFactor = max(1.0 - factor * factor, 0.0);
	float atten = (smoothFactor * smoothFactor) / max(distToLightSquare, minLightRadius * minLightRadius);

	float nDotL = max(0.0, dot(normal, lightDir));
	return nDotL * light.color * light.intensity * atten;
}
#endif
