#include "gfx.hpp"
#include "window.hpp"
#include <gfx/renderer.hpp>
#include <gfx/context.hpp>
#include <gfx/loop.hpp>
#include <gfx/window.hpp>
#include <gfx/error_utils.hpp>
#include <shards/core/params.hpp>

using namespace shards;

namespace gfx {
void endFrame(GraphicsContext &ctx) {
  if (!ctx.frameInProgress)
    return;

  ctx.frameInProgress = false;
  ctx.renderer->endFrame();
  ctx.context->endFrame();
}

struct RendererShard {
  static const inline Type WindowType = WindowContext::Type;

  PARAM(ParamVar, _window, "Window", "The window to run the renderer on.", {Type::VariableOf(WindowType)});
  PARAM(ShardsVar, _contents, "Contents", "The main input loop of this window.", {CoreInfo::ShardsOrNone});
  PARAM(OwnedVar, _ignoreCompilationErrors, "IgnoreCompilationErrors",
        "When enabled, shader or pipeline compilation errors will be ignored and either use fallback rendering or not "
        "render at all.",
        {CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_window), PARAM_IMPL_FOR(_contents), PARAM_IMPL_FOR(_ignoreCompilationErrors));

  static inline Type OutputType = Type(WindowContext::Type);

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }

  RendererShard() { _ignoreCompilationErrors = Var(false); }

  GraphicsContext _graphicsContext;
  SHVar *_graphicsContextVar{};

  GraphicsRendererContext _graphicsRendererContext;
  SHVar *_graphicsRendererContextVar{};

  ExposedInfo _innerExposedVariables;
  ExposedInfo _exposedVariables;

  gfx::Loop _loop;

  SHExposedTypesInfo exposedVariables() { return SHExposedTypesInfo(_exposedVariables); }

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    _exposedVariables.clear();

    // Require extra stack space for the wire containing the renderer
    data.wire->stackSize = std::max<size_t>(data.wire->stackSize, 4 * 1024 * 1024);

    if (_window.isNone())
      throw formatException("Window parameter is required but not set");

    // Make sure that renderers are UNIQUE
    for (uint32_t i = 0; i < data.shared.len; i++) {
      if (strcmp(data.shared.elements[i].name, GraphicsContext::VariableName) == 0) {
        throw SHException("GFX.Renderer must be unique, found another use!");
      }
    }

    _innerExposedVariables = ExposedInfo(data.shared);
    _innerExposedVariables.push_back(GraphicsContext::VariableInfo);
    _innerExposedVariables.push_back(GraphicsRendererContext::VariableInfo);

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

  void initRenderer(const std::shared_ptr<Window> &window) {
    _graphicsContext.window = window;

    ContextCreationOptions contextOptions = {};
    _graphicsContext.context = std::make_shared<Context>();
    _graphicsContext.context->init(*_graphicsContext.window.get(), contextOptions);

    _graphicsContext.renderer = std::make_shared<Renderer>(*_graphicsContext.context.get());
    _graphicsContext.renderer->setIgnoreCompilationErrors((bool)Var(_ignoreCompilationErrors));
    _graphicsRendererContext.renderer = _graphicsContext.renderer.get();
  }

  void warmup(SHContext *context) {
    _graphicsContextVar = referenceVariable(context, GraphicsContext::VariableName);
    assignVariableValue(*_graphicsContextVar, Var::Object(&_graphicsContext, GraphicsContext::Type));

    _graphicsRendererContextVar = referenceVariable(context, GraphicsRendererContext::VariableName);
    assignVariableValue(*_graphicsRendererContextVar, Var::Object(&_graphicsRendererContext, GraphicsRendererContext::Type));

    PARAM_WARMUP(context);
  }

