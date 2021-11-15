#ifndef PS_MATERIAL_SH
#define PS_MATERIAL_SH

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

#if GFX_LIT
	float metallic;
	float perceptualRoughness;
	float specularWeight;
	vec3 specularColor0;
	vec3 diffuseColor;
	float ior;
#endif
};

#ifndef DEFAULT_COLOR_FIELD
#define DEFAULT_COLOR_FIELD color
#endif

#if GFX_LIT
void materialSetIor(inout MaterialInfo mi, float u_Ior) {
	mi.specularColor0 = vec3_splat(pow((u_Ior - 1.0) / (u_Ior + 1.0), 2.0));
	mi.ior = u_Ior;
}

void materialSetMetallicRoughness(inout MaterialInfo mi, float u_MetallicFactor, float u_RoughnessFactor) {
	mi.metallic = u_MetallicFactor;
	mi.perceptualRoughness = u_RoughnessFactor;

	// Achromatic specularColor0 based on IOR.
	mi.diffuseColor = mix(mi.DEFAULT_COLOR_FIELD.rgb * (vec3_splat(1.0) - mi.specularColor0), vec3_splat(0.0), mi.metallic);
	mi.specularColor0 = mix(mi.specularColor0, mi.DEFAULT_COLOR_FIELD.rgb, mi.metallic);
}

void materialSetSpecularColor(inout MaterialInfo mi, vec3 color, float weight) {
	vec3 dielectricSpecularF0 = min(mi.specularColor0 * color, vec3_splat(1.0));
	mi.specularColor0 = mix(dielectricSpecularF0, mi.DEFAULT_COLOR_FIELD.rgb, mi.metallic);
	mi.specularWeight = weight;
	mi.diffuseColor = mix(mi.DEFAULT_COLOR_FIELD.rgb * (1.0 - max3(dielectricSpecularF0)), vec3_splat(0.0), mi.metallic);
}
#endif

#endif
