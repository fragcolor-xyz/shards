#include "window_mapping.hpp"
#include <gfx/window.hpp>

namespace shards::input {
WindowSubRegion WindowSubRegion::fromEntireWindow(gfx::Window &window) {
  int2 windowSize = window.getSize();
  return WindowSubRegion{
      .region = Rect(0, 0, windowSize.x, windowSize.y),
  };
}
} // namespace shards::input
