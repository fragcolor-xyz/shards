#include "runtime.hpp"

namespace BGFX {
extern void registerBGFXBlocks();
}

namespace chainblocks {
namespace ImGui {
extern void registerImGuiBlocks();
};

void cbInitExtras() {
  BGFX::registerBGFXBlocks();
  chainblocks::ImGui::registerImGuiBlocks();
}
}; // namespace chainblocks