#ifndef C7154E20_4050_4970_A6CC_9431AEA6EE79
#define C7154E20_4050_4970_A6CC_9431AEA6EE79

#include "input.hpp"
#include <gfx/window.hpp>

namespace shards::input {
using namespace linalg::aliases;

inline InputRegion getWindowInputRegion(gfx::Window &window) {
  return InputRegion{
      .size = float2(window.getSize()),
      .pixelSize = window.getDrawableSize(),
      .uiScalingFactor = window.getUIScale(),
  };
}
} // namespace shards::input

#endif /* C7154E20_4050_4970_A6CC_9431AEA6EE79 */
