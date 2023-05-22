#ifndef A70CCFB2_9913_4743_9046_5624A7B1ED9A
#define A70CCFB2_9913_4743_9046_5624A7B1ED9A

#include <shards/common_types.hpp>
#include <shards/core/foundation.hpp>
#include <shards/shards.hpp>
#include <shards/core/exposed_type_utils.hpp>
#include "context.hpp"
#include "panel.hpp"

namespace shards::Spatial {

struct Panel : public shards::spatial::Panel {
  struct SpatialPanelShard &panelShard;

  Panel(SpatialPanelShard &panelShard) : panelShard(panelShard) {}
  virtual const egui::FullOutput &render(const egui::Input &inputs);
  virtual spatial::PanelGeometry getGeometry() const;
};

struct SpatialContext {
  static constexpr uint32_t TypeId = 'spac';
  static inline SHTypeInfo Type{SHType::Object, {.object = {.vendorId = CoreCC, .typeId = TypeId}}};
  static inline const char VariableName[] = "Spatial.UI";
  static inline const SHOptionalString VariableDescription = SHCCSTR("The spatial UI context.");
  static inline SHExposedTypeInfo VariableInfo = shards::ExposedInfo::ProtectedVariable(VariableName, VariableDescription, Type);

  SHContext *activationContext{};
  SHVar activationInput{};
  spatial::Context context;
  std::vector<Panel> panels;
};

typedef shards::RequiredContextVariable<SpatialContext, SpatialContext::Type, SpatialContext::VariableName>
    RequiredSpatialContext;

} // namespace shards::Spatial

#endif /* A70CCFB2_9913_4743_9046_5624A7B1ED9A */
