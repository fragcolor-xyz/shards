/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2021 Giovanni Petrantoni */

#include "runtime.hpp"

extern "C" void registerRustBlocks(CBCore *core);

namespace BGFX {
extern void registerBGFXBlocks();
}

#ifdef _WIN32
namespace Desktop {
extern void registerDesktopBlocks();
}
#endif

namespace chainblocks {
namespace ImGui {
extern void registerImGuiBlocks();
}

namespace Snappy {
extern void registerBlocks();
}

namespace Brotli {
extern void registerBlocks();
}

namespace XR {
extern void registerBlocks();
}

namespace gltf {
extern void registerBlocks();
}

#ifdef __EMSCRIPTEN__
extern void registerEmscriptenShaderCompiler();
#endif

void cbInitExtras() {
  registerRustBlocks(chainblocksInterface(CHAINBLOCKS_CURRENT_ABI));
  Snappy::registerBlocks();
  Brotli::registerBlocks();
#ifdef __EMSCRIPTEN__
  registerEmscriptenShaderCompiler();
#endif
  BGFX::registerBGFXBlocks();
  chainblocks::ImGui::registerImGuiBlocks();
  XR::registerBlocks();
  gltf::registerBlocks();
#ifdef _WIN32
  Desktop::registerDesktopBlocks();
#endif
}
}; // namespace chainblocks
