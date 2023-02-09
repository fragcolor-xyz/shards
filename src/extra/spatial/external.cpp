#include "external.hpp"
#include "spatial.hpp"

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
