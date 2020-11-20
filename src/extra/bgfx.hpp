/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#pragma once

#include "bgfx/bgfx.h"
#include "bgfx/platform.h"
#include "blocks/shared.hpp"
#include "shaderc.h"

using namespace chainblocks;

namespace BGFX {
constexpr uint32_t BgfxTextureHandleCC = 'bgfT';
constexpr uint32_t BgfxShaderHandleCC = 'bgfS';
constexpr uint32_t BgfxContextCC = 'bgfx';
constexpr uint32_t BgfxNativeWindowCC = 'bgfW';

struct NativeWindow {
  static inline Type Info{
      {CBType::Object,
       {.object = {.vendorId = CoreCC, .typeId = BgfxNativeWindowCC}}}};
};

struct Context {
  static inline Type Info{
      {CBType::Object,
       {.object = {.vendorId = CoreCC, .typeId = BgfxContextCC}}}};

  // Useful to compare with with plugins, they might mismatch!
  const static inline uint32_t BgfxABIVersion = BGFX_API_VERSION;
};

struct Texture {
  static inline Type TextureHandleType{
      {CBType::Object,
       {.object = {.vendorId = CoreCC, .typeId = BgfxTextureHandleCC}}}};

  typedef ObjectVar<Texture> VarInfo;
  static inline VarInfo Var{"BGFX-Texture", CoreCC, BgfxTextureHandleCC};

  bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
  uint16_t width = 0;
  uint16_t height = 0;
  uint8_t channels = 0;
  int bpp = 1;

  ~Texture() {
    if (handle.idx != bgfx::kInvalidHandle) {
      bgfx::destroy(handle);
    }
  }
};

struct ShaderHandle {
  static inline Type ShaderHandleType{
      {CBType::Object,
       {.object = {.vendorId = CoreCC, .typeId = BgfxShaderHandleCC}}}};

  bgfx::ShaderHandle handle = BGFX_INVALID_HANDLE;
};
}; // namespace BGFX
