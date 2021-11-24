#ifndef PS_MATERIAL_SH
#define PS_MATERIAL_SH

#ifndef DEFAULT_COLOR_FIELD
#define DEFAULT_COLOR_FIELD color
#endif

struct MaterialInfo {
#ifdef GFX_MRT_FIELDS
	GFX_MRT_FIELDS
#else
	vec4 color;
#endif

	vec2 texcoord0;
	vec3 normal;
	vec3 tangent;
	vec3 worldPosition;
};

#endif
