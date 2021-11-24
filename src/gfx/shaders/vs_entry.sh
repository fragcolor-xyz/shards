#include <lib.sh>

struct MaterialInfo {
	vec3 localPosition;
	vec3 worldPosition;
	mat4 worldMatrix;
	vec4 ndcPosition;
	vec4 color;
	vec3 normal;
	vec3 tangent;
};

vec3 generateTangent(vec3 normal) { return normalize(cross(vec3(normal.y, -normal.x, normal.z), normal)); }

#if GFX_HAS_VS_MATERIAL_MAIN
void materialMain(inout MaterialInfo mi);
#endif

void main() {
	MaterialInfo mi;
	mi.localPosition = a_position;
#if GFX_HAS_VERTEX_NORMAL
	mi.normal = a_normal;
#else
	mi.normal = normalize(a_position);
#endif
#if GFX_HAS_VERTEX_TANGENT
	mi.normal = a_tangent;
#else
	mi.tangent = generateTangent(mi.normal);
#endif
#if GFX_HAS_VERTEX_COLOR0
	mi.color = a_color0;
#else
	mi.color = vec4_splat(1.0);
#endif
#if GFX_HAS_VERTEX_TEXCOORD0
	v_texcoord0 = a_texcoord0;
#endif

#if GFX_FLIP_TEX_Y
	v_texcoord0.y = 1.0 - v_texcoord0.y;
#endif

#if GFX_FULLSCREEN
	mi.worldPosition = mi.localPosition;
	mi.ndcPosition = vec4(mi.localPosition, 1.0);
#else
	mi.worldMatrix = u_model[0];
	mi.worldPosition = mul(mi.worldMatrix, vec4(mi.localPosition, 1.0)).xyz;
	mi.ndcPosition = mul(u_viewProj, vec4(mi.worldPosition, 1.0));
#endif

#if GFX_HAS_VS_MATERIAL_MAIN
	materialMain(mi);
#endif

	gl_Position = mi.ndcPosition;

	v_worldPosition = mi.worldPosition;
	v_normal = mi.normal;
	v_tangent = mi.tangent;
	v_color0 = mi.color;
}