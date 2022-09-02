#include "context.hpp"
#include "../gfx/shards_types.hpp"
#include <gfx/context.hpp>
#include <gfx/window.hpp>
#include <object_var_util.hpp>
#include <params.hpp>

namespace shards {
namespace Gizmos {
using namespace gfx;
struct GizmosContextShard : public gfx::BaseConsumer {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR("Provides a context for rendering gizmos"); }

  PARAM_PARAMVAR(_view, "View",
                 "The view used to render the gizmos."
                 "When drawing over a scene, the view should be the same",
                 {Type::VariableOf(gfx::Types::View)});
  PARAM_PARAMVAR(_queue, "Queue", "The queue to draw into", {Type::VariableOf(gfx::Types::DrawQueue)});
  PARAM(ShardsVar, _content, "Content", "Content", {CoreInfo::ShardsOrNone});
  PARAM_IMPL(GizmosContextShard, PARAM_IMPL_FOR(_view), PARAM_IMPL_FOR(_queue), PARAM_IMPL_FOR(_content));

  Context _context{};
  SHVar *_contextVarRef{};

  int2 _cursorPosition{};
  bool _mouseButtonState{};

  void warmup(SHContext *context) {
    baseConsumerWarmup(context, true);

    // Reference context variable
    _contextVarRef = referenceVariable(context, Context::contextVarName);

    withObjectVariable(*_contextVarRef, &_context, Context::Type, [&] { PARAM_WARMUP(context); });
  }

  void cleanup() {
    withObjectVariable(*_contextVarRef, &_context, Context::Type, [&] { PARAM_CLEANUP(); });

    baseConsumerCleanup();

    if (_contextVarRef) {
      releaseVariable(_contextVarRef);
    }
  }

  SHTypeInfo compose(SHInstanceData &data) {
    composeCheckMainThread(data);
    composeCheckMainWindowGlobals(data);

    std::vector<SHExposedTypeInfo> exposed(PtrIterator(data.shared.elements),
                                           PtrIterator(data.shared.elements + data.shared.len));
    exposed.push_back(ExposedInfo::ProtectedVariable(Context::contextVarName, SHCCSTR("Helper context"), Context::Type));

    if (!_queue.isVariable())
      throw ComposeError("Queue not set");

    if (!_view.isVariable())
      throw ComposeError("View not set");

    SHInstanceData contentInstanceData = data;
    contentInstanceData.shared.elements = exposed.data();
    contentInstanceData.shared.len = exposed.size();
    return _content.compose(contentInstanceData).outputType;
  }

  void handleGizmoInputEvents(std::vector<SDL_Event> &events) {
    for (auto &event : events) {
      if (event.type == SDL_MOUSEMOTION) {
        _cursorPosition.x = event.motion.x;
        _cursorPosition.y = event.motion.y;
      } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
        _cursorPosition.x = event.button.x;
        _cursorPosition.y = event.button.y;
        if (event.button.state == SDL_PRESSED)
          _mouseButtonState |= SDL_BUTTON(event.button.button);
        else
          _mouseButtonState &= ~SDL_BUTTON(event.button.button);
      }
    }
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    Var queueVar(_queue.get());
    Var viewVar(_view.get());

    ViewPtr view = static_cast<SHView *>(viewVar.payload.objectValue)->view;

    assert(queueVar.payload.objectValue);
    _context.queue = static_cast<SHDrawQueue *>(queueVar.payload.objectValue)->queue;
    assert(_context.queue);

    gfx::Context &gfxContext = getContext();
    gfx::MainWindowGlobals &mainWindowGlobals = getMainWindowGlobals();
    gfx::Window &window = getWindow();
    int2 outputSize = gfxContext.getMainOutputSize();

    handleGizmoInputEvents(mainWindowGlobals.events);

    float2 drawableScale = float2(window.getDrawableSize()) / float2(window.getSize());

    gfx::gizmos::InputState gizmoInput;
    gizmoInput.cursorPosition = float2(_cursorPosition) * drawableScale;
    gizmoInput.pressed = _mouseButtonState;
    gizmoInput.viewSize = outputSize;

    SHVar _shardsOutput{};
    withObjectVariable(*_contextVarRef, &_context, Context::Type, [&] {
      _context.gizmoContext.begin(gizmoInput, view);
      _content.activate(shContext, input, _shardsOutput);
      _context.gizmoContext.end(_context.queue);
    });

    return _shardsOutput;
  }
};

extern void registerHighlightShards();
extern void registerGizmoShards();
extern void registerShapeShards();
void registerShards() {
  REGISTER_SHARD("Gizmos.Context", GizmosContextShard);
  registerHighlightShards();
  registerGizmoShards();
  registerShapeShards();
}
} // namespace Gizmos
} // namespace shards
