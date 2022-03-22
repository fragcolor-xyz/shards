#include "gfx.hpp"
#include "gfx/buffer_vars.hpp"
#include <gfx/context.hpp>
#include <gfx/features/base_color.hpp>
#include <gfx/features/debug_color.hpp>
#include <gfx/features/transform.hpp>
#include <gfx/loop.hpp>
#include <gfx/math.hpp>
#include <gfx/mesh.hpp>
#include <gfx/renderer.hpp>
#include <gfx/view.hpp>
#include <gfx/window.hpp>
#include <linalg_shim.hpp>
#include <magic_enum.hpp>

using namespace chainblocks;

namespace gfx {
using chainblocks::Mat4;

struct BuiltinFeatureBlock {
  enum class Id {
    Transform,
    BaseColor,
    VertexColorFromNormal,
  };

  static constexpr uint32_t IdTypeId = 'feid';
  static inline chainblocks::Type IdType = chainblocks::Type::Enum(VendorId, IdTypeId);
  static inline chainblocks::EnumInfo<Id> IdEnumInfo{"BuiltinFeatureId", VendorId, IdTypeId};

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return FeatureType; }

  static inline Parameters params{{"Id", CBCCSTR("Builtin feature id."), {IdType}}};
  static CBParametersInfo parameters() { return params; }

  Id _id{};
  FeaturePtr *_feature{};

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _id = Id(value.payload.enumValue);
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var::Enum(_id, VendorId, IdTypeId);
    default:
      return Var::Empty;
    }
  }

  void cleanup() {
    if (_feature) {
      FeatureObjectVar.Release(_feature);
      _feature = nullptr;
    }
  }

  void warmup(CBContext *context) {
    _feature = FeatureObjectVar.New();
    switch (_id) {
    case Id::Transform:
      *_feature = features::Transform::create();
      break;
    case Id::BaseColor:
      *_feature = features::BaseColor::create();
      break;
    case Id::VertexColorFromNormal:
      *_feature = features::DebugColor::create("normal", ProgrammableGraphicsStage::Vertex);
      break;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) { return FeatureObjectVar.Get(_feature); }
};

struct DrawablePassBlock {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return PipelineStepType; }

  static inline Type PipelineStepSeqType = Type::SeqOf(PipelineStepType);
  static inline Parameters params{
      {"Features", CBCCSTR("Features to use."), {Type::VariableOf(PipelineStepSeqType), PipelineStepSeqType}},
  };
  static CBParametersInfo parameters() { return params; }

  PipelineStepPtr *_step{};
  ParamVar _features{};

  std::vector<FeaturePtr> _collectedFeatures;
  std::vector<Var> _featureVars;

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _features = value;
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _features;
    default:
      return Var::Empty;
    }
  }

  void cleanup() {
    if (_step) {
      PipelineStepObjectVar.Release(_step);
      _step = nullptr;
    }
    _features.cleanup();
  }

  void warmup(CBContext *context) {
    _step = PipelineStepObjectVar.New();
    _features.warmup(context);
  }

  std::vector<FeaturePtr> collectFeatures(const CBVar &input) {
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

  CBVar activate(CBContext *context, const CBVar &input) {
    *_step = makeDrawablePipelineStep(RenderDrawablesStep{
        .features = collectFeatures(_features),
    });
    return PipelineStepObjectVar.Get(_step);
  }
};

void CBView::updateVariables() {
  if (viewTransformVar.isVariable()) {
    view->view = chainblocks::Mat4(viewTransformVar.get());
  }
}

