/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#include "runtime.hpp"

namespace BGFX {
extern void registerBGFXBlocks();
}

namespace chainblocks {
namespace ImGui {
extern void registerImGuiBlocks();
};

#ifdef _WIN32
namespace Desktop {
extern void registerDesktopBlocks();
};
#endif

void cbInitExtras() {
  BGFX::registerBGFXBlocks();
  chainblocks::ImGui::registerImGuiBlocks();
#ifdef _WIN32
  Desktop::registerDesktopBlocks();
#endif
}
}; // namespace chainblocks
