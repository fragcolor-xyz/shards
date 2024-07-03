#ifndef DACF8017_857B_4084_9427_A52FCAE0D044
#define DACF8017_857B_4084_9427_A52FCAE0D044

#include <boost/container/flat_set.hpp>
#include <gfx/window.hpp>
#include <shards/core/foundation.hpp>
#include <shards/core/exposed_type_utils.hpp>
#include <input/input_stack.hpp>
#include <input/master.hpp>
#include <span>

namespace shards {

struct WindowContext {
  static constexpr uint32_t TypeId = 'wctx';
  static inline SHTypeInfo Type{SHType::Object, {.object = {.vendorId = CoreCC, .typeId = TypeId}}};
  static inline const char VariableName[] = "GFX.Window";
  static inline const SHOptionalString VariableDescription = SHCCSTR("The window context.");
  static inline SHExposedTypeInfo VariableInfo = shards::ExposedInfo::ProtectedVariable(VariableName, VariableDescription, Type);

  std::shared_ptr<gfx::Window> window;
  input::InputMaster inputMaster;

  std::weak_ptr<SHMesh> windowMesh;

  double time;
  float deltaTime;

  // Call after frame
  void nextFrame();

  ::gfx::Window &getWindow();

  void reset() {
    window.reset();
    inputMaster.reset();
    time = 0.0f;
    deltaTime = 0.0f;
  }
};

typedef shards::RequiredContextVariable<WindowContext, WindowContext::Type, WindowContext::VariableName> RequiredWindowContext;
typedef shards::RequiredContextVariable<WindowContext, WindowContext::Type, WindowContext::VariableName, false>
    OptionalWindowContext;

} // namespace shards

#endif /* DACF8017_857B_4084_9427_A52FCAE0D044 */
