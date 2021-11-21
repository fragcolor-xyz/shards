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

struct MaterialInfo {
	vec2 texcoord0;
	vec4 color;
	vec3 normal;
	vec3 tangent;
};

#ifdef GFX_HAS_PS_MATERIAL_MAIN
void materialMain(inout MaterialInfo mi);
#endif

void main() {
	MaterialInfo mi;
	mi.texcoord0 = v_texcoord0;
	mi.color = v_color0;
	mi.normal = v_normal;
	mi.tangent = v_tangent;
	
	mi.color *= u_baseColor;

#ifdef GFX_BASE_COLOR_TEXTURE
	mi.color = mi.color * texture2D(u_baseColorTexture, u_baseColorTexture_texcoord);
#endif

#ifdef GFX_HAS_PS_MATERIAL_MAIN
	materialMain(mi);
#endif

	gl_FragColor = mi.color;
}