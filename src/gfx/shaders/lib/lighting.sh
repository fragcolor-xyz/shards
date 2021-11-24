#ifndef LIGHTING_SH
#define LIGHTING_SH

#include "light_directional.sh"
#include "light_point.sh"

#if GFX_ENVIRONMENT_LIGHT
#include "light_environment.sh"
#endif

vec3 processLights(in MaterialInfo mi, vec3 negViewDirection) {
	vec3 worldPosition = mi.worldPosition;
	vec3 normal = mi.normal;
	vec3 result = vec3_splat(0);

#if GFX_MAX_DIR_LIGHTS
	DirectionalLight light;
	for (int i = 0; i < GFX_MAX_DIR_LIGHTS * GFX_DIR_LIGHT_DATA_STRIDE; i += GFX_DIR_LIGHT_DATA_STRIDE) {
		light.color = u_dirLightData[i].xyz;
		light.direction = u_dirLightData[i + 1].xyz;
		light.intensity = u_dirLightData[i + 1].w;
		result += directionalLight(light, negViewDirection, normal);
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
		result += pointLight(light, worldPosition, negViewDirection, normal);
	}
#endif

#if GFX_ENVIRONMENT_LIGHT
	result += environmentLight(mi, negViewDirection);
#endif

	return result;
}
#endif
