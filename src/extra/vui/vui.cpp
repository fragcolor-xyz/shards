#include "vui.hpp"
#include "../gfx.hpp"
#include "../inputs.hpp"
#include "gfx/egui/egui_types.hpp"
#include "gfx/linalg.hpp"
#include "linalg_shim.hpp"
#include "object_var_util.hpp"
#include "params.hpp"
#include "../egui/context.hpp"
#include <gfx/renderer.hpp>
#include <gfx/gizmos/gizmos.hpp>
#include <input/input.hpp>

namespace shards::VUI {

#define VUI_PANEL_SHARD_NAME "VUI.Panel"

struct VUIPanelShard;
struct VUIContextShard {
  VUIContext _vuiContext{};
  SHVar *_vuiContextVar{};

  Inputs::RequiredInputContext _inputContext;
  gfx::RequiredGraphicsContext _graphicsContext;

  ExposedInfo _exposedVariables;

  std::vector<VUIPanelShard *> _panels;

  input::InputBuffer _inputBuffer;

  std::shared_ptr<gfx::GizmoRenderer> _debugRenderer;

  PARAM_PARAMVAR(_queue, "Queue", "The draw queue to insert draw commands into.", {Type::VariableOf(gfx::Types::DrawQueue)});
  PARAM_PARAMVAR(_view, "View", "The view that is being used to render.", {Type::VariableOf(gfx::Types::View)});
  PARAM(ShardsVar, _contents, "Contents", "The list of UI panels to render.", {CoreInfo::ShardsOrNone});
  PARAM_VAR(_scale, "Scale", "The scale of how many UI units per world unit.", {CoreInfo::FloatType});
  PARAM_VAR(_debug, "Debug", "Visualize panel outlines and pointer input being sent to panels.", {CoreInfo::BoolType});
  PARAM_IMPL(VUIContextShard, PARAM_IMPL_FOR(_queue), PARAM_IMPL_FOR(_view), PARAM_IMPL_FOR(_contents), PARAM_IMPL_FOR(_scale),
             PARAM_IMPL_FOR(_debug));

  VUIContextShard() {
    _scale = Var(1000.0f);
    _debug = Var(false);
  }

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() {
    return SHCCSTR("Creates a context for virtual UI panels to make sure input is correctly handled between them");
  }

  SHExposedTypesInfo requiredVariables() {
    static auto e =
        exposedTypesOf(decltype(_inputContext)::getExposedTypeInfo(), decltype(_graphicsContext)::getExposedTypeInfo());
    return e;
  }

  void warmup(SHContext *context);

  void cleanup() {
    PARAM_CLEANUP()

    _contents.cleanup();
    _graphicsContext.cleanup();
    _inputContext.cleanup();

    if (_vuiContextVar) {
      if (_vuiContextVar->refcount > 1) {
        SHLOG_ERROR("VUI.Context: Found {} dangling reference(s) to {}", _vuiContextVar->refcount - 1, VUIContext::VariableName);
      }
      releaseVariable(_vuiContextVar);
    }
  }

  SHTypeInfo compose(SHInstanceData &data) {
    _exposedVariables = ExposedInfo(data.shared);
    _exposedVariables.push_back(VUIContext::VariableInfo);
    data.shared = SHExposedTypesInfo(_exposedVariables);

    _contents.compose(data);

    if (_queue->valueType == SHType::None)
      throw ComposeError("Queue is required");

    if (_view->valueType == SHType::None)
      throw ComposeError("View is required");

    return shards::CoreInfo::NoneType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &viewStack = _graphicsContext->renderer->getViewStack();
    auto viewStackTop = viewStack.getOutput();

    auto &queue = *varAsObjectChecked<gfx::SHDrawQueue>(_queue.get(), gfx::Types::DrawQueue);
    auto &view = *varAsObjectChecked<gfx::SHView>(_view.get(), gfx::Types::View);

    // TODO: Move to input context
    _inputBuffer.clear();
    for (auto &event : _inputContext->events)
      _inputBuffer.push_back(event);

    // Evaluate all UI panels
    _vuiContext.activationContext = shContext;
    _vuiContext.context.virtualPointScale = _scale.payload.floatValue;
    withObjectVariable(*_vuiContextVar, &_vuiContext, VUIContext::Type, [&]() {
      gfx::SizedView sizedView(view.view, gfx::float2(viewStackTop.viewport.getSize()));
      _vuiContext.context.prepareInputs(_inputBuffer, _graphicsContext->window->getInputScale(), sizedView);
      _vuiContext.context.evaluate(queue.queue, _graphicsContext->time, _graphicsContext->deltaTime);
    });
    _vuiContext.activationContext = nullptr;

    // Render debug overlay
    if ((bool)_debug) {
      if (!_debugRenderer) {
        _debugRenderer = std::make_shared<gfx::GizmoRenderer>();
      }

      _debugRenderer->begin(view.view, gfx::float2(viewStackTop.viewport.getSize()));
      _vuiContext.context.renderDebug(_debugRenderer->getShapeRenderer());
      _debugRenderer->end(queue.queue);
    }

    SHVar output{};
    return output;
  }
};

struct VUIPanelShard {
  PARAM_PARAMVAR(_transform, "Transform", "The world transform of this panel.",
                 {CoreInfo::Float4x4Type, Type::VariableOf(CoreInfo::Float4x4Type)});
  PARAM_PARAMVAR(_size, "Size", "The size of the panel.", {CoreInfo::Float2Type, Type::VariableOf(CoreInfo::Float2Type)});
  PARAM(ShardsVar, _contents, "Contents", "The panel UI contents.", {CoreInfo::ShardsOrNone});
  PARAM_IMPL(VUIPanelShard, PARAM_IMPL_FOR(_transform), PARAM_IMPL_FOR(_size), PARAM_IMPL_FOR(_contents));

