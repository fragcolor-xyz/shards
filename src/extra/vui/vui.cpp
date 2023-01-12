#include "vui.hpp"
#include "../gfx.hpp"
#include "../inputs.hpp"
#include "params.hpp"
#include "../egui/context.hpp"

namespace shards::VUI {

struct VUIContextShard {
  VUIContext _vuiContext{};
  SHVar *_vuiContextVar{};

  Inputs::RequiredInputContext _inputContext;
  gfx::RequiredGraphicsContext _graphicsContext;

  ExposedInfo _exposedVariables;

  PARAM(ShardsVar, _contents, "Contents", "The panel UI contents", {CoreInfo::ShardsOrNone});
  PARAM_IMPL(VUIContextShard, PARAM_IMPL_FOR(_contents));

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() {
    return SHCCSTR("Creates a context for virtual UI panels to make sure input is correctly handled between them");
  }

  SHExposedTypesInfo requiredVariables() {
    static auto e =
        exposedTypesOf(decltype(_inputContext)::getExposedTypeInfo(), decltype(_graphicsContext)::getExposedTypeInfo());
    return e;
  }

  void warmup(SHContext *context) {
    _vuiContextVar = referenceVariable(context, VUIContext::VariableName);
    _vuiContextVar->payload.objectTypeId = SHTypeInfo(VUIContext::Type).object.typeId;
    _vuiContextVar->payload.objectVendorId = SHTypeInfo(VUIContext::Type).object.vendorId;
    _vuiContextVar->payload.objectValue = &_vuiContext;
    _vuiContextVar->valueType = SHType::Object;

    _inputContext.warmup(context);
    _graphicsContext.warmup(context);
    _contents.warmup(context);
  }

  void cleanup() {
    _contents.cleanup();
    _graphicsContext.cleanup();
    _inputContext.cleanup();

    if (_vuiContextVar) {
      if (_vuiContextVar->refcount > 1) {
        SHLOG_ERROR("VUI.Context: Found {} dangling reference(s) to {}", _vuiContextVar->refcount - 1, VUIContext::VariableName);
      }
      releaseVariable(_vuiContextVar);
    }
  }

  SHTypeInfo compose(SHInstanceData &data) {
    _exposedVariables = ExposedInfo(data.shared);
    _exposedVariables.push_back(VUIContext::VariableInfo);
    data.shared = SHExposedTypesInfo(_exposedVariables);

    _contents.compose(data);

    return shards::CoreInfo::NoneType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    SHVar output{};
    _contents.activate(shContext, input, output);
    return output;
  }
};

struct VUIPanelShard {
  PARAM(ShardsVar, _contents, "Contents", "The panel UI contents", {CoreInfo::ShardsOrNone});
  PARAM_IMPL(VUIPanelShard, PARAM_IMPL_FOR(_contents));

  RequiredVUIContext _context;
  Shard *_uiShard{};
  EguiContext _eguiContext;
  ExposedInfo _exposedTypes;

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR("Defines a virtual UI panel"); }

  SHExposedTypesInfo requiredVariables() {
    static auto e = exposedTypesOf(RequiredVUIContext::getExposedTypeInfo());
    return e;
  }

  void ensureEguiContext() {
    if (!_eguiContext)
      _eguiContext = egui_createContext();
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _context.warmup(context);

    ensureEguiContext();
    egui_warmup(_eguiContext, context);
  }

  void cleanup() {
    PARAM_CLEANUP();
    _context.cleanup();

    if (_eguiContext) {
      egui_cleanup(_eguiContext);
      egui_destroyContext(_eguiContext);
      _eguiContext = nullptr;
    }
  }

  SHTypeInfo compose(SHInstanceData &data) {
    ensureEguiContext();

    SHExposedTypesInfo eguiExposedTypes{};
    egui_getExposedTypeInfo(_eguiContext, eguiExposedTypes);

    _exposedTypes = ExposedInfo(data.shared);
    mergeIntoExposedInfo(_exposedTypes, eguiExposedTypes);

    // Compose contents
    data.shared = SHExposedTypesInfo(_exposedTypes);
    _contents.compose(data);

    return shards::CoreInfo::NoneType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    egui::Input eguiInput{};
    eguiInput.screenRect = egui::Rect{{0, 0}, {400, 400}};
    eguiInput.pixelsPerPoint = 1.0f;

    SHVar output{};
    const char *error = egui_activate(_eguiContext, eguiInput, _contents.shards(), shContext, input, output);
    if (error)
      throw ActivationError(fmt::format("egui activation error: {}", error));

    const egui::FullOutput &_eguiOutput = *egui_getOutput(_eguiContext);
    (void)_eguiOutput;

    return output;
  }
};

void registerShards() {
  REGISTER_SHARD("VUI.Context", VUIContextShard);
  REGISTER_SHARD("VUI.Panel", VUIPanelShard);
}
} // namespace shards::VUI