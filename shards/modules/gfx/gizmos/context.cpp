#include "context.hpp"
#include <gfx/context.hpp>
#include <gfx/window.hpp>
#include <shards/core/object_var_util.hpp>
#include <shards/core/params.hpp>
#include <shards/core/module.hpp>
#include <shards/modules/inputs/inputs.hpp>
#include "../shards_types.hpp"
#include "../window.hpp"

using namespace shards::input;

namespace shards {
namespace Gizmos {
using namespace gfx;
struct GizmosContextShard {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR("Provides a context for rendering gizmos"); }

  PARAM_PARAMVAR(_view, "View",
                 "The view used to render the gizmos."
                 "When drawing over a scene, the view should be the same.",
                 {Type::VariableOf(gfx::Types::View)});
  PARAM_PARAMVAR(_queue, "Queue", "The queue to draw into.", {Type::VariableOf(gfx::Types::DrawQueue)});
  PARAM(ShardsVar, _content, "Contents",
        "Actual logic to draw the actual gizmos, the input of this flow will be a boolean that will be true if the gizmo is "
        "being pressed and so edited.",
        {CoreInfo::ShardsOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_view), PARAM_IMPL_FOR(_queue), PARAM_IMPL_FOR(_content));

  input::OptionalInputContext _inputContext;

  GizmoContext _gizmoContext{};
  SHVar *_contextVarRef{};

  int2 _cursorPosition{};
  bool _mouseButtonState{};

  ExposedInfo _exposedInfo;

  void warmup(SHContext *context) {
    _inputContext.warmup(context);

    // Reference context variable
    _contextVarRef = referenceVariable(context, GizmoContext::VariableName);

    withObjectVariable(*_contextVarRef, &_gizmoContext, GizmoContext::Type, [&] { PARAM_WARMUP(context); });
  }

  void cleanup() {
    if (_contextVarRef) {
      withObjectVariable(*_contextVarRef, &_gizmoContext, GizmoContext::Type, [&] { PARAM_CLEANUP(); });
      releaseVariable(_contextVarRef);
      _contextVarRef = nullptr;
    }

    _inputContext.cleanup();
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    _inputContext.compose(data, _requiredVariables);

    if (!_queue.isVariable())
      throw ComposeError("Queue not set");

    if (!_view.isVariable())
      throw ComposeError("View not set");

    _exposedInfo = ExposedInfo(data.shared);
    _exposedInfo.push_back(GizmoContext::VariableInfo);

    SHInstanceData contentInstanceData = data;
    contentInstanceData.inputType = CoreInfo::BoolType; // pressed or not
    contentInstanceData.shared = SHExposedTypesInfo(_exposedInfo);
    return _content.compose(contentInstanceData).outputType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    Var queueVar(_queue.get());
    Var viewVar(_view.get());

    ViewPtr view = static_cast<SHView *>(viewVar.payload.objectValue)->view;

    assert(queueVar.payload.objectValue);
    _gizmoContext.queue = static_cast<SHDrawQueue *>(queueVar.payload.objectValue)->queue;
    assert(_gizmoContext.queue);

    gfx::gizmos::Context &gfxGizmoContext = _gizmoContext.gfxGizmoContext;

    gfx::gizmos::InputState gizmoInput;
    if (_inputContext) {
      auto &region = _inputContext->getState().region;
      gizmoInput.cursorPosition = _inputContext->getState().cursorPosition;
      gizmoInput.pressed = _inputContext->getState().isMouseButtonHeld(SDL_BUTTON_LEFT);
      gizmoInput.viewSize = float2(region.size);
    }

    SHVar _shardsOutput{};
    withObjectVariable(*_contextVarRef, &_gizmoContext, GizmoContext::Type, [&] {
      gfxGizmoContext.begin(gizmoInput, view);
      _content.activate(shContext, Var(gfxGizmoContext.input.held != nullptr), _shardsOutput);
      gfxGizmoContext.end(_gizmoContext.queue);
    });

    if (_inputContext) {
      auto &consumeFlags = _inputContext->getConsumeFlags();
      consumeFlags.requestFocus = _gizmoContext.gfxGizmoContext.input.held;
      consumeFlags.wantsPointerInput = _gizmoContext.gfxGizmoContext.input.held || _gizmoContext.gfxGizmoContext.input.hovering;
      consumeFlags.wantsKeyboardInput = consumeFlags.wantsPointerInput;
    }

    return _shardsOutput;
  }
};

extern void registerHighlightShards();
extern void registerGizmoShards();
extern void registerShapeShards();
} // namespace Gizmos
} // namespace shards
SHARDS_REGISTER_FN(gizmos) {
  using namespace shards::Gizmos;
  REGISTER_SHARD("Gizmos.Context", GizmosContextShard);
  registerHighlightShards();
  registerGizmoShards();
  registerShapeShards();
}
