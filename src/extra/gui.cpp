#include "gfx/shards_types.hpp"
#include <foundation.hpp>
#include <shards_macros.hpp>
#include <common_types.hpp>
#include <params.hpp>
#include "gfx/egui/egui_render_pass.hpp"

using namespace gfx;

namespace shards {
namespace Gui {
using gfx::Types;

struct UIPassShard {

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return Types::PipelineStep; }

  PARAM_PARAMVAR(_queue, "Queue", "The queue to draw from (Optional). Uses the default queue if not specified",
                 {CoreInfo::NoneType, Type::VariableOf(Types::DrawQueue)});
  PARAM_IMPL(UIPassShard, PARAM_IMPL_FOR(_queue));

  PipelineStepPtr *_step{};

  void cleanup() {
    if (_step) {
      Types::PipelineStepObjectVar.Release(_step);
      _step = nullptr;
    }
    PARAM_CLEANUP();
  }

  void warmup(SHContext *context) {
    _step = Types::PipelineStepObjectVar.New();
    PARAM_WARMUP(context);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    Var queueVar(_queue.get());
    if (queueVar.isNone())
      throw ActivationError("Queue is required");

    SHDrawQueue *shDrawQueue = (reinterpret_cast<SHDrawQueue *>(queueVar.payload.objectValue));

    *_step = EguiRenderPass::createPipelineStep(shDrawQueue->queue);
    return Types::PipelineStepObjectVar.Get(_step);
  }
};

void registerShards() { REGISTER_SHARD("GFX.UIPass", UIPassShard); }
} // namespace Gui
} // namespace shards