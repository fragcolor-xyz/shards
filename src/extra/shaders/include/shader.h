/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2021 Giovanni Petrantoni */

#ifndef CB_SHADER_H
#define CB_SHADER_H

#include "bgfx_shader.h"
#include "shaderlib.h"

uniform vec4 u_private_time4; // w is unused
#define u_timeDelta u_private_time4.x
#define u_timeAbs u_private_time4.y
#define u_frameIdx u_private_time4.z

#endif