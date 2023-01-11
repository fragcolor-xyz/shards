#ifndef SH_EXTRA_GFX
#define SH_EXTRA_GFX

#include "gfx/shards_types.hpp"
#include <SDL_events.h>
#include <common_types.hpp>
#include <foundation.hpp>
#include <gfx/drawable.hpp>
#include <gfx/fwd.hpp>
#include <input/input_stack.hpp>
#include <exposed_type_utils.hpp>
#include <shards.hpp>

namespace gfx {
struct Window;
struct Renderer;

struct GraphicsContext {
  static constexpr uint32_t TypeId = 'mwnd';
  static inline SHTypeInfo Type{SHType::Object, {.object = {.vendorId = VendorId, .typeId = TypeId}}};
  static inline const char VariableName[] = "GFX.Context";
  static inline const SHOptionalString VariableDescription = SHCCSTR("The graphics context.");
  static inline SHExposedTypeInfo VariableInfo =
      shards::ExposedInfo::ProtectedVariable(VariableName, VariableDescription, Type);

  std::shared_ptr<Context> context;
  std::shared_ptr<Window> window;
  std::shared_ptr<Renderer> renderer;

  double time;
  float deltaTime;

  // Draw queue used when it's not manually specified
  SHDrawQueue shDrawQueue;

  ::gfx::DrawQueuePtr getDrawQueue() { return shDrawQueue.queue; }
  ::gfx::Context &getContext();
  ::gfx::Window &getWindow();
  SDL_Window *getSdlWindow();
};

typedef shards::RequiredContextVariable<GraphicsContext, GraphicsContext::Type, GraphicsContext::VariableName> RequiredGraphicsContext;

struct ContextUserData {
  SHContext *shardsContext{};
};

inline void composeCheckGfxThread(const SHInstanceData &data) {
  if (data.onWorkerThread) {
    throw shards::ComposeError("GFX Shards cannot be used on a worker thread (e.g. "
                               "within an Await shard)");
  }
}
} // namespace gfx

#endif // SH_EXTRA_GFX
