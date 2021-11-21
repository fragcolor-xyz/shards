#include <bgfx_shader.sh>

struct MaterialInfo {
	vec3 localPosition;
	vec4 color;
	vec3 normal;
	vec3 tangent;
};

vec3 generateTangent(vec3 normal) {
	return normalize(cross(vec3(normal.y, -normal.x, normal.z), normal));
}

#if GFX_HAS_USER_CODE
void materialMain(inout MaterialInfo);
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

#if GFX_HAS_USER_CODE
	materialMain(mi);
#endif

	mat4 worldMatrix = u_model[0];
	vec4 worldPos = mul(worldMatrix, vec4(mi.localPosition.xyz, 1.0));
	gl_Position = mul(u_viewProj, worldPos);

	v_normal = mi.normal;
	v_tangent = mi.tangent;
	v_color0 = mi.color;
}