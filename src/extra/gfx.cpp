#include "gfx.hpp"
#include "gfx/buffer_vars.hpp"
#include <gfx/context.hpp>
#include <gfx/loop.hpp>
#include <gfx/math.hpp>
#include <gfx/mesh.hpp>
#include <gfx/renderer.hpp>
#include <gfx/view.hpp>
#include <gfx/window.hpp>
#include <linalg_shim.hpp>
#include <magic_enum.hpp>

using namespace shards;

namespace gfx {
using shards::Mat4;

struct DrawablePassShard {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return Types::PipelineStep; }

  static inline Parameters params{
      {"Features", SHCCSTR("Features to use."), {Type::VariableOf(Types::PipelineStepSeq), Types::PipelineStepSeq}},
      {"Queue", SHCCSTR("The queue to draw from (optional). Uses the default queue if not specified"), {Types::DrawQueue}},
  };
  static SHParametersInfo parameters() { return params; }

  PipelineStepPtr *_step{};
  ParamVar _features{};
  ParamVar _queueVar{};

  std::vector<FeaturePtr> _collectedFeatures;
  std::vector<Var> _featureVars;

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _features = value;
      break;
    case 1:
      _queueVar = value;
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _features;
    case 1:
      return _queueVar;
    default:
      return Var::Empty;
    }
  }

  void cleanup() {
    if (_step) {
      Types::PipelineStepObjectVar.Release(_step);
      _step = nullptr;
    }
    _queueVar.cleanup();
    _features.cleanup();
  }

  void warmup(SHContext *context) {
    _step = Types::PipelineStepObjectVar.New();
    _features.warmup(context);
    _queueVar.warmup(context);
  }

  std::vector<FeaturePtr> collectFeatures(const SHVar &input) {
    _collectedFeatures.clear();

    Var featuresVar(_features.get());
    featuresVar.intoVector(_featureVars);

    for (auto &featureVar : _featureVars) {
      if (featureVar.payload.objectValue) {
        _collectedFeatures.push_back(*(FeaturePtr *)featureVar.payload.objectValue);
      }
    }

    _featureVars.clear();

    return _collectedFeatures;
  }

  DrawQueuePtr getDrawQueue() {
    SHVar queueVar = _queueVar.get();
    if (queueVar.payload.objectValue) {
      return ((SHDrawQueue *)queueVar.payload.objectValue)->queue;
    } else {
      return DrawQueuePtr();
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    *_step = makeDrawablePipelineStep(RenderDrawablesStep{
        .drawQueue = getDrawQueue(),
        .features = collectFeatures(_features),
    });
    return Types::PipelineStepObjectVar.Get(_step);
  }
};

void SHView::updateVariables() {
  if (viewTransformVar.isVariable()) {
    view->view = shards::Mat4(viewTransformVar.get());
  }
}

struct ViewShard {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return Types::View; }

  static inline Parameters params{
      {"View", SHCCSTR("The view matrix."), {Type::VariableOf(CoreInfo::Float4x4Type), CoreInfo::Float4x4Type}},
  };
  static SHParametersInfo parameters() { return params; }

  ParamVar _viewTransform;
  SHView *_view;

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _viewTransform = value;
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _viewTransform;
    default:
      return Var::Empty;
    }
  }

  void cleanup() {
    if (_view) {
      Types::ViewObjectVar.Release(_view);
      _view = nullptr;
    }
    _viewTransform.cleanup();
  }

  void warmup(SHContext *context) {
    _view = Types::ViewObjectVar.New();
    _viewTransform.warmup(context);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    ViewPtr view = _view->view = std::make_shared<View>();
    if (_viewTransform->valueType != SHType::None) {
      view->view = shards::Mat4(_viewTransform.get());
    }

    // TODO: Add projection/viewport override params
    view->proj = ViewPerspectiveProjection{};

    if (_viewTransform.isVariable()) {
      _view->viewTransformVar = (SHVar &)_viewTransform;
      _view->viewTransformVar.warmup(context);
    }
    return Types::ViewObjectVar.Get(_view);
  }
};

struct RenderShard : public BaseConsumer {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static inline Parameters params{
      {"Steps", SHCCSTR("Render steps to follow."), {Type::VariableOf(Types::PipelineStepSeq), Types::PipelineStepSeq}},
      {"View", SHCCSTR("The view to render into."), {Type::VariableOf(Types::View), Types::View}},
      {"Views", SHCCSTR("The views to render into."), {Type::VariableOf(Types::ViewSeq), Types::ViewSeq}},
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
      SHView *shView = (SHView *)var.payload.objectValue;
      if (!shView)
        throw formatException("GFX.Render View is not defined");

      shView->updateVariables();
      _collectedViews.push_back(shView->view);
    } else if (_views->valueType != SHType::None) {
      SeqVar &seq = (SeqVar &)_views.get();
      for (size_t i = 0; i < seq.size(); i++) {
        const SHVar &viewVar = seq[i];
        SHView *shView = (SHView *)viewVar.payload.objectValue;
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
              arg.drawQueue = getMainWindowGlobals().drawQueue;
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
      PipelineStepPtr *ptr = (PipelineStepPtr *)stepVar.payload.objectValue;
      if (!ptr)
        throw formatException("GFX.Render PipelineStep[{}] is not defined", index);

      fixupPipelineStep(*ptr->get());

      _collectedPipelineSteps.push_back(*ptr);
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
namespace shader {
extern void registerTranslatorShards();
}
void registerShards() {
  registerMainWindowShards();
  registerMeshShards();
  registerDrawableShards();
  registerMaterialShards();
  registerFeatureShards();
  registerGLTFShards();
  shader::registerTranslatorShards();

  REGISTER_SHARD("GFX.DrawablePass", DrawablePassShard);
  REGISTER_SHARD("GFX.View", ViewShard);
  REGISTER_SHARD("GFX.Render", RenderShard);
}
} // namespace gfx
