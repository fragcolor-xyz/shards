#include "../gfx.hpp"
#include <gfx/view.hpp>
#include <gfx/renderer.hpp>
#include <gfx/window.hpp>
#include <gfx/fmt.hpp>
#include <input/input_stack.hpp>
#include "common_types.hpp"
#include "extra/gfx.hpp"
#include "extra/gfx/shards_utils.hpp"
#include "foundation.hpp"
#include "gfx/error_utils.hpp"
#include "gfx/fwd.hpp"
#include "params.hpp"
#include "linalg_shim.hpp"
#include "shards.h"
#include "shards_utils.hpp"
#include "drawable_utils.hpp"
#include "../inputs.hpp"

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

  static SHOptionalString help() {
    return SHCCSTR("Defines a viewer (or camera) for a rendered frame based on a view transform matrix");
  }

  PARAM_PARAMVAR(_viewTransform, "View", "The view matrix.", {CoreInfo::NoneType, Type::VariableOf(CoreInfo::Float4x4Type)});
  PARAM_PARAMVAR(_fov, "Fov", "The vertical field of view. (In radians. Implies perspective projection)",
                 {CoreInfo::NoneType, CoreInfo::FloatType, Type::VariableOf(CoreInfo::FloatType)});
  PARAM_IMPL(ViewShard, PARAM_IMPL_FOR(_viewTransform), PARAM_IMPL_FOR(_fov));

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
    ViewPtr view = _view->view = std::make_shared<View>();
    if (_viewTransform->valueType != SHType::None) {
      view->view = shards::Mat4(_viewTransform.get());
    }

    // TODO: Add projection/viewport override params
    view->proj = ViewPerspectiveProjection{};

    Var fovVar = (Var &)_fov.get();
    if (!fovVar.isNone())
      std::get<ViewPerspectiveProjection>(view->proj).fov = float(fovVar);

    if (_viewTransform.isVariable()) {
      _view->viewTransformVar = (SHVar &)_viewTransform;
      _view->viewTransformVar.warmup(context);
    }

    return Types::ViewObjectVar.Get(_view);
  }
};

struct RegionShard {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyTableType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR("Renders within a region of the screen and/or to a render target"); }

  PARAM(ShardsVar, _content, "Content", "The code to run inside this region", {CoreInfo::ShardsOrNone});
  PARAM_IMPL(RegionShard, PARAM_IMPL_FOR(_content));

  RequiredGraphicsRendererContext _graphicsRendererContext;
  Inputs::RequiredInputContext _inputContext;

  void warmup(SHContext *context) {
    _graphicsRendererContext.warmup(context);
    _inputContext.warmup(context);
    PARAM_WARMUP(context);
  }

  void cleanup() {
    PARAM_CLEANUP();
    _graphicsRendererContext.cleanup();
    _inputContext.cleanup();
  }

  SHExposedTypesInfo requiredVariables() {
    static auto e =
        exposedTypesOf(decltype(_graphicsRendererContext)::getExposedTypeInfo(), decltype(_inputContext)::getExposedTypeInfo());
    return e;
  }

  SHTypeInfo compose(SHInstanceData &data) { return _content.compose(data).outputType; }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    ViewStack::Item viewItem{};
    input::InputStack::Item inputItem;

    // (optional) set viewport rectangle
    auto &inputTable = input.payload.tableValue;
    SHVar viewportVar{};
    if (getFromTable(shContext, inputTable, "Viewport", viewportVar)) {
      auto &v = viewportVar.payload.float4Value;
      // Read SHType::Float4 rect as (X0, Y0, X1, Y1)
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
      // Read SHType::Float4 rect as (X0, Y0, X1, Y1)
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

    ViewStack &viewStack = _graphicsRendererContext->renderer->getViewStack();
    input::InputStack &inputStack = _inputContext->inputStack;

    viewStack.push(std::move(viewItem));
    inputStack.push(std::move(inputItem));

    SHVar contentOutput;
    _content.activate(shContext, SHVar{}, contentOutput);

    viewStack.pop();
    inputStack.pop();

    return contentOutput;
  }
};

// :Textures {:outputName .textureVar}
// Face indices are ordered   X+ X- Y+ Y- Z+ Z-
// :Textures {:outputName {:Texture .textureVar :Face 0}}
// Mip map indices starting at 0, can be combined with cubemap faces
// :Textures {:outputName {:Texture .textureVar :Mip 1 :Face 0}}
struct RenderIntoShard {
  static inline std::array<SHString, 4> TextureSubresourceTableKeys{"Texture", "Face", "Mip", ""};
  static inline shards::Types TextureSubresourceTableTypes{Type::VariableOf(Types::Texture), CoreInfo::IntType,
                                                           CoreInfo::IntType};
  static inline shards::Type TextureSubresourceTable = Type::TableOf(TextureSubresourceTableTypes, TextureSubresourceTableKeys);

