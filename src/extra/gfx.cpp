#include "gfx.hpp"
#include "extra/gfx.hpp"
#include "extra/gfx/shards_utils.hpp"
#include "gfx/buffer_vars.hpp"
#include "shards.h"
#include <gfx/context.hpp>
#include <gfx/loop.hpp>
#include <gfx/math.hpp>
#include <gfx/mesh.hpp>
#include <gfx/renderer.hpp>
#include <gfx/window.hpp>
#include <gfx/view.hpp>
#include <gfx/steps/defaults.hpp>
#include <foundation.hpp>
#include <magic_enum.hpp>
#include <exposed_type_utils.hpp>
#include <params.hpp>

using namespace shards;

namespace gfx {

Context &GraphicsContext::getContext() { return *context.get(); }
Window &GraphicsContext::getWindow() { return *window.get(); }
SDL_Window *GraphicsContext::getSdlWindow() { return getWindow().window; }

struct RenderShard {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }

  static inline Parameters params{
      {"Steps", SHCCSTR("Render steps to follow."), {Type::VariableOf(Types::PipelineStepSeq), Types::PipelineStepSeq}},
      {"View", SHCCSTR("The view to render into. (Optional)"), {CoreInfo::NoneType, Type::VariableOf(Types::View)}},
      {"Views",
       SHCCSTR("The views to render into. (Optional)"),
       {CoreInfo::NoneType, Type::VariableOf(Types::ViewSeq), Types::ViewSeq}},
      {"ClearedHint",
       SHCCSTR("Hints about already cleared views. Unsupported, do not use!"),
       {CoreInfo::NoneType, Type::SeqOf(CoreInfo::StringType)}},
  };
  static SHParametersInfo parameters() { return params; }

  OptionalGraphicsContext _graphicsContext;
  RequiredGraphicsRendererContext _graphicsRendererContext;
  ParamVar _steps{};
  ParamVar _view{};
  ParamVar _views{};
  OwnedVar _clearedHint;
  ViewPtr _defaultView;
  std::vector<ViewPtr> _collectedViews;
  std::vector<Var> _collectedPipelineStepVars;
  PipelineSteps _collectedPipelineSteps;

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _steps = value;
      break;
    case 1:
      _view = value;
      break;
    case 2:
      _views = value;
      break;
    case 3:
      _clearedHint = value;
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _steps;
    case 1:
      return _view;
    case 2:
      return _views;
    case 3:
      return _clearedHint;
    default:
      return Var::Empty;
    }
  }

  ExposedInfo _exposed;
  SHExposedTypesInfo requiredVariables() {
    _exposed.clear();
    _exposed.push_back(decltype(_graphicsRendererContext)::getExposedTypeInfo());
    if (_steps.isVariable()) {
      _exposed.push_back(SHExposedTypeInfo{
          .name = _steps.variableName(),
          .exposedType = Types::PipelineStepSeq,
      });
    }
    if (_view.isVariable()) {
      _exposed.push_back(SHExposedTypeInfo{
          .name = _view.variableName(),
          .exposedType = Types::View,
      });
    }
    if (_views.isVariable()) {
      _exposed.push_back(SHExposedTypeInfo{
          .name = _views.variableName(),
          .exposedType = Types::ViewSeq,
      });
    }
    return (SHExposedTypesInfo)_exposed;
  }

  void cleanup() {
    _graphicsRendererContext.cleanup();
    _graphicsContext.cleanup();
    _steps.cleanup();
    _view.cleanup();
    _views.cleanup();
    _defaultView.reset();
  }

  void warmup(SHContext *context) {
    _graphicsRendererContext.warmup(context);
    _graphicsContext.warmup(context);
    _steps.warmup(context);
    _view.warmup(context);
    _views.warmup(context);
  }

  ViewPtr getDefaultView() {
    if (!_defaultView)
      _defaultView = std::make_shared<View>();
    return _defaultView;
  }

  void validateViewParameters() {
    if (_view->valueType != SHType::None && _views->valueType != SHType::None) {
      throw ComposeError("GFX.Render :View and :Views can not be set at the same time");
    }
  }

  SHTypeInfo compose(SHInstanceData &data) {
    composeCheckGfxThread(data);
    validateViewParameters();
    return CoreInfo::NoneType;
  }

  const std::vector<ViewPtr> &updateAndCollectViews() {
    _collectedViews.clear();
    Var &viewVar = (Var &)_view.get();
    Var &viewsVar = (Var &)_views.get();
    if (!viewVar.isNone()) {
      SHView *shView = varAsObjectChecked<SHView>(viewVar, Types::View);
      if (!shView)
        throw formatException("Specified view to GFX.Render is invalid");

      shView->updateVariables();
      _collectedViews.push_back(shView->view);
    } else if (!viewsVar.isNone()) {
      SeqVar &seq = (SeqVar &)viewsVar;
      for (size_t i = 0; i < seq.size(); i++) {
        SHView *shView = varAsObjectChecked<SHView>(seq[i], Types::View);
        if (!shView)
          throw formatException("Specified view {} to GFX.Render is invalid", i);

        shView->updateVariables();
        _collectedViews.push_back(shView->view);
      }
    } else {
      _collectedViews.push_back(getDefaultView());
    }
    return _collectedViews;
  }

  // TODO(guusw): Remove the global draw queue
  void fixupPipelineStep(PipelineStep &step) {
    std::visit(
        [&](auto &&arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, RenderDrawablesStep>) {
            if (!arg.drawQueue && _graphicsContext)
              arg.drawQueue = _graphicsContext->getDrawQueue();
          }
        },
        step);
  }

  PipelineSteps collectPipelineSteps() {
    try {
      _collectedPipelineSteps.clear();

      Var stepsSHVar(_steps.get());
      checkType(stepsSHVar.valueType, SHType::Seq, "Render steps parameter");

      stepsSHVar.intoVector(_collectedPipelineStepVars);

      size_t index = 0;
      for (const auto &stepVar : _collectedPipelineStepVars) {
        if (!stepVar.payload.objectValue)
          throw formatException("GFX.Render PipelineStep[{}] is not defined", index);

        PipelineStepPtr &ptr = *reinterpret_cast<PipelineStepPtr *>(stepVar.payload.objectValue);
        fixupPipelineStep(*ptr.get());

        _collectedPipelineSteps.push_back(ptr);
        index++;
      }

      return _collectedPipelineSteps;
    } catch (...) {
      SHLOG_ERROR("Failed to collect pipeline steps");
      throw;
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (_graphicsRendererContext->render) {
      _graphicsRendererContext->render(updateAndCollectViews(), collectPipelineSteps());
    } else {
      RendererHacks hacks;
      if (_clearedHint.valueType != SHType::None) {
        for (auto &val : _clearedHint) {
          hacks.clearedHints.emplace_back(val.payload.stringValue);
        }
      }

      _graphicsRendererContext->renderer->render(updateAndCollectViews(), collectPipelineSteps(), hacks);
    }
    return SHVar{};
  }
};

