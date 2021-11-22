#include <common.sh>

uniform vec4 u_baseColor;

#ifdef GFX_BASE_COLOR_TEXTURE
SAMPLER2D(u_baseColorTexture, u_baseColorTexture_register);
#endif
#ifdef GFX_NORMAL_TEXTURE
SAMPLER2D(u_normalTexture, u_normalTexture_register);
#endif
#ifdef GFX_METALLIC_ROUGHNESS_TEXTURE
SAMPLER2D(u_metallicRougnessTexture, u_metallicRougnessTexture_register);
#endif
#ifdef GFX_EMISSIVE_TEXTURE
SAMPLER2D(u_emissiveTexture, u_emissiveTexture_register);
#endif

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
};

#ifdef GFX_HAS_PS_MATERIAL_MAIN
void materialMain(inout MaterialInfo mi);
#endif

void main() {
	MaterialInfo mi;
	mi.texcoord0 = v_texcoord0;
	mi.DEFAULT_COLOR_FIELD = v_color0;
	mi.normal = v_normal;
	mi.tangent = v_tangent;

	mi.DEFAULT_COLOR_FIELD *= u_baseColor;

#ifdef GFX_BASE_COLOR_TEXTURE
	mi.DEFAULT_COLOR_FIELD = mi.DEFAULT_COLOR_FIELD * texture2D(u_baseColorTexture, u_baseColorTexture_texcoord);
#endif

#ifdef GFX_HAS_PS_MATERIAL_MAIN
	materialMain(mi);
#endif

#ifdef GFX_MRT_ASSIGNMENTS
	GFX_MRT_ASSIGNMENTS
#else
	gl_FragColor = mi.DEFAULT_COLOR_FIELD;
#endif
}