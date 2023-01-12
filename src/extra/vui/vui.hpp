#ifndef A70CCFB2_9913_4743_9046_5624A7B1ED9A
#define A70CCFB2_9913_4743_9046_5624A7B1ED9A

#include <common_types.hpp>
#include <foundation.hpp>
#include <shards.hpp>
#include <exposed_type_utils.hpp>

namespace shards::VUI {

struct VUIContext {
  static constexpr uint32_t TypeId = 'vuic';
  static inline SHTypeInfo Type{SHType::Object, {.object = {.vendorId = CoreCC, .typeId = TypeId}}};
  static inline const char VariableName[] = "VUI.Context";
  static inline const SHOptionalString VariableDescription = SHCCSTR("The virtual UI context.");
  static inline SHExposedTypeInfo VariableInfo =
      shards::ExposedInfo::ProtectedVariable(VariableName, VariableDescription, Type);
};

typedef shards::RequiredContextVariable<VUIContext, VUIContext::Type, VUIContext::VariableName> RequiredVUIContext;

}

#endif /* A70CCFB2_9913_4743_9046_5624A7B1ED9A */
