#include "gfx.hpp"
#include "window.hpp"
#include "renderer.hpp"
#include <gfx/renderer.hpp>
#include <gfx/context.hpp>
#include <gfx/loop.hpp>
#include <gfx/window.hpp>
#include <gfx/error_utils.hpp>
#include <shards/core/params.hpp>

using namespace shards;

namespace gfx {
struct RendererShard {
  static const inline Type WindowType = WindowContext::Type;

  PARAM_PARAMVAR(_window, "Window", "The window to run the renderer on.", {Type::VariableOf(WindowType)});
  PARAM(ShardsVar, _contents, "Contents", "The main input loop of this window.", {CoreInfo::ShardsOrNone});
  PARAM_VAR(_ignoreCompilationErrors, "IgnoreCompilationErrors",
            "When enabled, shader or pipeline compilation errors will be ignored and either use fallback rendering or not "
            "render at all.",
            {CoreInfo::BoolType});
  PARAM_PARAMVAR(_debug, "Debug", "Enable debug visualization mode.",
                 {CoreInfo::NoneType, CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_window), PARAM_IMPL_FOR(_contents), PARAM_IMPL_FOR(_ignoreCompilationErrors),
             PARAM_IMPL_FOR(_debug));

  static inline Type OutputType = Type(WindowContext::Type);

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  RendererShard() { _ignoreCompilationErrors = Var(false); }

  ShardsRenderer renderer;

  ExposedInfo _innerExposedVariables;
  ExposedInfo _exposedVariables;

  SHExposedTypesInfo exposedVariables() { return SHExposedTypesInfo(_exposedVariables); }

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    _exposedVariables.clear();

    if (_window.isNone())
      throw formatException("Window parameter is required but not set");

    // Make sure that renderers are UNIQUE
    for (uint32_t i = 0; i < data.shared.len; i++) {
      if (strcmp(data.shared.elements[i].name, GraphicsContext::VariableName) == 0) {
        throw SHException("GFX.Renderer must be unique, found another use!");
      }
    }

    renderer.compose(data);

    _innerExposedVariables = ExposedInfo(data.shared);
    renderer.getExposedContextVariables(_innerExposedVariables);

    SHInstanceData innerData = data;
    innerData.shared = SHExposedTypesInfo(_innerExposedVariables);
    _contents.compose(innerData);

    mergeIntoExposedInfo(_exposedVariables, _contents.composeResult().exposedInfo);

    // Merge required, but without the context variables
    for (auto &required : _contents.composeResult().requiredInfo) {
      std::string_view varName(required.name);
      if (varName == GraphicsContext::VariableName || varName == GraphicsRendererContext::VariableName)
        continue;
      _requiredVariables.push_back(required);
    }

    return _contents.composeResult().outputType;
  }

  void warmup(SHContext *context) {
    renderer.warmup(context);
    renderer._ignoreCompilationErrors = (bool)*_ignoreCompilationErrors;
    PARAM_WARMUP(context);
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    renderer.cleanup(context);
  }

  void activate(SHContext *shContext, const SHVar &input) {
    auto &windowContext = varAsObjectChecked<shards::WindowContext>(_window.get(), shards::WindowContext::Type);

    SHVar tmpOutput{};

    if (renderer._graphicsContext.renderer) {
      Var &debug = (Var &)_debug.get();
      renderer._graphicsContext.renderer->setDebug(debug.isNone() ? false : (bool)debug);
    }

    if (renderer.begin(shContext, windowContext)) {
      _contents.activate(shContext, input, tmpOutput);
      renderer.end();
    }
  }
};

struct EndFrame {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The input to this shard is passed through as its output."); }
  static SHOptionalString help() {
    return SHCCSTR("Explicitly end frame rendering, this is done automatically inside MainWindow. This shards is only needed "
                   "when you want to end a frame earlier inside MainWindow");
  }

  PARAM_IMPL();

  RequiredGraphicsContext _requiredGraphicsContext;

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    _requiredVariables.push_back(RequiredGraphicsContext::getExposedTypeInfo());
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _requiredGraphicsContext.warmup(context);
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    _requiredGraphicsContext.cleanup();
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    endFrame(_requiredGraphicsContext);
    return input;
  }
};

struct ViewportShard {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::Int4Type; }
  static SHOptionalString help() { return SHCCSTR(""); }

  // PARAM_PARAMVAR(_someParam, "RenameThis", "AddDescriptionHere", {shards::CoreInfo::AnyType});
  PARAM_IMPL();

  RequiredGraphicsContext _requiredGraphicsContext;

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    _requiredVariables.push_back(RequiredGraphicsContext::getExposedTypeInfo());
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _requiredGraphicsContext.warmup(context);
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    _requiredGraphicsContext.cleanup();
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto vs = _requiredGraphicsContext->renderer->getViewStack().getOutput();
    SHVar out{.valueType = SHType::Int4};
    auto &i4out = out.payload.int4Value;
    if (vs.viewport) {
      i4out[0] = vs.viewport->x;
      i4out[1] = vs.viewport->y;
      i4out[2] = i4out[0] + vs.viewport->width;
      i4out[3] = i4out[1] + vs.viewport->height;
    } else {
      i4out[0] = 0;
      i4out[1] = 0;
      i4out[2] = vs.size.x;
      i4out[3] = vs.size.y;
    }
    return out;
  }
};

void registerRendererShards() {
  REGISTER_SHARD("GFX.Renderer", RendererShard);
  REGISTER_SHARD("GFX.EndFrame", EndFrame);
  REGISTER_SHARD("GFX.Viewport", ViewportShard);
}
} // namespace gfx
