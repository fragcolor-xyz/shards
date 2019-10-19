#pragma once

#include "bgfx/bgfx.h"
#include "bgfx/platform.h"
#include "blocks/shared.hpp"

using namespace chainblocks;

namespace BGFX {
constexpr uint32_t BgfxTextureHandleCC = 'bgfT';
constexpr uint32_t BgfxContextCC = 'bgfx';

struct Context {
  static inline TypeInfo Info = TypeInfo::Object(FragCC, BgfxContextCC);
  // Useful to compare with with plugins, they might mismatch!
  static inline uint32_t BgfxABIVersion = BGFX_API_VERSION;
};
}; // namespace BGFX