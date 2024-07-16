#ifndef SH_EXTRA_GFX
#define SH_EXTRA_GFX

#include "shards_types.hpp"
#include <shards/common_types.hpp>
#include <shards/core/foundation.hpp>
#include <gfx/drawable.hpp>
#include <gfx/fwd.hpp>
#include <input/input_stack.hpp>
#include <shards/core/exposed_type_utils.hpp>
#include <vector>
#include <shards/shards.hpp>

namespace gfx {
struct Window;
struct Renderer;

struct GraphicsContext {
  static constexpr uint32_t TypeId = 'mwnd';
  static inline SHTypeInfo Type{SHType::Object, {.object = {.vendorId = VendorId, .typeId = TypeId}}};
  static inline const char VariableName[] = "GFX.Context";
  static inline const SHOptionalString VariableDescription = SHCCSTR("The graphics context.");
  static inline SHExposedTypeInfo VariableInfo = shards::ExposedInfo::ProtectedVariable(VariableName, VariableDescription, Type);

  std::shared_ptr<Context> context;
  std::shared_ptr<Window> window;
  std::shared_ptr<Renderer> renderer;

  double time{};
  float deltaTime{};
  bool frameInProgress{};

  ::gfx::Context &getContext();
  ::gfx::Window &getWindow();
};

using RequiredGraphicsContext =
    shards::RequiredContextVariable<GraphicsContext, GraphicsContext::Type, GraphicsContext::VariableName>;
using OptionalGraphicsContext =
    shards::RequiredContextVariable<GraphicsContext, GraphicsContext::Type, GraphicsContext::VariableName, false>;

struct GraphicsRendererContext {
  static constexpr uint32_t TypeId = 'grcx';
  static inline SHTypeInfo Type{SHType::Object, {.object = {.vendorId = VendorId, .typeId = TypeId}}};
  static inline const char VariableName[] = "GFX.RendererContext";
  static inline const SHOptionalString VariableDescription = SHCCSTR("The graphics renderer context.");
  static inline SHExposedTypeInfo VariableInfo = shards::ExposedInfo::ProtectedVariable(VariableName, VariableDescription, Type);

  Renderer *renderer{};

  // Overridable callback for rendering
  std::function<void(ViewPtr view, const PipelineSteps &pipelineSteps)> render;
};

typedef shards::RequiredContextVariable<GraphicsRendererContext, GraphicsRendererContext::Type,
                                        GraphicsRendererContext::VariableName>
    RequiredGraphicsRendererContext;
typedef shards::RequiredContextVariable<GraphicsRendererContext, GraphicsRendererContext::Type,
                                        GraphicsRendererContext::VariableName, false>
    OptionalGraphicsRendererContext;

} // namespace gfx

#endif // SH_EXTRA_GFX
