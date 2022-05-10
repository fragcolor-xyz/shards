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
  static CBTypesInfo outputTypes() { return Types::Feature; }

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
      Types::FeatureObjectVar.Release(_feature);
      _feature = nullptr;
    }
  }

  void warmup(CBContext *context) {
    _feature = Types::FeatureObjectVar.New();
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

  CBVar activate(CBContext *context, const CBVar &input) { return Types::FeatureObjectVar.Get(_feature); }
};

struct DrawablePassBlock {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return Types::PipelineStep; }

  static inline Parameters params{
      {"Features", CBCCSTR("Features to use."), {Type::VariableOf(Types::PipelineStepSeq), Types::PipelineStepSeq}},
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
      Types::PipelineStepObjectVar.Release(_step);
      _step = nullptr;
    }
    _features.cleanup();
  }

  void warmup(CBContext *context) {
    _step = Types::PipelineStepObjectVar.New();
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
    return Types::PipelineStepObjectVar.Get(_step);
  }
};

void CBView::updateVariables() {
  if (viewTransformVar.isVariable()) {
    view->view = chainblocks::Mat4(viewTransformVar.get());
  }
}

struct ViewBlock {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return Types::View; }

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
      Types::ViewObjectVar.Release(_view);
      _view = nullptr;
    }
    _viewTransform.cleanup();
  }

  void warmup(CBContext *context) {
    _view = Types::ViewObjectVar.New();
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
    return Types::ViewObjectVar.Get(_view);
  }
};

struct RenderBlock : public BaseConsumer {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static inline Parameters params{
      {"Steps", CBCCSTR("Render steps to follow."), {Type::VariableOf(Types::PipelineStepSeq), Types::PipelineStepSeq}},
      {"View", CBCCSTR("The view to render into."), {Type::VariableOf(Types::View), Types::View}},
      {"Views", CBCCSTR("The views to render into."), {Type::VariableOf(Types::ViewSeq), Types::ViewSeq}},
  };
  static CBParametersInfo parameters() { return params; }

  ParamVar _steps{};
  ParamVar _view{};
  ParamVar _views{};
  ViewPtr _defaultView;
  std::vector<ViewPtr> _collectedViews;
  std::vector<Var> _collectedPipelineStepVars;
  PipelineSteps _collectedPipelineSteps;

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
    _defaultView.reset();
  }

  void warmup(CBContext *context) {
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
    _collectedViews.clear();
    if (_view->valueType != CBType::None) {
      const CBVar &var = _view.get();
      CBView *cbView = (CBView *)var.payload.objectValue;
      cbView->updateVariables();
      _collectedViews.push_back(cbView->view);
    } else if (_views->valueType != CBType::None) {
      SeqVar &seq = (SeqVar &)_views.get();
      for (size_t i = 0; i < seq.size(); i++) {
        const CBVar &viewVar = seq[i];
        CBView *cbView = (CBView *)viewVar.payload.objectValue;
        cbView->updateVariables();
        _collectedViews.push_back(cbView->view);
      }
    } else {
      _collectedViews.push_back(getDefaultView());
    }
    return _collectedViews;
  }

  PipelineSteps collectPipelineSteps() {
    _collectedPipelineSteps.clear();

    Var stepsCBVar(_steps.get());
    stepsCBVar.intoVector(_collectedPipelineStepVars);

    for (const auto &stepVar : _collectedPipelineStepVars) {
      _collectedPipelineSteps.push_back(*(PipelineStepPtr *)stepVar.payload.objectValue);
    }

    return _collectedPipelineSteps;
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
