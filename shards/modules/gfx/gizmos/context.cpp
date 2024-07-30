#include "context.hpp"
#include <gfx/context.hpp>
#include <gfx/window.hpp>
#include <shards/core/object_var_util.hpp>
#include <shards/core/params.hpp>
#include <shards/core/module.hpp>
#include <shards/modules/inputs/inputs.hpp>
#include <shards/modules/gfx/gfx.hpp>
#include "../shards_types.hpp"
#include "../window.hpp"
#include "shards/core/foundation.hpp"
#include "shards/shards.h"

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
                 {Type::VariableOf(gfx::ShardsTypes::View)});
  PARAM_PARAMVAR(_viewSize, "ViewSize", "The size of the view", {CoreInfo::NoneType, CoreInfo::Int2Type, CoreInfo::Int2VarType});
  PARAM_PARAMVAR(_queue, "Queue", "The queue to draw into.", {Type::VariableOf(gfx::ShardsTypes::DrawQueue)});
  PARAM_PARAMVAR(_scaling, "Scaling", "The scaling factor for gizmo elements.",
                 {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_interactive, "Interactive", "Used to togle gizmo interactions on/off.",
                 {CoreInfo::NoneType, CoreInfo::BoolVarType});
  PARAM(ShardsVar, _content, "Contents",
        "Actual logic to draw the actual gizmos, the input of this flow will be a boolean that will be true if the gizmo is "
        "being pressed and so edited.",
        {CoreInfo::ShardsOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_view), PARAM_IMPL_FOR(_viewSize), PARAM_IMPL_FOR(_queue), PARAM_IMPL_FOR(_content),
             PARAM_IMPL_FOR(_scaling), PARAM_IMPL_FOR(_interactive));

  input::OptionalInputContext _inputContext;
  gfx::OptionalGraphicsRendererContext _gfxContext;

  GizmoContext _gizmoContext{};
  SHVar *_contextVarRef{};

  int2 _cursorPosition{};
  bool _mouseButtonState{};

  ExposedInfo _innerExposedInfo;
  ExposedInfo _exposedInfo;

  size_t frameCounter{};

  SHExposedTypesInfo exposedVariables() { return SHExposedTypesInfo(_exposedInfo); }

  void warmup(SHContext *context) {
    _inputContext.warmup(context);
    _gfxContext.warmup(context);

    // Reference context variable
    _contextVarRef = referenceVariable(context, GizmoContext::VariableName);

    withObjectVariable(*_contextVarRef, &_gizmoContext, GizmoContext::Type, [&] { PARAM_WARMUP(context); });
  }

  void cleanup(SHContext *context) {
    if (_contextVarRef) {
      withObjectVariable(*_contextVarRef, &_gizmoContext, GizmoContext::Type, [&] { PARAM_CLEANUP(context); });
      releaseVariable(_contextVarRef);
      _contextVarRef = nullptr;
    }

    _inputContext.cleanup();
    _gfxContext.cleanup();
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    _inputContext.compose(data, _requiredVariables);
    _gfxContext.compose(data, _requiredVariables);

    if (!_queue.isVariable())
      throw ComposeError("Queue not set");

    if (!_view.isVariable())
      throw std::runtime_error("View must be a variable");

    if (_viewSize.isNone() && (!_gfxContext && !_inputContext)) {
      throw std::runtime_error("ViewSize must be set if running outside of graphics/input context");
    }

    _innerExposedInfo = ExposedInfo(data.shared);
    _innerExposedInfo.push_back(GizmoContext::VariableInfo);

    SHInstanceData contentInstanceData = data;
    contentInstanceData.inputType = CoreInfo::BoolType; // pressed or not
    contentInstanceData.shared = SHExposedTypesInfo(_innerExposedInfo);

    auto cr = _content.compose(contentInstanceData);

    _exposedInfo.clear();
    for (auto &exposed : cr.exposedInfo) {
      _exposedInfo.push_back(exposed);
    }

    return cr.outputType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    ++frameCounter;

    Var queueVar(_queue.get());
    Var viewVar(_view.get());
    Var scalingVar(_scaling.get());

    ViewPtr view = static_cast<SHView *>(viewVar.payload.objectValue)->view;

    assert(queueVar.payload.objectValue);
    _gizmoContext.queue = static_cast<SHDrawQueue *>(queueVar.payload.objectValue)->queue;
    assert(_gizmoContext.queue);

    _gizmoContext.wireframeRenderer.reset(frameCounter);

    gfx::gizmos::Context &gfxGizmoContext = _gizmoContext.gfxGizmoContext;
    gfxGizmoContext.renderer.scalingFactor = !scalingVar.isNone() ? float(scalingVar) : 1.0f;

    gfx::gizmos::InputState gizmoInput;

    bool isInteractive = true;
    if (_interactive.isVariable()) {
      isInteractive = (bool)(Var &)_interactive.get();
    }

    if (!_viewSize.isNone()) {
      gizmoInput.viewportSize = (float2)toFloat2(_viewSize.get());
    } else if (_gfxContext) {
      auto &vs = _gfxContext->renderer->getViewStack();
      gizmoInput.viewportSize = float2(vs.getOutput().referenceSize);
    }

    // Read input
    if (_inputContext) {
      bool canReceiveInput = _inputContext->canReceiveInput();

      auto &region = _inputContext->getState().region;
      gizmoInput.cursorPosition = _inputContext->getState().cursorPosition;
      if (isInteractive) {
        gizmoInput.held = canReceiveInput && _inputContext->getState().isMouseButtonHeld(SDL_BUTTON_LEFT);
        if (gfxGizmoContext.input.held != nullptr) {
          canReceiveInput = _inputContext->requestFocus();
        }

        for (auto &event : _inputContext->getEvents()) {
          if (const PointerButtonEvent *pbEvent = std::get_if<PointerButtonEvent>(&event.event)) {
            if ((!canReceiveInput || event.isConsumed()) && pbEvent->pressed)
              continue;

            if (pbEvent->index == SDL_BUTTON_LEFT) {
              gizmoInput.pressed = pbEvent->pressed;
            }
          }
        }
      }
      gizmoInput.inputSize = float2(region.size);
      gizmoInput.viewportSize = float2(region.pixelSize);
    }

    SHVar _shardsOutput{};
    withObjectVariable(*_contextVarRef, &_gizmoContext, GizmoContext::Type, [&] {
      gfxGizmoContext.begin(gizmoInput, view);
      _content.activate(shContext, Var(gfxGizmoContext.input.held != nullptr), _shardsOutput);
      gfxGizmoContext.end(_gizmoContext.queue);
    });

    // Consume inputs
    if (_inputContext && (gfxGizmoContext.input.hovering != nullptr || gfxGizmoContext.input.held != nullptr)) {
      auto consume = _inputContext->getEventConsumer();

      for (auto &event : _inputContext->getEvents()) {
        if (isPointerEvent(event.event)) {
          consume(event);
        }
      }
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
