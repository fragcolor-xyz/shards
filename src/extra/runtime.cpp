/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#include "runtime.hpp"

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

extern "C" void registerRustBlocks(CBCore *core);

void cbInitExtras() {
  BGFX::registerBGFXBlocks();
  chainblocks::ImGui::registerImGuiBlocks();
  XR::registerBlocks();
  gltf::registerBlocks();
#ifdef _WIN32
  Desktop::registerDesktopBlocks();
#endif
  Snappy::registerBlocks();
  Brotli::registerBlocks();
  registerRustBlocks(chainblocksInterface(CHAINBLOCKS_CURRENT_ABI));
}
}; // namespace chainblocks
