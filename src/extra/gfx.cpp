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

  PARAM_PARAMVAR(_steps, "Steps", "Render steps to follow.", {Type::VariableOf(Types::PipelineStepSeq), Types::PipelineStepSeq});
  PARAM_PARAMVAR(_view, "View", "The view to render. (Optional)", {CoreInfo::NoneType, Type::VariableOf(Types::View)});
  PARAM_IMPL(RenderShard, PARAM_IMPL_FOR(_steps), PARAM_IMPL_FOR(_view));

  OptionalGraphicsContext _graphicsContext;
  RequiredGraphicsRendererContext _graphicsRendererContext;
  ViewPtr _defaultView;
  std::vector<ViewPtr> _collectedViews;
  std::vector<Var> _collectedPipelineStepVars;
  PipelineSteps _collectedPipelineSteps;

  PARAM_REQUIRED_VARIABLES();

  void cleanup() {
    _graphicsRendererContext.cleanup();
    _graphicsContext.cleanup();
    _defaultView.reset();
    PARAM_CLEANUP();
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

  const ViewPtr &getAndUpdateView() {
    Var &viewVar = (Var &)_view.get();
    if (!viewVar.isNone()) {
      SHView &shView = varAsObjectChecked<SHView>(viewVar, Types::View);
      shView.updateVariables();
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
      _graphicsRendererContext->render(getAndUpdateView(), collectPipelineSteps());
    } else {
      _graphicsRendererContext->renderer->render(getAndUpdateView(), collectPipelineSteps());
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
  REGISTER_ENUM(Types::TextureAddressingEnumInfo);
  REGISTER_ENUM(Types::TextureFilteringEnumInfo);

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
