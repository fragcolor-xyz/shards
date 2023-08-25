#include "gfx.hpp"
#include <gfx/view.hpp>
#include <gfx/renderer.hpp>
#include <gfx/window.hpp>
#include <gfx/fmt.hpp>
#include <input/input_stack.hpp>
#include <shards/common_types.hpp>
#include "shards_utils.hpp"
#include <shards/core/foundation.hpp>
#include "gfx/error_utils.hpp"
#include "gfx/fwd.hpp"
#include "gfx/render_target.hpp"
#include <shards/core/params.hpp>
#include <shards/linalg_shim.hpp>
#include <shards/shards.h>
#include "shards_utils.hpp"
#include "drawable_utils.hpp"
#include <shards/modules/inputs/inputs.hpp>
#include "window.hpp"

using namespace shards;

namespace gfx {
void SHView::updateVariables() {
  if (viewTransformVar && viewTransformVar->isVariable()) {
    view->view = shards::Mat4(viewTransformVar->get());
  }
}

struct ViewShard {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return Types::View; }

  static SHOptionalString help() {
    return SHCCSTR("Defines a viewer (or camera) for a rendered frame based on a view transform matrix");
  }

  PARAM_PARAMVAR(_viewTransform, "View", "The view matrix.", {CoreInfo::NoneType, Type::VariableOf(CoreInfo::Float4x4Type)});
  PARAM_PARAMVAR(_fov, "Fov", "The vertical field of view. (In radians. Implies perspective projection)",
                 {CoreInfo::NoneType, CoreInfo::FloatType, Type::VariableOf(CoreInfo::FloatType)});
  PARAM_IMPL(PARAM_IMPL_FOR(_viewTransform), PARAM_IMPL_FOR(_fov));

  SHView *_view;

  void cleanup() {
    PARAM_CLEANUP();

    if (_view) {
      Types::ViewObjectVar.Release(_view);
      _view = nullptr;
    }
  }

  void warmup(SHContext *context) {
    _view = Types::ViewObjectVar.New();

    PARAM_WARMUP(context);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    ViewPtr &view = _view->view;
    if (!view)
      view = std::make_shared<View>();

    if (_viewTransform->valueType != SHType::None) {
      view->view = shards::Mat4(_viewTransform.get());
    }

    // TODO: Add projection/viewport override params
    view->proj = ViewPerspectiveProjection{};

    Var fovVar = (Var &)_fov.get();
    if (!fovVar.isNone())
      std::get<ViewPerspectiveProjection>(view->proj).fov = float(fovVar);

    _view->viewTransformVar = &_viewTransform;

    return Types::ViewObjectVar.Get(_view);
  }
};

// :Textures {:outputName .textureVar}
// Face indices are ordered   X+ X- Y+ Y- Z+ Z-
// :Textures {:outputName {:Texture .textureVar :Face 0}}
// Mip map indices starting at 0, can be combined with cubemap faces
// :Textures {:outputName {:Texture .textureVar :Mip 1 :Face 0}}
struct RenderIntoShard {
  // static inline std::array<SHString, 4> TextureSubresourceTableKeys{"Texture", "Face", "Mip", ""};
  // static inline shards::Types TextureSubresourceTableTypes{Type::VariableOf(Types::Texture), CoreInfo::IntType,
  //                                                          Type::VariableOf(CoreInfo::IntType)};
  // static inline shards::Type TextureSubresourceTable = Type::TableOf(TextureSubresourceTableTypes,
  // TextureSubresourceTableKeys);
  // Can not check, support any table for now
  static inline shards::Type TextureSubresourceTable = CoreInfo::AnyTableType;

