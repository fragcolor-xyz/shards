/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2021 Giovanni Petrantoni */

#define STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c" // Enables Vorbis decoding.

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

// The stb_vorbis implementation must come after the implementation of
// miniaudio.
#undef STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"

namespace chainblocks {
namespace Audio {

/*

Inner audio chains should not be allowed to have (Pause) or clipping would
happen Also they should probably run like RunChain Detached so that multiple
references to the same chain would be possible and they would just produce
another iteration

*/

struct Output {};

struct Input {};

void registerBlocks() {}
} // namespace Audio
} // namespace chainblocks