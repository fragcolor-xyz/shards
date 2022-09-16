#include "../gfx.hpp"
#include <gfx/view.hpp>
#include <gfx/renderer.hpp>
#include <gfx/window.hpp>
#include <input/input_stack.hpp>
#include "linalg_shim.hpp"
#include "shards_utils.hpp"

using namespace shards;

namespace gfx {
void SHView::updateVariables() {
  if (viewTransformVar.isVariable()) {
    view->view = shards::Mat4(viewTransformVar.get());
  }
}

struct ViewShard {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return Types::View; }

  static inline Parameters params{
      {"View", SHCCSTR("The view matrix. (Optional)"), {CoreInfo::NoneType, Type::VariableOf(CoreInfo::Float4x4Type)}},
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

struct PushViewShard : public BaseConsumer {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyTableType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }

  static SHParametersInfo parameters() {
    static Parameters params{};
    return params;
  }

  void cleanup() { baseConsumerCleanup(); }

  void warmup(SHContext *context) { baseConsumerWarmup(context); }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    ViewStack::Item viewItem{};
    input::InputStack::Item inputItem;

    // (optional) set viewport rectangle
    auto &inputTable = input.payload.tableValue;
    SHVar viewportVar{};
    if (getFromTable(shContext, inputTable, "Viewport", viewportVar)) {
      auto &v = viewportVar.payload.float4Value;
      // Read Float4 rect as (X0, Y0, X1, Y1)
      viewItem.viewport = Rect::fromCorners(v[0], v[1], v[2], v[3]);
    }

    // (optional) set render target
    SHVar rtVar{};
    if (getFromTable(shContext, inputTable, "Target", rtVar)) {
      SHRenderTarget *renderTarget = varAsObjectChecked<SHRenderTarget>(rtVar, Types::RenderTarget);
      viewItem.renderTarget = renderTarget->renderTarget;
    }

    // (optional) Push window region for input
    SHVar windowRegionVar{};
    if (getFromTable(shContext, inputTable, "WindowRegion", windowRegionVar)) {
      // Read Float4 rect as (X0, Y0, X1, Y1)
      auto &v = windowRegionVar.payload.float4Value;
      int4 region(v[0], v[1], v[2], v[3]);

      // This is used for translating cursor inputs from window to view space
      // TODO: Adjust relative to parent region, for now always assume this is in global window coordinates
      inputItem.windowMapping = input::WindowSubRegion{
          .region = region,
      };

      // Safe to automatically set reference size based on region
      float2 regionSize = float2(region.z - region.x, region.w - region.y);
      viewItem.referenceSize = int2(linalg::floor(float2(regionSize)));
    }

    auto &mwg = getMainWindowGlobals();
    ViewStack &viewStack = mwg.renderer->getViewStack();
    input::InputStack &inputStack = mwg.inputStack;

    viewStack.push(std::move(viewItem));
    inputStack.push(std::move(inputItem));

    return Var::Empty;
  }
};

struct PopViewShard : public BaseConsumer {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }

  static SHParametersInfo parameters() {
    static Parameters params{};
    return params;
  }

  void cleanup() { baseConsumerCleanup(); }
  void warmup(SHContext *context) { baseConsumerWarmup(context); }
  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &mwg = getMainWindowGlobals();
    ViewStack &viewStack = mwg.renderer->getViewStack();
    input::InputStack &inputStack = mwg.inputStack;

    viewStack.pop();
    inputStack.pop();

    return Var::Empty;
  }
};

void registerViewShards() {
  REGISTER_SHARD("GFX.View", ViewShard);
  REGISTER_SHARD("GFX.PushView", PushViewShard);
  REGISTER_SHARD("GFX.PopView", PopViewShard);
}
} // namespace gfx
