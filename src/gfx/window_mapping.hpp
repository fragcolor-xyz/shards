#ifndef C8F3D0A3_080C_4279_9AA2_AAC8F0D9E7D3
#define C8F3D0A3_080C_4279_9AA2_AAC8F0D9E7D3

#include <variant>
#include "types.hpp"

namespace gfx {
struct Window;

/// A sub-region of the window area
struct WindowSubRegion {
  Rect region;

  static WindowSubRegion fromEntireWindow(Window &window);
};

// Placeholder for world space planes

/// Describes a rectangular region and how it maps to window coordinates
/// <div rustbindgen hide></div>
typedef std::variant<WindowSubRegion> WindowMapping;
} // namespace gfx

#endif /* C8F3D0A3_080C_4279_9AA2_AAC8F0D9E7D3 */
