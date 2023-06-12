/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "runtime.hpp"

#if SHARDS_WITH_RUST_SHARDS
extern "C" void registerRustShards(SHCore *core);
#endif

namespace gfx {
extern void registerShards();
}

#ifdef _WIN32
namespace Desktop {
extern void registerDesktopShards();
}
#endif

namespace shards {
namespace Inputs {
extern void registerShards();
}

namespace Snappy {
extern void registerShards();
}

namespace Brotli {
extern void registerShards();
}

namespace Audio {
extern void registerShards();
}

namespace DSP {
extern void registerShards();
}

namespace Gui {
extern void registerShards();
}

namespace Gizmos {
extern void registerShards();
}

namespace egui {
extern void registerShards();
}

namespace Spatial {
extern void registerShards();
}

namespace Animations {
extern void registerShards();
}

namespace UI::Clipboard {
  extern void registerShards();
}

void shInitExtras() {
#if SHARDS_WITH_RUST_SHARDS
  registerRustShards(shardsInterface(SHARDS_CURRENT_ABI));
#endif

  Snappy::registerShards();
  Brotli::registerShards();

  gfx::registerShards();
  Gizmos::registerShards();
  Inputs::registerShards();
  Audio::registerShards();
  DSP::registerShards();
  egui::registerShards();
  Spatial::registerShards();
  Animations::registerShards();
  UI::Clipboard::registerShards();

#ifdef _WIN32
  Desktop::registerDesktopShards();
#endif
}
}; // namespace shards