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
namespace Snappy {
extern void registerBlocks();
}

namespace Brotli {
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
  Audio::registerBlocks();
  DSP::registerBlocks();

#ifdef _WIN32
#endif
}
}; // namespace chainblocks