  RequiredVUIContext _context;
  Shard *_uiShard{};
  EguiHost _eguiHost;
  ExposedInfo _exposedTypes;
  std::shared_ptr<Panel> _panel;

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR("Defines a virtual UI panel"); }

  SHExposedTypesInfo requiredVariables() {
    static auto e = exposedTypesOf(RequiredVUIContext::getExposedTypeInfo());
    return e;
  }

  void ensureEguiHost() {
    if (!_eguiHost)
      _eguiHost = egui_createHost();
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _context.warmup(context);

    ensureEguiHost();
    egui_hostWarmup(_eguiHost, context);

    _panel = std::make_shared<Panel>(*this);
    _context->context.panels.emplace_back(_panel);
  }

  void cleanup() {
    PARAM_CLEANUP();
    _context.cleanup();

    if (_eguiHost) {
      egui_hostCleanup(_eguiHost);
      egui_destroyHost(_eguiHost);
      _eguiHost = nullptr;
    }

    _panel.reset();
  }

  SHTypeInfo compose(SHInstanceData &data) {
    ensureEguiHost();

    SHExposedTypesInfo eguiExposedTypes{};
    egui_hostGetExposedTypeInfo(_eguiHost, eguiExposedTypes);

    _exposedTypes = ExposedInfo(data.shared);
    mergeIntoExposedInfo(_exposedTypes, eguiExposedTypes);

    // Compose contents
    data.shared = SHExposedTypesInfo(_exposedTypes);
    _contents.compose(data);

    return shards::CoreInfo::NoneType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    throw ActivationError("Invalid activation, VUIPanel can not be used directly");
  }

  // This evaluates the egui contents for this panel
  virtual const egui::FullOutput &render(const egui::Input &inputs) {
    SHVar output{};
    const char *error = egui_hostActivate(_eguiHost, inputs, _contents.shards(), _context->activationContext, SHVar{}, output);
    if (error)
      throw ActivationError(fmt::format("egui activation error: {}", error));

    const egui::FullOutput &eguiOutput = *egui_hostGetOutput(_eguiHost);
    return eguiOutput;
  }

  vui::PanelGeometry getGeometry() const {
    gfx::float4x4 transform = toFloat4x4(_transform.get());
    gfx::float2 alignment{0.5f};

    vui::PanelGeometry result;
    result.anchor = gfx::extractTranslation(transform);
    result.up = transform.y.xyz();
    result.right = transform.x.xyz();
    result.size = toFloat2(_size.get());
    result.center =
        result.anchor + result.right * (0.5f - alignment.x) * result.size.x + result.up * (0.5f - alignment.y) * result.size.y;
    return result;
  }
};

const egui::FullOutput &Panel::render(const egui::Input &inputs) { return panelShard.render(inputs); }
vui::PanelGeometry Panel::getGeometry() const { return panelShard.getGeometry(); }

void VUIContextShard::warmup(SHContext *context) {
  _vuiContextVar = referenceVariable(context, VUIContext::VariableName);

  _inputContext.warmup(context);
  _graphicsContext.warmup(context);

  withObjectVariable(*_vuiContextVar, &_vuiContext, VUIContext::Type, [&]() { PARAM_WARMUP(context); });
}

void registerShards() {
  REGISTER_SHARD("VUI.Context", VUIContextShard);
  REGISTER_SHARD(VUI_PANEL_SHARD_NAME, VUIPanelShard);
}
} // namespace shards::VUI
