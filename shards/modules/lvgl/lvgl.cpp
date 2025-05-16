#include <shards/shardwrapper.hpp>
#include <shards/utility.hpp>
#include <shards/core/params.hpp>
#include <shards/core/module.hpp>
#include <shards/core/shared.hpp>
#include <shards/object_type.hpp>
#include "lvgl.h"

namespace shards::lvgl {

struct LVGLContext {
  static inline const char VariableName[] = "LVGL.Context";
  static constexpr uint32_t TypeId = 'LVGL';
  static inline SHTypeInfo Type{SHType::Object, {.object = {.vendorId = CoreCC, .typeId = TypeId}}};
  static inline const SHOptionalString VariableDescription = SHCCSTR("The LVGL context.");
  static inline shards::ObjectVar<LVGLContext> ObjectVar{VariableName, shards::CoreCC, TypeId};

  lv_display_t *display{};

  LVGLContext() {
    static int _initMarker = []() {
      lv_init();
      return 0;
    }();
    (void)_initMarker;
    display = lv_display_create(1024, 1024);
  }
  ~LVGLContext() { lv_display_delete(display); }
};

struct Context {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() {
    static Types outputTypes{LVGLContext::Type};
    return outputTypes;
  }

  static SHOptionalString help() { return SHCCSTR("Creates and returns an LVGL context."); }

  LVGLContext *_ctx{};

  PARAM_IMPL();
  PARAM_REQUIRED_VARIABLES();

  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return LVGLContext::Type;
  }

  lv_obj_t *testScreen{};
  void warmup(SHContext *context) {
    LVGLContext::ObjectVar.Init(_ctx);

    testScreen = lv_obj_create(nullptr);
    auto btn = lv_button_create(testScreen);
    lv_obj_set_pos(btn, 100, 100);
    lv_obj_set_size(btn, 100, 40);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn, lv_color_hex(0xffffff), LV_PART_MAIN);

    auto txt = lv_label_create(btn);
    lv_label_set_text(txt, "Hello");
    lv_obj_align(txt, LV_ALIGN_CENTER, 0, 0);

    PARAM_WARMUP(context);
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    LVGLContext::ObjectVar.Release(_ctx);
  }

  SHVar activate(SHContext *context, const SHVar &input) { return LVGLContext::ObjectVar.Get(_ctx); }
};

SHARDS_REGISTER_FN(lvgl) { REGISTER_SHARD("LVGL.Context", Context); }

}; // namespace shards::lvgl
