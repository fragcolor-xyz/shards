#include "gfx.hpp"
#include "gfx/buffer_vars.hpp"
#include <gfx/context.hpp>
#include <gfx/loop.hpp>
#include <gfx/math.hpp>
#include <gfx/mesh.hpp>
#include <gfx/renderer.hpp>
#include <gfx/window.hpp>
#include <gfx/view.hpp>
#include <gfx/steps/defaults.hpp>
#include <magic_enum.hpp>
#include <params.hpp>

using namespace shards;

namespace gfx {

struct RenderShard : public BaseConsumer {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }

  static inline Parameters params{
      {"Steps", SHCCSTR("Render steps to follow."), {Type::VariableOf(Types::PipelineStepSeq), Types::PipelineStepSeq}},
      {"View", SHCCSTR("The view to render into. (Optional)"), {CoreInfo::NoneType, Type::VariableOf(Types::View)}},
      {"Views",
       SHCCSTR("The views to render into. (Optional)"),
       {CoreInfo::NoneType, Type::VariableOf(Types::ViewSeq), Types::ViewSeq}},
  };
  static SHParametersInfo parameters() { return params; }

  ParamVar _steps{};
  ParamVar _view{};
  ParamVar _views{};
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
    default:
      return Var::Empty;
    }
  }

  void cleanup() {
    baseConsumerCleanup();
    _steps.cleanup();
    _view.cleanup();
    _views.cleanup();
    _defaultView.reset();
  }

  void warmup(SHContext *context) {
    baseConsumerWarmup(context);
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
    composeCheckMainThread(data);
    validateViewParameters();
    return CoreInfo::NoneType;
  }

  const std::vector<ViewPtr> &updateAndCollectViews() {
    _collectedViews.clear();
    if (_view->valueType != SHType::None) {
      const SHVar &var = _view.get();
      SHView *shView = reinterpret_cast<SHView *>(var.payload.objectValue);
      if (!shView)
        throw formatException("GFX.Render View is not defined");

      shView->updateVariables();
      _collectedViews.push_back(shView->view);
    } else if (_views->valueType != SHType::None) {
      SeqVar &seq = (SeqVar &)_views.get();
      for (size_t i = 0; i < seq.size(); i++) {
        const SHVar &viewVar = seq[i];
        SHView *shView = reinterpret_cast<SHView *>(viewVar.payload.objectValue);
        if (!shView)
          throw formatException("GFX.Render View[{}] is not defined", i);

        shView->updateVariables();
        _collectedViews.push_back(shView->view);
      }
    } else {
      _collectedViews.push_back(getDefaultView());
    }
    return _collectedViews;
  }

  void fixupPipelineStep(PipelineStep &step) {
    std::visit(
        [&](auto &&arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, RenderDrawablesStep>) {
            if (!arg.drawQueue)
              arg.drawQueue = getMainWindowGlobals().getDrawQueue();
          }
        },
        step);
  }

  PipelineSteps collectPipelineSteps() {
    _collectedPipelineSteps.clear();

    Var stepsSHVar(_steps.get());
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
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    MainWindowGlobals &globals = getMainWindowGlobals();
    globals.renderer->render(updateAndCollectViews(), collectPipelineSteps());
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
