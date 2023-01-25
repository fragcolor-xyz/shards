#ifndef DACF8017_857B_4084_9427_A52FCAE0D044
#define DACF8017_857B_4084_9427_A52FCAE0D044

#include "gfx/window.hpp"
#include <foundation.hpp>
#include <input/input_stack.hpp>
#include <exposed_type_utils.hpp>

namespace shards::Inputs {

struct InputContext {
  static constexpr uint32_t TypeId = 'ictx';
  static inline SHTypeInfo Type{SHType::Object, {.object = {.vendorId = CoreCC, .typeId = TypeId}}};
  static inline const char VariableName[] = "Input.Context";
  static inline const SHOptionalString VariableDescription = SHCCSTR("The input context.");
  static inline SHExposedTypeInfo VariableInfo =
      shards::ExposedInfo::ProtectedVariable(VariableName, VariableDescription, Type);

  std::shared_ptr<gfx::Window> window;
  std::vector<SDL_Event> events;
  shards::input::InputStack inputStack;

  double time;
  float deltaTime;

  ::gfx::Window &getWindow();
  SDL_Window *getSdlWindow();
};

typedef shards::RequiredContextVariable<InputContext, InputContext::Type, InputContext::VariableName> RequiredInputContext;
typedef shards::RequiredContextVariable<InputContext, InputContext::Type, InputContext::VariableName, false> OptionalInputContext;

} // namespace shards::Inputs

#endif /* DACF8017_857B_4084_9427_A52FCAE0D044 */
