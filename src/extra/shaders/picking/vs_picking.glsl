$input a_position
$output v_color0

/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

#include <shader.h>

uniform vec4 u_picking_id;

void main() {
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
	v_color0 = u_picking_id;
}