extern void registerMainWindowShards();
extern void registerMeshShards();
extern void registerDrawableShards();
extern void registerMaterialShards();
extern void registerFeatureShards();
extern void registerGLTFShards();
extern void registerCameraShards();
extern void registerTextureShards();
extern void registerViewShards();
extern void registerRenderStepShards();
namespace shader {
extern void registerTranslatorShards();
}
void registerShards() {
  REGISTER_ENUM(Types::WindingOrderEnumInfo);
  REGISTER_ENUM(Types::ShaderFieldBaseTypeEnumInfo);
  REGISTER_ENUM(Types::ProgrammableGraphicsStageEnumInfo);
  REGISTER_ENUM(Types::DependencyTypeEnumInfo);
  REGISTER_ENUM(Types::BlendFactorEnumInfo);
  REGISTER_ENUM(Types::BlendOperationEnumInfo);
  REGISTER_ENUM(Types::FilterModeEnumInfo);
  REGISTER_ENUM(Types::CompareFunctionEnumInfo);
  REGISTER_ENUM(Types::ColorMaskEnumInfo);
  REGISTER_ENUM(Types::TextureTypeEnumInfo);
  REGISTER_ENUM(Types::SortModeEnumInfo);
  REGISTER_ENUM(Types::TextureFormatEnumInfo);
  REGISTER_ENUM(Types::TextureDimensionEnumInfo);

  registerMainWindowShards();
  registerMeshShards();
  registerDrawableShards();
  registerMaterialShards();
  registerFeatureShards();
  registerGLTFShards();
  registerCameraShards();
  registerTextureShards();
  registerViewShards();
  registerRenderStepShards();
  shader::registerTranslatorShards();
  REGISTER_SHARD("GFX.Render", RenderShard);
}
} // namespace gfx
