#include "context.hpp"
#include "../shards_types.hpp"
#include <gfx/context.hpp>
#include <gfx/window.hpp>
#include <shards/core/object_var_util.hpp>
#include <shards/core/params.hpp>
#include <shards/modules/inputs/inputs.hpp>
#include "core/module.hpp"

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
  PARAM(ShardsVar, _content, "Content",
        "Actual logic to draw the actual gizmos, the input of this flow will be a boolean that will be true if the gizmo is "
        "being pressed and so edited.",
        {CoreInfo::ShardsOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_view), PARAM_IMPL_FOR(_queue), PARAM_IMPL_FOR(_content));

  RequiredGraphicsContext _graphicsContext;
  Inputs::RequiredInputContext _inputContext;

  GizmoContext _gizmoContext{};
  SHVar *_contextVarRef{};

  int2 _cursorPosition{};
  bool _mouseButtonState{};

  ExposedInfo _exposedInfo;

  void warmup(SHContext *context) {
    _graphicsContext.warmup(context);
    _inputContext.warmup(context);

    // Reference context variable
    _contextVarRef = referenceVariable(context, GizmoContext::VariableName);

    withObjectVariable(*_contextVarRef, &_gizmoContext, GizmoContext::Type, [&] { PARAM_WARMUP(context); });
  }

  void cleanup() {
    withObjectVariable(*_contextVarRef, &_gizmoContext, GizmoContext::Type, [&] { PARAM_CLEANUP(); });

    _graphicsContext.cleanup();
    _inputContext.cleanup();

    if (_contextVarRef) {
      releaseVariable(_contextVarRef);
    }
  }

  SHExposedTypesInfo requiredVariables() {
    static auto e =
        exposedTypesOf(RequiredGraphicsContext::getExposedTypeInfo(), Inputs::RequiredInputContext::getExposedTypeInfo());
    return e;
  }

  SHTypeInfo compose(SHInstanceData &data) {
    gfx::composeCheckGfxThread(data);

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

  void handleGizmoInputEvents(std::vector<SDL_Event> &events) {
    for (auto &event : events) {
      if (event.type == SDL_MOUSEMOTION) {
        _cursorPosition.x = event.motion.x;
        _cursorPosition.y = event.motion.y;
      } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
        _cursorPosition.x = event.button.x;
        _cursorPosition.y = event.button.y;
        if (event.button.button == SDL_BUTTON_LEFT) {
          _mouseButtonState = event.button.state == SDL_PRESSED;
        }
      }
    }
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    Var queueVar(_queue.get());
    Var viewVar(_view.get());

    ViewPtr view = static_cast<SHView *>(viewVar.payload.objectValue)->view;

    assert(queueVar.payload.objectValue);
    _gizmoContext.queue = static_cast<SHDrawQueue *>(queueVar.payload.objectValue)->queue;
    assert(_gizmoContext.queue);

    gfx::gizmos::Context &gfxGizmoContext = _gizmoContext.gfxGizmoContext;
    gfx::Window &window = _graphicsContext->getWindow();
    int2 outputSize = _graphicsContext->context->getMainOutputSize();

    handleGizmoInputEvents(_inputContext->events);

    float2 inputScale = window.getInputScale();

    gfx::gizmos::InputState gizmoInput;
    gizmoInput.cursorPosition = float2(_cursorPosition) * inputScale;
    gizmoInput.pressed = _mouseButtonState;
    gizmoInput.viewSize = float2(outputSize);

    SHVar _shardsOutput{};
    withObjectVariable(*_contextVarRef, &_gizmoContext, GizmoContext::Type, [&] {
      gfxGizmoContext.begin(gizmoInput, view);
      _content.activate(shContext, Var(gfxGizmoContext.input.held != nullptr), _shardsOutput);
      gfxGizmoContext.end(_gizmoContext.queue);
    });

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
