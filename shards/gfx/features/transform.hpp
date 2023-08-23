#ifndef GFX_FEATURES_TRANSFORM
#define GFX_FEATURES_TRANSFORM

#include <gfx/fwd.hpp>

namespace gfx {
namespace features {

struct Transform {
  static FeaturePtr create(bool applyView = true, bool applyProjection = true);
};

} // namespace features
} // namespace gfx

#endif // GFX_FEATURES_TRANSFORM
