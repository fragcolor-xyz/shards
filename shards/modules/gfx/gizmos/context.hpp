#ifndef C55FCF15_9B8F_44F0_AA7B_73BA2144579D
#define C55FCF15_9B8F_44F0_AA7B_73BA2144579D

#include "../gfx.hpp"
#include <shards/common_types.hpp>
#include <shards/core/foundation.hpp>
#include <shards/core/params.hpp>
#include <gfx/fwd.hpp>
#include <gfx/gizmos/gizmos.hpp>
#include <gfx/gizmos/wireframe.hpp>

namespace shards {
// Shards for rendering visual helpers
// Gizmos, grids, lines, etc.
namespace Gizmos {

struct GizmoContext {
  static constexpr uint32_t TypeId = 'HLPc';
  static inline SHTypeInfo Type{SHType::Object, {.object = {.vendorId = gfx::VendorId, .typeId = TypeId}}};
  static inline const char VariableName[] = "Gizmos.Context";
  static inline const SHOptionalString VariableDescription = SHCCSTR("The gizmo context.");
  static inline SHExposedTypeInfo VariableInfo = shards::ExposedInfo::ProtectedVariable(VariableName, VariableDescription, Type);

  gfx::DrawQueuePtr queue;
  gfx::gizmos::Context gfxGizmoContext;
  gfx::WireframeRenderer wireframeRenderer;
};

typedef shards::RequiredContextVariable<GizmoContext, GizmoContext::Type, GizmoContext::VariableName> RequiredGizmoContext;

struct Base {
  RequiredGizmoContext _gizmoContext;

  void baseWarmup(SHContext *context) { _gizmoContext.warmup(context); }
  void baseCleanup(SHContext* context) { _gizmoContext.cleanup(); }
  void baseCompose() {
    _requiredVariables.push_back(RequiredGizmoContext::getExposedTypeInfo());
  }

  void warmup(SHContext *context) { baseWarmup(context); }
  void cleanup(SHContext* context) { baseCleanup(context); }

  PARAM_REQUIRED_VARIABLES();
};

} // namespace Gizmos
} // namespace shards

#endif /* C55FCF15_9B8F_44F0_AA7B_73BA2144579D */