struct ViewBlock {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return ViewType; }

  static inline Type PipelineStepSeqType = Type::SeqOf(PipelineStepType);
  static inline Parameters params{
      {"View", CBCCSTR("The view matrix."), {Type::VariableOf(CoreInfo::Float4x4Type), CoreInfo::Float4x4Type}},
  };
  static CBParametersInfo parameters() { return params; }

  ParamVar _viewTransform;
  CBView *_view;

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _viewTransform = value;
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _viewTransform;
    default:
      return Var::Empty;
    }
  }

  void cleanup() {
    if (_view) {
      ViewObjectVar.Release(_view);
      _view = nullptr;
    }
    _viewTransform.cleanup();
  }

  void warmup(CBContext *context) {
    _view = ViewObjectVar.New();
    _viewTransform.warmup(context);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    ViewPtr view = _view->view = std::make_shared<View>();
    if (_viewTransform->valueType != CBType::None) {
      view->view = chainblocks::Mat4(_viewTransform.get());
    }

    // TODO: Add projection/viewport override params
    view->proj = ViewPerspectiveProjection{};
    // view->viewport

    if (_viewTransform.isVariable()) {
      _view->viewTransformVar = (CBVar &)_viewTransform;
      _view->viewTransformVar.warmup(context);
    }
    return ViewObjectVar.Get(_view);
  }
};

struct RenderBlock : public BaseConsumer {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static inline Type PipelineStepSeqType = Type::SeqOf(PipelineStepType);
  static inline Parameters params{
      {"Steps", CBCCSTR("Render steps to follow."), {Type::VariableOf(PipelineStepSeqType), PipelineStepSeqType}},
      {"View", CBCCSTR("The view to render into."), {Type::VariableOf(ViewType), ViewType}},
      {"Views", CBCCSTR("The views to render into."), {Type::VariableOf(ViewSeqType), ViewSeqType}},
  };
  static CBParametersInfo parameters() { return params; }

  ParamVar _steps{};
  ParamVar _view{};
  ParamVar _views{};
  ViewPtr defaultView;
  std::vector<ViewPtr> views;

  void setParam(int index, const CBVar &value) {
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

  CBVar getParam(int index) {
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
    defaultView.reset();
  }

  void warmup(CBContext *context) {
    baseConsumerWarmup(context);
    _steps.warmup(context);
    _view.warmup(context);
    _views.warmup(context);
  }

  ViewPtr getDefaultView() {
    if (!defaultView)
      defaultView = std::make_shared<View>();
    return defaultView;
  }

  void validateViewParameters() {
    if (_view->valueType != CBType::None && _views->valueType != CBType::None) {
      throw ComposeError("GFX.Render :View and :Views can not be set at the same time");
    }
  }

  CBTypeInfo compose(CBInstanceData &data) {
    composeCheckMainThread(data);
    validateViewParameters();
    return CoreInfo::NoneType;
  }

  const std::vector<ViewPtr> &updateAndCollectViews() {
    views.clear();
    if (_view->valueType != CBType::None) {
      const CBVar &var = _view.get();
      CBView *cbView = (CBView *)var.payload.objectValue;
      cbView->updateVariables();
      views.push_back(cbView->view);
    } else if (_views->valueType != CBType::None) {
      SeqVar &seq = (SeqVar &)_views.get();
      for (size_t i = 0; i < seq.size(); i++) {
        const CBVar &viewVar = seq[i];
        CBView *cbView = (CBView *)viewVar.payload.objectValue;
        cbView->updateVariables();
        views.push_back(cbView->view);
      }
    } else {
      views.push_back(getDefaultView());
    }
    return views;
  }

  PipelineSteps collectPipelineSteps() {
    PipelineSteps steps;
    Var stepsCBVar(_steps.get());
    std::vector<Var> stepVars(stepsCBVar);
    for (const auto &stepVar : stepVars) {
      steps.push_back(*(PipelineStepPtr *)stepVar.payload.objectValue);
    }
    return steps;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    MainWindowGlobals &globals = getMainWindowGlobals();
    globals.renderer->render(globals.drawQueue, updateAndCollectViews(), collectPipelineSteps());
    return CBVar{};
  }
};

extern void registerMainWindowBlocks();
extern void registerMeshBlocks();
extern void registerDrawableBlocks();
extern void registerMaterialBlocks();
void registerBlocks() {
  registerMainWindowBlocks();
  registerMeshBlocks();
  registerDrawableBlocks();
  registerMaterialBlocks();

  REGISTER_CBLOCK("GFX.BuiltinFeature", BuiltinFeatureBlock);
  REGISTER_CBLOCK("GFX.DrawablePass", DrawablePassBlock);
  REGISTER_CBLOCK("GFX.View", ViewBlock);
  REGISTER_CBLOCK("GFX.Render", RenderBlock);
}
} // namespace gfx
