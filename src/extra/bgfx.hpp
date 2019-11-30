/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#pragma once

#include "bgfx/bgfx.h"
#include "bgfx/platform.h"
#include "blocks/shared.hpp"

using namespace chainblocks;

namespace BGFX {
constexpr uint32_t BgfxTextureHandleCC = 'bgfT';
constexpr uint32_t BgfxContextCC = 'bgfx';

struct Context {
  const static inline TypeInfo Info = TypeInfo::Object(FragCC, BgfxContextCC);
  // Useful to compare with with plugins, they might mismatch!
  const static inline uint32_t BgfxABIVersion = BGFX_API_VERSION;
};
}; // namespace BGFX