  static inline shards::Types AttachmentTableTypes{TextureSubresourceTable, Type::VariableOf(Types::Texture)};
  static inline shards::Type AttachmentTable = Type::TableOf(AttachmentTableTypes);

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR("Renders within a region of the screen and/or to a render target"); }

  PARAM(OwnedVar, _textures, "Textures", "The textures to render into to create.", {AttachmentTable});
  PARAM(ShardsVar, _contents, "Contents", "The shards that will render into the given textures.", {CoreInfo::Shards});
  PARAM_PARAMVAR(_referenceSize, "Size", "The reference size. This will control the size of the render targets.",
                 {CoreInfo::Int2Type, CoreInfo::Int2VarType});
  PARAM_VAR(_matchOutputSize, "MatchOutputSize",
            "When true, the texture rendered into is automatically resized to match the output size.", {CoreInfo::BoolType});
  PARAM_PARAMVAR(_viewport, "Viewport", "The viewport.", {CoreInfo::Int4Type, CoreInfo::Int4VarType});
  PARAM_PARAMVAR(_windowRegion, "WindowRegion", "Sets the window region for input handling.",
                 {CoreInfo::NoneType, CoreInfo::Float4Type, CoreInfo::Float4VarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_textures), PARAM_IMPL_FOR(_contents), PARAM_IMPL_FOR(_referenceSize),
             PARAM_IMPL_FOR(_matchOutputSize), PARAM_IMPL_FOR(_viewport), PARAM_IMPL_FOR(_windowRegion));

  RequiredGraphicsRendererContext _graphicsRendererContext;
  shards::input::OptionalInputContext _inputContext;
  RenderTargetPtr _renderTarget;

  void warmup(SHContext *context) {
    _graphicsRendererContext.warmup(context);
    _inputContext.warmup(context);
    _renderTarget = std::make_shared<RenderTarget>();
    PARAM_WARMUP(context);
  }

  void cleanup() {
    PARAM_CLEANUP();
    _graphicsRendererContext.cleanup();
    _inputContext.cleanup();
    _renderTarget.reset();
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    _requiredVariables.push_back(decltype(_graphicsRendererContext)::getExposedTypeInfo());

    if (findExposedVariable(data.shared, decltype(_inputContext)::getExposedTypeInfo().name)) {
      _requiredVariables.push_back(decltype(_inputContext)::getExposedTypeInfo());
    }

    return _contents.compose(data).outputType;
  }

  TextureSubResource applyAttachment(SHContext *shContext, const SHVar &input) {
    if (input.valueType == SHType::ContextVar) {
      ParamVar var{input};
      var.warmup(shContext);
      return varAsObjectChecked<TexturePtr>(var.get(), Types::Texture);
    } else {
      checkType(input.valueType, SHType::Table, "Attachment");
      auto &table = input.payload.tableValue;

      Var textureVar;
      if (!getFromTable(shContext, table, Var("Texture"), textureVar)) {
        throw formatException("Texture is required");
      }

      TexturePtr texture = varToTexture(textureVar);

      uint8_t faceIndex{};
      Var faceIndexVar;
      if (getFromTable(shContext, table, Var("Face"), faceIndexVar)) {
        checkType(faceIndexVar.valueType, SHType::Int, "Face should be an integer(variable)");
        faceIndex = uint8_t(int(faceIndexVar));
      }

      uint8_t mipIndex{};
      Var mipIndexVar;
      if (getFromTable(shContext, table, Var("Mip"), mipIndexVar)) {
        checkType(mipIndexVar.valueType, SHType::Int, "Mip should be an integer(variable)");
        mipIndex = uint8_t(int(mipIndexVar));
      }

      return TextureSubResource(texture, faceIndex, mipIndex);
    }
  }

  void applyAttachments(SHContext *shContext, std::map<std::string, TextureSubResource> &outAttachments) {
    auto &table = _textures.payload.tableValue;
    outAttachments.clear();
    ForEach(table, [&](SHVar &k, SHVar &v) {
      if (k.valueType != SHType::String)
        throw formatException("RenderInto attachment key should be a string");
      std::string keyStr(SHSTRVIEW(k));
      outAttachments.emplace(std::move(keyStr), applyAttachment(shContext, v));
    });

    if (outAttachments.size() == 0) {
      throw formatException("RenderInto is missing at least one output texture");
    }
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    // Set the render target textures
    auto &rt = _renderTarget;
    auto &attachments = rt->attachments;

    applyAttachments(shContext, attachments);

    ViewStack::Item viewItem{};
    input::InputStack::Item inputItem;

    viewItem.renderTarget = _renderTarget;

    GraphicsRendererContext &ctx = _graphicsRendererContext;
    const ViewStack &viewStack = ctx.renderer->getViewStack();

    Var referenceSizeVar = (Var &)_referenceSize.get();
    bool matchOutputSize = !_matchOutputSize->isNone() ? (bool)*_matchOutputSize : false;

    int2 referenceSize{};
    if (!referenceSizeVar.isNone()) {
      referenceSize = toInt2(referenceSizeVar);
    } else if (matchOutputSize) {
      referenceSize = viewStack.getOutput().referenceSize;
    } else {
      for (auto &attachment : attachments) {
        referenceSize = attachment.second.getResolution();
        if (referenceSize.x != 0 || referenceSize.y != 0)
          break;
      }
    }

    if (_inputContext) {
      // (optional) Push window region for input
      Var windowRegionVar{_windowRegion.get()};
      if (!windowRegionVar.isNone()) {
        // Read SHType::Float4 rect as (X0, Y0, X1, Y1)
        float4 v = toVec<float4>(windowRegionVar);
        int4 region(v[0], v[1], v[2], v[3]);

        // This is used for translating cursor inputs from window to view space
        // TODO: Adjust relative to parent region, for now always assume this is in global window coordinates
        inputItem.windowMapping = input::WindowSubRegion{
            .region = region,
        };

        // Set reference size to actual window size
        float2 regionSize = float2(region.z - region.x, region.w - region.y);
        referenceSize = int2(linalg::floor(float2(regionSize)));
      }
    }

    viewItem.referenceSize = referenceSize;

    if (referenceSize.x == 0 || referenceSize.y == 0)
      throw formatException("Invalid texture size for RenderInto: {}", referenceSize);

    // (optional) set viewport rectangle
    Var viewportVar = (Var &)_viewport.get();
    if (!viewportVar.isNone()) {
      auto &v = viewportVar.payload.float4Value;
      // Read SHType::Float4 rect as (X0, Y0, X1, Y1)
      viewItem.viewport = Rect::fromCorners(v[0], v[1], v[2], v[3]);
    } else {
      viewItem.viewport = Rect(referenceSize);
    }

    // Make render target fixed size
    rt->size = RenderTargetSize::Fixed{.size = referenceSize};

    ctx.renderer->pushView(std::move(viewItem));

    input::InputStack *inputStack = _inputContext ? &_inputContext->getInputStack() : nullptr;
    if (inputStack) {
      inputStack->push(std::move(inputItem));
    }

    SHVar contentOutput;
    _contents.activate(shContext, input, contentOutput);

    ctx.renderer->popView();

    if (inputStack) {
      inputStack->pop();
    }

    return contentOutput;
  }
};

struct ViewProjectionMatrixShard {
  static SHTypesInfo inputTypes() { return Types::View; }
  static SHTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }
  static SHOptionalString help() { return SHCCSTR("Returns the combined view projection matrix of the view"); }

  PARAM_PARAMVAR(_viewSize, "ViewSize", "The size of the screen this view is being used with",
                 {CoreInfo::Float2Type, CoreInfo::Float2VarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_viewSize));

  Mat4 _result;

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return CoreInfo::Float4x4Type;
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup() { PARAM_CLEANUP(); }

  SHVar activate(SHContext *context, const SHVar &input) {
    ViewPtr view = varAsObjectChecked<ViewPtr>(input, Types::View);
    auto projMatrix = view->getProjectionMatrix(toVec<float2>(_viewSize.get()));
    _result = linalg::mul(projMatrix, view->view);
    return _result;
  }
};

void registerViewShards() {
  REGISTER_SHARD("GFX.View", ViewShard);
  REGISTER_SHARD("GFX.RenderInto", RenderIntoShard);
  REGISTER_SHARD("GFX.ViewProjectionMatrix", ViewProjectionMatrixShard);
}
} // namespace gfx
