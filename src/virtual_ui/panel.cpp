#include "panel.hpp"
using namespace gfx;
namespace shards::vui {
PanelGeometry Panel::getGeometry() const {
  PanelGeometry result;
  result.anchor = extractTranslation(transform);
  result.up = transform.y.xyz();
  result.right = transform.x.xyz();
  result.size = size;
  result.center =
      result.anchor + result.right * (0.5f - alignment.x) * result.size.x + result.up * (0.5f - alignment.y) * result.size.y;
  return result;
}
} // namespace shards::vui