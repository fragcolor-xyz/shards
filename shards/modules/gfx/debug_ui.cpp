#include "../inputs/inputs.hpp"
#include "debug_ui.hpp"
#include <optional>
#include <shards/common_types.hpp>
#include <shards/core/module.hpp>

using namespace shards::input;

extern "C" {
void shards_gfx_freeString(const char *str) { free((void *)str); }
}

namespace shards::gfx {
struct DebugUI {
  RequiredInputContext _inputContext;
  ExposedInfo _required;
  SHVar *_uiContext{};

  debug::DebugUIOpts _opts{};

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR("Shows the graphics system debug UI"); }

  SHExposedTypesInfo requiredVariables() { return SHExposedTypesInfo(_required); }

  static inline Type EguiUiType = Type::Object(CoreCC, 'eguU');
  static inline Type EguiContextType = Type::Object(CoreCC, 'eguC');
  static inline char EguiContextName[] = "UI.Contexts";

  static void mergeRequiredUITypes(ExposedInfo &out) {
    out.push_back(SHExposedTypeInfo{
        .name = EguiContextName,
        .exposedType = EguiContextType,
    });
  }

  SHTypeInfo compose(SHInstanceData &data) {
    _required.clear();
    _required.push_back(_inputContext.getExposedTypeInfo());
    mergeRequiredUITypes(_required);

    return CoreInfo::NoneType;
  }

  void warmup(SHContext *context) {
    _inputContext.warmup(context);
    _uiContext = referenceVariable(context, EguiContextName);
  }
  void cleanup() {
    _inputContext.cleanup();

    releaseVariable(_uiContext);
    _uiContext = nullptr;
  }

  size_t frameIndex{};

  SHVar activate(SHContext *shContext, const SHVar &input) {
    debug::DebugUIParams params{
        .opts = _opts,
        .frameQueues = nullptr,
        .numFrameQueues = 0,
        .currentFrame = frameIndex,
    };
    shards_gfx_showDebugUI(_uiContext, params);

    return SHVar{};
  }
};
} // namespace shards::gfx

SHARDS_REGISTER_FN(debug) {
  using namespace shards::gfx;
  REGISTER_SHARD("GFX.DebugUI", DebugUI);
}
