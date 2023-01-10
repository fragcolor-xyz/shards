#ifndef DACF8017_857B_4084_9427_A52FCAE0D044
#define DACF8017_857B_4084_9427_A52FCAE0D044

#include "gfx/window.hpp"
#include <foundation.hpp>
#include <input/input_stack.hpp>

namespace shards::Inputs {

struct Context {
  static constexpr uint32_t TypeId = 'ictx';
  static inline shards::Type Type{{SHType::Object, {.object = {.vendorId = CoreCC, .typeId = TypeId}}}};

  std::shared_ptr<gfx::Window> window;
  std::vector<SDL_Event> events;
  shards::input::InputStack inputStack;

  double time;
  float deltaTime;
};

struct BaseConsumer {
  static inline const char *varName = "Inputs";
  static inline SHExposedTypeInfo cVarInfo = shards::ExposedInfo::Variable(varName, SHCCSTR("The input context."), Context::Type);
  static inline shards::ExposedInfo varInfo = shards::ExposedInfo(cVarInfo);

  SHVar *_inputContextVar{nullptr};

  Context &getInputContext() { return *shards::varAsObjectChecked<Context>(*_inputContextVar, Context::Type); }
};
} // namespace shards::Inputs

#endif /* DACF8017_857B_4084_9427_A52FCAE0D044 */
