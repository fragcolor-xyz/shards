/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

#include "common.sh"

uniform vec4 u_picking_id;

void main() {
	gl_FragColor = u_picking_id;
}
