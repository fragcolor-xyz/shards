#ifndef FAD79EC4_CD09_4DDB_88A1_E168CCE2B4F5
#define FAD79EC4_CD09_4DDB_88A1_E168CCE2B4F5

#include "input/input_stack.hpp"
#include <input/messages.hpp>
#include <input/state.hpp>
#include <input/events.hpp>
#include <input/master.hpp>
#include <input/input_stack.hpp>
#include <shards/linalg_shim.hpp>
#include <shards/core/foundation.hpp>
#include <shards/core/exposed_type_utils.hpp>
#include <vector>

namespace shards::input {
struct InputMaster;

struct IInputContext {
  static constexpr uint32_t TypeId = 'ictx';
  static inline SHTypeInfo Type{SHType::Object, {.object = {.vendorId = CoreCC, .typeId = TypeId}}};
  static inline const char VariableName[] = "Shards.InputContext";
  static inline const SHOptionalString VariableDescription = SHCCSTR("The input context.");
  static inline SHExposedTypeInfo VariableInfo = shards::ExposedInfo::ProtectedVariable(VariableName, VariableDescription, Type);

  virtual ~IInputContext() = default;

  virtual shards::input::InputMaster &getMaster() const = 0;

  virtual shards::input::InputStack &getInputStack() = 0;

  virtual void postMessage(const Message &message) = 0;
  virtual const InputState &getState() const = 0;
  virtual std::vector<ConsumableEvent> &getEvents() = 0;

  virtual const std::weak_ptr<IInputHandler> &getHandler() const = 0;
  EventConsumer getEventConsumer() const { return EventConsumer(getHandler()); }

  virtual bool requestFocus() = 0;
  virtual bool canReceiveInput() const = 0;

  virtual float getTime() const = 0;
  virtual float getDeltaTime() const = 0;
};

using RequiredInputContext = shards::RequiredContextVariable<IInputContext, IInputContext::Type, IInputContext::VariableName>;
using OptionalInputContext =
    shards::RequiredContextVariable<IInputContext, IInputContext::Type, IInputContext::VariableName, false>;

} // namespace shards::input

#endif /* FAD79EC4_CD09_4DDB_88A1_E168CCE2B4F5 */
