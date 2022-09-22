#include "window_mapping.hpp"
#include "window.hpp"

namespace gfx {
WindowSubRegion WindowSubRegion::fromEntireWindow(Window &window) {
  int2 windowSize = window.getSize();
  return WindowSubRegion{
      .region = Rect(windowSize.x, windowSize.y),
  };
}
} // namespace gfx
