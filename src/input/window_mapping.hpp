#ifndef C8F3D0A3_080C_4279_9AA2_AAC8F0D9E7D3
#define C8F3D0A3_080C_4279_9AA2_AAC8F0D9E7D3

#include "types.hpp"
#include <variant>

namespace gfx {
struct Window;
}

namespace shards::input {
// A sub-region of the window area
// Coordinates are in pixel coordinates
struct WindowSubRegion {
  Rect region;

  static WindowSubRegion fromEntireWindow(gfx::Window &window);
};

// Placeholder for world space planes

/// Describes a rectangular region and how it maps to window coordinates
/// <div rustbindgen hide></div>
typedef std::variant<WindowSubRegion> WindowMapping;
} // namespace shards::input

#endif /* C8F3D0A3_080C_4279_9AA2_AAC8F0D9E7D3 */
