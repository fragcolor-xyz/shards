#ifndef A498DFD1_1C40_4DCE_8F87_1AC66002779A
#define A498DFD1_1C40_4DCE_8F87_1AC66002779A

#include <gfx/rust_interop.hpp>

namespace shards::spatial {
struct PanelGeometry {
  gfx::float3 anchor;
  gfx::float3 center;
  gfx::float3 up;
  gfx::float3 right;
  gfx::float2 size;

#ifndef RUST_BINDGEN
  PanelGeometry scaled(float scale) const {
    return PanelGeometry{
        .anchor = anchor,
        .center = center,
        .up = up,
        .right = right,
        .size = size * scale,
    };
  }

  gfx::float3 getTopLeft() const {
    gfx::float2 halfSize = size * 0.5f;
    return center - right * halfSize.x + up * halfSize.y;
  }

  gfx::float3 getBottomRight() const {
    gfx::float2 halfSize = size * 0.5f;
    return center + right * halfSize.x - up * halfSize.y;
  }
#endif
};
} // namespace shards::spatial

#endif /* A498DFD1_1C40_4DCE_8F87_1AC66002779A */
