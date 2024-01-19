#include "spatial.hpp"
#include <shards/modules/gfx/gfx.hpp>
#include <shards/modules/gfx/window.hpp>
#include <shards/modules/inputs/inputs.hpp>
#include <shards/modules/egui/context.hpp>
#include <shards/modules/egui/egui_types.hpp>
#include <shards/core/shared.hpp>
#include <shards/linalg_shim.hpp>
#include <shards/core/object_var_util.hpp>
#include <shards/core/params.hpp>
#include <gfx/linalg.hpp>
#include <gfx/renderer.hpp>
#include <gfx/gizmos/gizmos.hpp>
#include <input/input.hpp>

namespace shards {
namespace Spatial {

struct SpatialPanelShard;
struct SpatialUIContextShard {
  SpatialContext _spatialContext{};
  SHVar *_spatialContextVar{};

  RequiredWindowContext _windowContext;
  gfx::RequiredGraphicsContext _graphicsContext;

  ExposedInfo _exposedVariables;

  std::vector<SpatialPanelShard *> _panels;

  input::InputBuffer _inputBuffer;

  std::shared_ptr<gfx::GizmoRenderer> _debugRenderer;

  PARAM_PARAMVAR(_queue, "Queue", "The draw queue to insert draw commands into.", {Type::VariableOf(gfx::ShardsTypes::DrawQueue)});
  PARAM_PARAMVAR(_view, "View", "The view that is being used to render.", {Type::VariableOf(gfx::ShardsTypes::View)});
  PARAM(ShardsVar, _contents, "Contents", "The list of UI panels to render.", {CoreInfo::ShardsOrNone});
  PARAM_VAR(_scale, "Scale", "The scale of how many UI units per world unit.", {CoreInfo::FloatType});
  PARAM_VAR(_debug, "Debug", "Visualize panel outlines and pointer input being sent to panels.", {CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_queue), PARAM_IMPL_FOR(_view), PARAM_IMPL_FOR(_contents),
             PARAM_IMPL_FOR(_scale), PARAM_IMPL_FOR(_debug));

