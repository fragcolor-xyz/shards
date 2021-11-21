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
	vec4 color;
	vec3 normal;
	vec3 tangent;
};

#ifdef GFX_HAS_USER_CODE
void materialMain(inout MaterialInfo);
#endif

void main() {
	MaterialInfo mi;
	mi.color = v_color0;
	
	mi.color *= u_baseColor;

#ifdef GFX_BASE_COLOR_TEXTURE
	mi.color = mi.color * texture2D(u_baseColorTexture, u_baseColorTexture_texcoord);
#endif

	mi.normal = v_normal;
	mi.tangent = v_tangent;

#ifdef GFX_HAS_USER_CODE
	materialMain(mi);
#endif

	gl_FragColor = mi.color;
}