#ifndef FAD79EC4_CD09_4DDB_88A1_E168CCE2B4F5
#define FAD79EC4_CD09_4DDB_88A1_E168CCE2B4F5

#include <input/input_stack.hpp>
#include <input/detached.hpp>
#include <input/event_buffer.hpp>
#include <input/messages.hpp>
#include <shards/linalg_shim.hpp>
#include <shards/core/foundation.hpp>
#include <shards/core/exposed_type_utils.hpp>

namespace shards::input {
struct InputMaster;

struct InputContext {
  static constexpr uint32_t TypeId = 'ictx';
  static inline SHTypeInfo Type{SHType::Object, {.object = {.vendorId = CoreCC, .typeId = TypeId}}};
  static inline const char VariableName[] = "Shards.InputContext";
  static inline const SHOptionalString VariableDescription = SHCCSTR("The input context.");
  static inline SHExposedTypeInfo VariableInfo = shards::ExposedInfo::ProtectedVariable(VariableName, VariableDescription, Type);

  std::shared_ptr<gfx::Window> window;
  input::InputMaster *master{};
  input::DetachedInput detached;

  bool wantsPointerInput{};
  bool wantsKeyboardInput{};
  bool requestFocus{};

  void postMessage(const input::Message &message);
  const input::InputState &getState() const { return detached.state; }
  const std::vector<input::Event> &getEvents() const { return detached.virtualInputEvents; }

  double time{};
  float deltaTime{};
};

using RequiredInputContext = shards::RequiredContextVariable<InputContext, InputContext::Type, InputContext::VariableName>;

} // namespace shards

#endif /* FAD79EC4_CD09_4DDB_88A1_E168CCE2B4F5 */
