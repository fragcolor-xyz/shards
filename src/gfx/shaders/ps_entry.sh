#include <lib.sh>

uniform vec4 u_baseColor;

#ifdef TEXTURE_FIELDS
TEXTURE_FIELDS
#endif

#include <ps_material.sh>

#ifdef GFX_HAS_PS_MATERIAL_MAIN
void materialMain(inout MaterialInfo mi);
#endif

#include <lib/lighting.sh>

void main() {
	MaterialInfo mi;
	mi.texcoord0 = v_texcoord0;
	mi.DEFAULT_COLOR_FIELD = v_color0;
	mi.normal = v_normal;
	mi.tangent = v_tangent;
	mi.worldPosition = v_worldPosition;

#if GFX_LIT
	mi.ior = 1.5;
	mi.specularColor0 = vec3_splat(0.04);
	mi.specularWeight = 1.0;
	materialSetMetallicRoughness(mi, 1.0, 0.3);
#endif

	mi.DEFAULT_COLOR_FIELD *= u_baseColor;

#ifdef GFX_BASE_COLOR_TEXTURE
	mi.DEFAULT_COLOR_FIELD = mi.DEFAULT_COLOR_FIELD * texture2D(u_baseColorTexture, u_baseColorTexture_texcoord);
#endif

#ifdef GFX_HAS_PS_MATERIAL_MAIN
	materialMain(mi);
#endif

#ifdef GFX_LIT
	vec3 negViewDirection = normalize(getCameraPosition() - mi.worldPosition);
	mi.DEFAULT_COLOR_FIELD.xyz *= processLights(mi, negViewDirection);
#endif

#ifdef GFX_MRT_ASSIGNMENTS
	GFX_MRT_ASSIGNMENTS
#else
	gl_FragColor = mi.DEFAULT_COLOR_FIELD;
#endif
}