  void cleanup() {
    PARAM_CLEANUP();

    if (_graphicsContextVar) {
      if (_graphicsContextVar->refcount > 1) {
        SHLOG_ERROR("MainWindow: Found {} dangling reference(s) to {}", _graphicsContextVar->refcount - 1,
                    GraphicsContext::VariableName);
      }
      releaseVariable(_graphicsContextVar);
    }

    if (_graphicsRendererContextVar) {
      if (_graphicsRendererContextVar->refcount > 1) {
        SHLOG_ERROR("MainWindow: Found {} dangling reference(s) to {}", _graphicsRendererContextVar->refcount - 1,
                    GraphicsRendererContext::VariableName);
      }
      releaseVariable(_graphicsRendererContextVar);
    }

    _graphicsContext = GraphicsContext{};
    _graphicsRendererContext = GraphicsRendererContext{};
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    // Need to lazily init since we depend on renderer
    if (!_graphicsContext.context) {
      auto &windowContext = varAsObjectChecked<WindowContext>(_window.get(), WindowContext::Type);
      initRenderer(windowContext.window);
    }

    auto &window = _graphicsContext.window;
    if (!window->isInitialized()) {
      SHLOG_WARNING("Failed to render to surface, window is closed. Frame skipped.");
      return SHVar{};
    }

    auto &renderer = _graphicsRendererContext.renderer;
    auto &context = _graphicsContext.context;

    gfx::int2 windowSize = window->getDrawableSize();
    try {
      context->resizeMainOutputConditional(windowSize);
    } catch (std::exception &err) {
      SHLOG_WARNING("Swapchain creation failed: {}. Frame skipped.", err.what());
      return SHVar{};
    }

    float deltaTime = 0.0;
    _loop.beginFrame(0.0f, deltaTime);
    if (context->beginFrame()) {
      _graphicsContext.time = _loop.getAbsoluteTime();
      _graphicsContext.deltaTime = deltaTime;
      renderer->beginFrame();

      SHVar tmpOutput{};
      _contents.activate(shContext, input, tmpOutput);

      renderer->endFrame();
      context->endFrame();
    }

    // window->pollEvents(events);
    // for (auto &event : events) {
    //   if (event.type == SDL_QUIT) {
    //     throw ActivationError("Window closed, aborting wire.");
    //   } else if (event.type == SDL_WINDOWEVENT) {
    //     if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
    //       throw ActivationError("Window closed, aborting wire.");
    //     } else if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
    //       gfx::int2 newSize = window->getDrawableSize();
    //       // context->resizeMainOutputConditional(newSize);
    //     }
    //   } else if (event.type == SDL_APP_DIDENTERBACKGROUND) {
    //     // context->suspend();
    //   } else if (event.type == SDL_APP_DIDENTERFOREGROUND) {
    //     // context->resume();
    //   }
    // }

    return SHVar{};
  }
};

// struct EndFrame {
//   static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
//   static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
//   static SHOptionalString outputHelp() { return SHCCSTR("The input to this shard is passed through as its output."); }
//   static SHOptionalString help() {
//     return SHCCSTR("Explicitly end frame rendering, this is done automatically inside MainWindow. This shards is only needed "
//                    "when you want to end a frame earlier inside MainWindow");
//   }

//   //     {"Contents", SHCCSTR("The main input loop of this window."), {CoreInfo::ShardsOrNone}},
//   //     // {"Debug",
//   //     //  SHCCSTR("If the device backing the window should be created with "
//   //     //          "debug layers on."),
//   //     //  {CoreInfo::BoolType}},
//   //     // {"IgnoreCompilationErrors",
//   //     //  SHCCSTR("When enabled, shader or pipeline compilation errors will be ignored and either use fallback rendering or
//   not
//   //     "
//   //     //          "render at all."),
//   //     //  {CoreInfo::BoolType}},

//   PARAM_IMPL();

//   RequiredWindowContext _requiredWindowContext;

//   PARAM_REQUIRED_VARIABLES();
//   SHTypeInfo compose(SHInstanceData &data) {
//     PARAM_COMPOSE_REQUIRED_VARIABLES(data);
//     _requiredVariables.push_back(RequiredGraphicsContext::getExposedTypeInfo());
//     return outputTypes().elements[0];
//   }

//   void warmup(SHContext *context) {
//     PARAM_WARMUP(context);
//     _requiredWindowContext.warmup(context);
//   }

//   void cleanup() {
//     PARAM_CLEANUP();
//     _requiredWindowContext.cleanup();
//   }

//   SHVar activate(SHContext *shContext, const SHVar &input) {
//     // endFrame(_requiredWindowContext);
//     return input;
//   }
// };

void registerRendererShards() {
  REGISTER_SHARD("GFX.Renderer", RendererShard);
  // REGISTER_SHARD("GFX.EndFrame", EndFrame);
}

} // namespace gfx