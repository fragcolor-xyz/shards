#ifndef B44846E1_9E91_4350_8C77_98F3A10E8882
#define B44846E1_9E91_4350_8C77_98F3A10E8882

#include <vector>
#include <algorithm>
#include <linalg/linalg.h>

namespace shards::input {
using namespace linalg::aliases;

struct InputRegion {
  // Size in virtual pixels, same space as input coordinates
  float2 size;
  // Size in physical pixels
  int2 pixelSize;
  // Request OS scaling factor for UI
  float uiScalingFactor{};
};

} // namespace shards::input

#endif /* B44846E1_9E91_4350_8C77_98F3A10E8882 */
