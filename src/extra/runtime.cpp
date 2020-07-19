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

#ifndef CB_NO_SNAPPY_BLOCKS
namespace Snappy {
extern void registerBlocks();
}
#endif

void cbInitExtras() {
  BGFX::registerBGFXBlocks();
  chainblocks::ImGui::registerImGuiBlocks();
#ifdef _WIN32
  Desktop::registerDesktopBlocks();
#endif
#ifndef CB_NO_SNAPPY_BLOCKS
  Snappy::registerBlocks();
#endif
}
}; // namespace chainblocks
