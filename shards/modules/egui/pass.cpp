#include <shards/modules/gfx/shards_types.hpp>
#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include "egui_render_pass.hpp"

using namespace gfx;

namespace shards::egui {
using shards::CoreInfo;

struct UIPassShard {

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return gfx::ShardsTypes::PipelineStep; }

  static SHOptionalString help() {
    return SHCCSTR("This shard creates a render pass object designed for rendering UI using the drawables queue specified in the Queue parameter.");
  }

  static SHOptionalString inputHelp() { return DefaultHelpText::InputHelpIgnored; }
  static SHOptionalString outputHelp() { return SHCCSTR("The render pass object."); }

  PARAM_EXT(ParamVar, _name, ShardsTypes::NameParameterInfo);
  PARAM_PARAMVAR(_queue, "Queue", "The drawables queue to be used for UI rendering.",
                 {CoreInfo::NoneType, Type::VariableOf(ShardsTypes::DrawQueue)});
  PARAM_IMPL(PARAM_IMPL_FOR(_queue), PARAM_IMPL_FOR(_name));

  PipelineStepPtr *_stepPtr{};

  RenderDrawablesStep &getRenderDrawablesStep() {
    assert(_stepPtr);
    return std::get<RenderDrawablesStep>(*_stepPtr->get());
  }

  void cleanup(SHContext *context) {
    if (_stepPtr) {
      ShardsTypes::PipelineStepObjectVar.Release(_stepPtr);
      _stepPtr = nullptr;
    }
    PARAM_CLEANUP(context);
  }

  void warmup(SHContext *context) {
    _stepPtr = ShardsTypes::PipelineStepObjectVar.New();
    PARAM_WARMUP(context);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    Var queueVar(_queue.get());
    if (queueVar.isNone())
      throw ActivationError("Queue is required");

    SHDrawQueue *shDrawQueue = (reinterpret_cast<SHDrawQueue *>(queueVar.payload.objectValue));

    if (!(*_stepPtr)) {
      *_stepPtr = EguiRenderPass::createPipelineStep(shDrawQueue->queue);

      if (!_name.isNone()) {
        std::get<RenderDrawablesStep>(*(*_stepPtr).get()).name = SHSTRVIEW(_name.get());
      }
    }
    getRenderDrawablesStep().drawQueue = shDrawQueue->queue;

    return ShardsTypes::PipelineStepObjectVar.Get(_stepPtr);
  }
};

} // namespace shards::egui
SHARDS_REGISTER_FN(pass) { REGISTER_SHARD("GFX.UIPass", shards::egui::UIPassShard); }
