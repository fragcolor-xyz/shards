#include "window_mapping.hpp"
#include <gfx/window.hpp>

namespace shards::input {
WindowSubRegion WindowSubRegion::fromEntireWindow(gfx::Window &window) {
  int2 drawableSize = window.getDrawableSize();
  return WindowSubRegion{
      .region = Rect(0, 0, drawableSize.x, drawableSize.y),
  };
}
} // namespace shards::input