  SpatialUIContextShard() {
    _scale = Var(1000.0f);
    _debug = Var(false);
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static SHOptionalString help() {
    return SHCCSTR("Creates a context for spatial UI panels to make sure input is correctly handled between them");
  }

  SHExposedTypesInfo requiredVariables() {
    static auto e =
        exposedTypesOf(decltype(_windowContext)::getExposedTypeInfo(), decltype(_graphicsContext)::getExposedTypeInfo());
    return e;
  }

  void warmup(SHContext *context);

  void cleanup(SHContext* context) {
    PARAM_CLEANUP(context)

    _contents.cleanup();
    _graphicsContext.cleanup();
    _windowContext.cleanup();

    if (_spatialContextVar) {
      if (_spatialContextVar->refcount > 1) {
        SHLOG_ERROR("Spatial.UI: Found {} dangling reference(s) to {}", _spatialContextVar->refcount - 1,
                    SpatialContext::VariableName);
      }
      releaseVariable(_spatialContextVar);
    }
  }

  SHTypeInfo compose(SHInstanceData &data) {
    _exposedVariables = ExposedInfo(data.shared);
    _exposedVariables.push_back(SpatialContext::VariableInfo);
    data.shared = SHExposedTypesInfo(_exposedVariables);

    _contents.compose(data);

    if (_queue->valueType == SHType::None)
      throw ComposeError("Queue is required");

    if (_view->valueType == SHType::None)
      throw ComposeError("View is required");

    return data.inputType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &viewStack = _graphicsContext->renderer->getViewStack();
    auto viewStackTop = viewStack.getOutput();

    auto &queue = varAsObjectChecked<gfx::SHDrawQueue>(_queue.get(), gfx::ShardsTypes::DrawQueue);
    auto &view = varAsObjectChecked<gfx::SHView>(_view.get(), gfx::ShardsTypes::View);

    // TODO: Move to input context
    // _inputBuffer.clear();
    // for (auto &event : _windowContext->events)
    //   _inputBuffer.push_back(event);

    // Evaluate all UI panels
    _spatialContext.activationContext = shContext;
    _spatialContext.activationInput = input;
    _spatialContext.context.virtualPointScale = _scale.payload.floatValue;
    withObjectVariable(*_spatialContextVar, &_spatialContext, SpatialContext::Type, [&]() {
      gfx::SizedView sizedView(view.view, gfx::float2(viewStackTop.viewport.getSize()));
      _spatialContext.context.prepareInputs(_inputBuffer, _graphicsContext->window->getInputScale(), sizedView);
      _spatialContext.context.evaluate(queue.queue, _graphicsContext->time, _graphicsContext->deltaTime);
    });
    _spatialContext.activationContext = nullptr;

    // Render debug overlay
    if ((bool)*_debug) {
      if (!_debugRenderer) {
        _debugRenderer = std::make_shared<gfx::GizmoRenderer>();
      }

      _debugRenderer->begin(view.view, gfx::float2(viewStackTop.viewport.getSize()));
      _spatialContext.context.renderDebug(_debugRenderer->getShapeRenderer());
      _debugRenderer->end(queue.queue);
    }

    return input;
  }
};

struct SpatialPanelShard {
  PARAM_PARAMVAR(_transform, "Transform", "The world transform of this panel.",
                 {CoreInfo::Float4x4Type, Type::VariableOf(CoreInfo::Float4x4Type)});
  PARAM_PARAMVAR(_size, "Size", "The size of the panel.", {CoreInfo::Float2Type, Type::VariableOf(CoreInfo::Float2Type)});
  PARAM(ShardsVar, _contents, "Contents", "The panel UI contents.", {CoreInfo::ShardsOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_transform), PARAM_IMPL_FOR(_size), PARAM_IMPL_FOR(_contents));

  RequiredSpatialContext _context;
  Shard *_uiShard{};
  EguiHost _eguiHost;
  ExposedInfo _exposedTypes;
  std::shared_ptr<Panel> _panel;

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR("Defines a spatial UI panel"); }

  SHExposedTypesInfo requiredVariables() {
    static auto e = exposedTypesOf(RequiredSpatialContext::getExposedTypeInfo());
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

  void cleanup(SHContext* context) {
    PARAM_CLEANUP(context);
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
    throw ActivationError("Invalid activation, SpatialPanel can not be used directly");
  }

  // This evaluates the egui contents for this panel
  virtual const egui::FullOutput &render(const egui::Input &inputs) {
    SHVar output{};
    const char *error =
        egui_hostActivate(_eguiHost, inputs, _contents.shards(), _context->activationContext, _context->activationInput, output);
    if (error)
      throw ActivationError(fmt::format("egui activation error: {}", error));

    const egui::FullOutput &eguiOutput = *egui_hostGetOutput(_eguiHost);
    return eguiOutput;
  }

  spatial::PanelGeometry getGeometry() const {
    gfx::float4x4 transform = toFloat4x4(_transform.get());
    gfx::float2 alignment{0.5f};

    spatial::PanelGeometry result;
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
spatial::PanelGeometry Panel::getGeometry() const { return panelShard.getGeometry(); }

void SpatialUIContextShard::warmup(SHContext *context) {
  _spatialContextVar = referenceVariable(context, SpatialContext::VariableName);

  _windowContext.warmup(context);
  _graphicsContext.warmup(context);

  withObjectVariable(*_spatialContextVar, &_spatialContext, SpatialContext::Type, [&]() { PARAM_WARMUP(context); });
}
} // namespace Spatial
} // namespace shards
SHARDS_REGISTER_FN(spatial) {
  using namespace shards::Spatial;
  REGISTER_SHARD("Spatial.UI", SpatialUIContextShard);
  REGISTER_SHARD("Spatial.Panel", SpatialPanelShard);
}
