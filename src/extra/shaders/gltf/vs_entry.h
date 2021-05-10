$input a_position, a_normal, a_tangent, a_texcoord0, a_texcoord1, a_color0, i_data0, i_data1, i_data2, i_data3
$output v_normal, v_tangent, v_bitangent, v_texcoord0, v_texcoord1, v_color0, v_wpos, v_view

/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2021 Giovanni Petrantoni */

#include <bgfx_shader.h>
#include <shaderlib.h>

mat3 mtx3FromCols(vec3 c0, vec3 c1, vec3 c2) {
#ifdef BGFX_SHADER_LANGUAGE_GLSL
	return mat3(c0, c1, c2);
#else
	return transpose(mat3(c0, c1, c2));
#endif
}

void main() {
#ifdef CB_INSTANCED
	mat4 model;
	model[0] = i_data0;
	model[1] = i_data1;
	model[2] = i_data2;
	model[3] = i_data3;
#else
	#define model u_model[0]
#endif

	// this will need to become vec4 when we add anims and morphs and divide by .w
	vec3 wpos = mul(model, vec4(a_position, 1.0) ).xyz;
	v_wpos = wpos;

	gl_Position = mul(u_viewProj, vec4(wpos, 1.0) );

#ifdef CB_HAS_NORMAL
	vec3 normal = a_normal * 2.0 - 1.0;
	vec3 wnormal = mul(model, vec4(normal, 0.0)).xyz;
	v_normal = normalize(wnormal);
#ifdef CB_HAS_TANGENT
	vec4 tangent = a_tangent * 2.0 - 1.0;
	vec3 wtangent = mul(model, vec4(tangent.xyz, 0.0)).xyz;
	v_tangent = normalize(wtangent);
	v_bitangent = cross(v_normal, v_tangent) * tangent.w;
	mat3 tbn = mtx3FromCols(v_tangent, v_bitangent, v_normal);
	// eye position in world space
	vec3 weyepos = mul(vec4(0.0, 0.0, 0.0, 1.0), u_view).xyz;
	// tangent space view dir
	v_view = mul(weyepos - wpos, tbn);
#endif
#endif

#ifdef CB_HAS_TEXCOORD_0
	v_texcoord0 = a_texcoord0;
#endif
#ifdef CB_HAS_TEXCOORD_1
	v_texcoord1 = a_texcoord1;
#endif

#ifdef CB_HAS_COLOR_0
	v_color0 = a_color0;
#endif
}
