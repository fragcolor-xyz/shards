#ifndef A70CCFB2_9913_4743_9046_5624A7B1ED9A
#define A70CCFB2_9913_4743_9046_5624A7B1ED9A

#include <common_types.hpp>
#include <foundation.hpp>
#include <shards.hpp>
#include <exposed_type_utils.hpp>
#include <virtual_ui/context.hpp>
#include <virtual_ui/panel.hpp>

namespace shards::VUI {

struct Panel : public shards::vui::Panel {
  struct VUIPanelShard &panelShard;

  Panel(VUIPanelShard &panelShard) : panelShard(panelShard) {}
  virtual const egui::FullOutput &render(const egui::Input &inputs);
  virtual vui::PanelGeometry getGeometry() const;
};

struct VUIContext {
  static constexpr uint32_t TypeId = 'vuic';
  static inline SHTypeInfo Type{SHType::Object, {.object = {.vendorId = CoreCC, .typeId = TypeId}}};
  static inline const char VariableName[] = "VUI.Context";
  static inline const SHOptionalString VariableDescription = SHCCSTR("The virtual UI context.");
  static inline SHExposedTypeInfo VariableInfo = shards::ExposedInfo::ProtectedVariable(VariableName, VariableDescription, Type);

  SHContext *activationContext{};
  SHVar activationInput{};
  vui::Context context;
  std::vector<Panel> panels;
};

typedef shards::RequiredContextVariable<VUIContext, VUIContext::Type, VUIContext::VariableName> RequiredVUIContext;

} // namespace shards::VUI

#endif /* A70CCFB2_9913_4743_9046_5624A7B1ED9A */
