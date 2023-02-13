#include "external.hpp"
#include "spatial.hpp"
#include "gfx/linalg.hpp"
#include "linalg_shim.hpp"

using namespace shards;
using namespace shards::Spatial;

void spatial_add_panel(ExtPanelPtr ptr, const SHVar &spatialContextVar) {
  SpatialContext *spatialContext = varAsObjectChecked<SpatialContext>(spatialContextVar, SpatialContext::Type);

  std::shared_ptr<ExternalPanel> shared = std::make_shared<ExternalPanel>(ptr);
  spatialContext->context.panels.emplace_back(shared);
  // FIXME that seems hackish
  // TODO: return a handle to help removal
}

void spatial_remove_panel(ExtPanelPtr impl, const SHVar &spatialContextVar) {
  // TODO use handle returned by spatial_add_panel()

  SpatialContext *spatialContext = varAsObjectChecked<SpatialContext>(spatialContextVar, SpatialContext::Type);
  auto &panels = spatialContext->context.panels;

  auto find = std::find_if(panels.begin(), panels.end(), [impl](const spatial::PanelPtr &s) {
    auto external = std::dynamic_pointer_cast<ExternalPanel>(s);
    return external.get() != nullptr && external->impl == impl;
  });

  if (find != panels.end()) {
    panels.erase(find);
  }
}

spatial::PanelGeometry spatial_temp_get_geometry_from_transform(const SHVar &_transform, const SHVar &_size) {
 gfx::float4x4 transform = toFloat4x4(_transform);
  gfx::float2 alignment{0.5f};

  spatial::PanelGeometry result;
  result.anchor = gfx::extractTranslation(transform);
  result.up = transform.y.xyz();
  result.right = transform.x.xyz();
  result.size = toFloat2(_size);
  result.center =
      result.anchor + result.right * (0.5f - alignment.x) * result.size.x + result.up * (0.5f - alignment.y) * result.size.y;
  return result;
}
