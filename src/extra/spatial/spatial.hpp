#ifndef A70CCFB2_9913_4743_9046_5624A7B1ED9A
#define A70CCFB2_9913_4743_9046_5624A7B1ED9A

#include <common_types.hpp>
#include <foundation.hpp>
#include <shards.hpp>
#include <exposed_type_utils.hpp>
#include <spatial/context.hpp>
#include <spatial/panel.hpp>

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
  std::vector<shards::spatial::Panel> panels;
};

typedef shards::RequiredContextVariable<SpatialContext, SpatialContext::Type, SpatialContext::VariableName>
    RequiredSpatialContext;

} // namespace shards::Spatial

#endif /* A70CCFB2_9913_4743_9046_5624A7B1ED9A */
