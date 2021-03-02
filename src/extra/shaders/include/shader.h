/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2021 Giovanni Petrantoni */

#include "bgfx_shader.h"
#include "shaderlib.h"

uniform vec4 u_private_time4; // we might change this in the future, use yzw for other stuff
#define u_time u_private_time4.x