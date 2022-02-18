/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "runtime.hpp"

#if CHAINBLOCKS_WITH_RUST_BLOCKS
extern "C" void registerRustBlocks(CBCore *core);
#endif

namespace gfx {
extern void registerBlocks();
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

namespace Gizmo {
extern void registerGizmoBlocks();
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

namespace Inputs {
extern void registerBlocks();
}

namespace Audio {
extern void registerBlocks();
}

namespace DSP {
extern void registerBlocks();
}

void cbInitExtras() {
#if CHAINBLOCKS_WITH_RUST_BLOCKS
  registerRustBlocks(chainblocksInterface(CHAINBLOCKS_CURRENT_ABI));
#endif

  Snappy::registerBlocks();
  Brotli::registerBlocks();

  gfx::registerBlocks();
  chainblocks::ImGui::registerImGuiBlocks();
  // chainblocks::Gizmo::registerGizmoBlocks();
  // XR::registerBlocks();
  // gltf::registerBlocks();
  Inputs::registerBlocks();
  Audio::registerBlocks();
  DSP::registerBlocks();

#ifdef _WIN32
  Desktop::registerDesktopBlocks();
#endif
}
}; // namespace chainblocks
