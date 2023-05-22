#ifndef C6156B57_AD63_45C1_86D1_ABA4F1815677
#define C6156B57_AD63_45C1_86D1_ABA4F1815677

#include <gfx/fwd.hpp>

namespace gfx {
namespace features {

// Outputs per-object screen-space velocity into the 'velocity' output and global
struct Velocity {
  // Scaling factor for velocity values stored in the framebuffer
  // output = (screen pixels/s) * scaling factor
  static constexpr double scalingFactor = 32.0;

  static FeaturePtr create(bool applyView = true, bool applyProjection = true);
};

} // namespace features
} // namespace gfx

#endif /* C6156B57_AD63_45C1_86D1_ABA4F1815677 */