  static inline shards::Types AttachmentTableTypes{TextureSubresourceTable, Type::VariableOf(Types::Texture)};
  static inline shards::Type AttachmentTable = Type::TableOf(AttachmentTableTypes);

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR("Renders within a region of the screen and/or to a render target"); }

  PARAM_VAR(_textures, "Textures", "The textures to render into to create.", {AttachmentTable});
  PARAM(ShardsVar, _contents, "Contents", "The shards that will render into the given textures.", {CoreInfo::ShardRefSeqType});
  PARAM_PARAMVAR(_referenceSize, "Size", "The reference size. This will control the size of the render targets.",
                 {CoreInfo::Int2Type, CoreInfo::Int2VarType});
  PARAM_VAR(_matchOutputSize, "MathOutputSize",
            "When true, the texture rendered into is automatically resized to match the output size.", {CoreInfo::BoolType});
  PARAM_PARAMVAR(_viewport, "Viewport", "The viewport.", {CoreInfo::Int4Type, CoreInfo::Int4VarType});
  PARAM_IMPL(RenderIntoShard, PARAM_IMPL_FOR(_textures), PARAM_IMPL_FOR(_contents), PARAM_IMPL_FOR(_referenceSize),
             PARAM_IMPL_FOR(_matchOutputSize), PARAM_IMPL_FOR(_viewport));

  RequiredGraphicsRendererContext _graphicsRendererContext;
  Inputs::OptionalInputContext _inputContext;
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

  SHExposedTypesInfo requiredVariables() {
    static auto e = exposedTypesOf(decltype(_graphicsRendererContext)::getExposedTypeInfo());
    return e;
  }

  SHTypeInfo compose(SHInstanceData &data) { return _contents.compose(data).outputType; }

  TextureSubResource applyAttachment(SHContext *shContext, const SHVar &input) {
    if (input.valueType == SHType::ContextVar) {
      ParamVar var{input};
      var.warmup(shContext);
      return *varAsObjectChecked<TexturePtr>(var.get(), Types::Texture);
    } else {
      checkType(input.valueType, SHType::Table, "Attachment");
      auto &table = input.payload.tableValue;

      Var textureVar;
      if (!getFromTable(shContext, table, "Texture", textureVar)) {
        throw formatException("Texture is required");
      }

      TexturePtr texture = varToTexture(textureVar);

      uint8_t faceIndex{};
      Var faceIndexVar;
      if (getFromTable(shContext, table, "Face", faceIndexVar)) {
        checkType(faceIndexVar.valueType, SHType::Int, "Face should be an integer");
        faceIndex = uint8_t(int(faceIndexVar));
      }

      uint8_t mipIndex{};
      Var mipIndexVar;
      if (getFromTable(shContext, table, "Mip", mipIndexVar)) {
        checkType(mipIndexVar.valueType, SHType::Int, "Mip should be an integer");
        mipIndex = uint8_t(int(mipIndexVar));
      }

      return TextureSubResource(texture, faceIndex, mipIndex);
    }
  }

  void applyAttachments(SHContext *shContext, std::map<std::string, TextureSubResource> &outAttachments, const SHTable &input) {
    auto &table = _textures.payload.tableValue;
    outAttachments.clear();
    ForEach(table, [&](SHString &k, SHVar &v) { outAttachments.emplace(k, applyAttachment(shContext, v)); });

    if (outAttachments.size() == 0) {
      throw formatException("RenderInto is missing at least one output texture");
    }
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    // Set the render target textures
    auto &rt = _renderTarget;
    auto &attachments = rt->attachments;

    applyAttachments(shContext, attachments, input.payload.tableValue);

    ViewStack::Item viewItem{};
    input::InputStack::Item inputItem;

    viewItem.renderTarget = _renderTarget;

    GraphicsRendererContext &ctx = _graphicsRendererContext;
    ViewStack &viewStack = ctx.renderer->getViewStack();

    Var referenceSizeVar = (Var &)_referenceSize.get();
    bool matchOutputSize = !_matchOutputSize.isNone() ? (bool)_matchOutputSize : true;

    int2 referenceSize;
    if (!referenceSizeVar.isNone()) {
      referenceSize = toInt2(referenceSizeVar);
    } else if (matchOutputSize) {
      referenceSize = viewStack.getOutput().referenceSize;
    } else {
      referenceSize = attachments[0].texture->getResolution();
      for (auto &attachment : attachments) {
        referenceSize = attachment.second.getResolution();
        if (referenceSize.x != 0 || referenceSize.y != 0)
          break;
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

    // NOTE: Don't need to resize textures since the renderer does it automatically for given render targets

    viewStack.push(std::move(viewItem));

    if (_inputContext) {
      _inputContext->inputStack.push(std::move(inputItem));
    }

    SHVar contentOutput;
    _contents.activate(shContext, input, contentOutput);

    viewStack.pop();

    if (_inputContext) {
      _inputContext->inputStack.pop();
    }

    return contentOutput;
  }
};

void registerViewShards() {
  REGISTER_SHARD("GFX.View", ViewShard);
  REGISTER_SHARD("GFX.Region", RegionShard);
  REGISTER_SHARD("GFX.RenderInto", RenderIntoShard);
}
} // namespace gfx
