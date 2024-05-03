#include "gfx.hpp"
#include "shards/core/module.hpp"
#include "shards_utils.hpp"
#include "buffer_vars.hpp"
#include <shards/shards.h>
#include <gfx/context.hpp>
#include <gfx/loop.hpp>
#include <gfx/math.hpp>
#include <gfx/mesh.hpp>
#include <gfx/renderer.hpp>
#include <gfx/window.hpp>
#include <gfx/view.hpp>
#include <gfx/steps/defaults.hpp>
#include <shards/core/foundation.hpp>
#include <magic_enum.hpp>
#include <shards/core/exposed_type_utils.hpp>
#include <shards/core/params.hpp>

using namespace shards;

namespace gfx {

Context &GraphicsContext::getContext() { return *context.get(); }
Window &GraphicsContext::getWindow() { return *window.get(); }
// SDL_Window *GraphicsContext::getSdlWindow() { return getWindow().window; }

struct RenderShard {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }

  PARAM_PARAMVAR(_steps, "Steps", "Render steps to follow.", {Type::VariableOf(ShardsTypes::PipelineStepSeq), ShardsTypes::PipelineStepSeq});
  PARAM_PARAMVAR(_view, "View", "The view to render. (Optional)", {CoreInfo::NoneType, Type::VariableOf(ShardsTypes::View)});
  PARAM_IMPL(PARAM_IMPL_FOR(_steps), PARAM_IMPL_FOR(_view));

  OptionalGraphicsContext _graphicsContext;
  RequiredGraphicsRendererContext _graphicsRendererContext;
  ViewPtr _defaultView;
  std::vector<ViewPtr> _collectedViews;
  std::vector<Var> _collectedPipelineStepVars;
  PipelineSteps _collectedPipelineSteps;

  PARAM_REQUIRED_VARIABLES();

  void cleanup(SHContext* context) {
    _graphicsRendererContext.cleanup(context);
    _graphicsContext.cleanup(context);
    _defaultView.reset();
    PARAM_CLEANUP(context);
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _graphicsRendererContext.warmup(context);
    _graphicsContext.warmup(context);
  }

  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    _requiredVariables.push_back(decltype(_graphicsRendererContext)::getExposedTypeInfo());

    composeCheckGfxThread(data);

    return CoreInfo::NoneType;
  }

  const ViewPtr &getDefaultView() {
    if (!_defaultView)
      _defaultView = std::make_shared<View>();
    return _defaultView;
  }

  const ViewPtr &getView() {
    Var &viewVar = (Var &)_view.get();
    if (!viewVar.isNone()) {
      SHView &shView = varAsObjectChecked<SHView>(viewVar, ShardsTypes::View);
      return shView.view;
    } else {
      return getDefaultView();
    }
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
      _graphicsRendererContext->render(getView(), collectPipelineSteps());
    } else {
      _graphicsRendererContext->renderer->render(getView(), collectPipelineSteps());
    }
    return SHVar{};
  }
};

extern void registerMainWindowShards();
extern void registerRendererShards();
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
} // namespace gfx

SHARDS_REGISTER_FN(gfx) {
  using namespace gfx;
  REGISTER_ENUM(ShardsTypes::WindingOrderEnumInfo);
  REGISTER_ENUM(ShardsTypes::ShaderFieldBaseTypeEnumInfo);
  REGISTER_ENUM(ShardsTypes::ProgrammableGraphicsStageEnumInfo);
  REGISTER_ENUM(ShardsTypes::DependencyTypeEnumInfo);
  REGISTER_ENUM(ShardsTypes::BlendFactorEnumInfo);
  REGISTER_ENUM(ShardsTypes::BlendOperationEnumInfo);
  REGISTER_ENUM(ShardsTypes::FilterModeEnumInfo);
  REGISTER_ENUM(ShardsTypes::CompareFunctionEnumInfo);
  REGISTER_ENUM(ShardsTypes::ColorMaskEnumInfo);
  REGISTER_ENUM(ShardsTypes::TextureTypeEnumInfo);
  REGISTER_ENUM(ShardsTypes::SortModeEnumInfo);
  REGISTER_ENUM(ShardsTypes::TextureFormatEnumInfo);
  REGISTER_ENUM(ShardsTypes::TextureDimensionEnumInfo);
  REGISTER_ENUM(ShardsTypes::TextureAddressingEnumInfo);
  REGISTER_ENUM(ShardsTypes::TextureFilteringEnumInfo);
  REGISTER_ENUM(ShardsTypes::OrthographicSizeTypeEnumInfo);
  REGISTER_ENUM(ShardsTypes::BindGroupIdEnumInfo);
  REGISTER_ENUM(ShardsTypes::TextureSampleTypeEnumInfo);
  REGISTER_ENUM(ShardsTypes::BufferAddressSpaceEnumInfo);

  registerMainWindowShards();
  registerRendererShards();
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
