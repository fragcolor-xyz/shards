#include "debug_ui.hpp"
#include "inputs.hpp"
#include <shards/input/debug.hpp>
#include <shards/input/master.hpp>
#include <shards/common_types.hpp>
#include <shards/core/module.hpp>

extern "C" {
const char *shards_input_eventToString(shards::input::debug::OpaqueEvent opaque) {
  auto &evt = *(shards::input::ConsumableEvent *)opaque;
  auto str = shards::input::debugFormat(evt.event);
  auto result = strdup(str.c_str());
  return result;
}
void shards_input_freeString(const char *str) { free((void *)str); }
bool shards_input_eventIsConsumed(shards::input::debug::OpaqueEvent opaque) {
  auto &evt = *(shards::input::ConsumableEvent *)opaque;
  return (bool)evt.consumed;
}
shards::input::debug::OpaqueLayer shards_input_eventConsumedBy(shards::input::debug::OpaqueEvent opaque) {
  auto &evt = *(shards::input::ConsumableEvent *)opaque;
  if (evt.consumed) {
    auto tag = evt.consumed.value();
    if (auto handler = tag.handler.lock())
      return handler.get();
  }
  return nullptr;
}
const char *shards_input_layerName(shards::input::debug::OpaqueLayer opaque) {
  auto layer = dynamic_cast<shards::input::debug::IDebug *>((shards::input::IInputHandler *)opaque);
  if (layer) {
    return strdup(layer->getDebugName());
  } else {
    return strdup("unknown");
  }
}
}

namespace shards::input {
struct DebugUI {
  RequiredInputContext _inputContext;
  ExposedInfo _required;
  SHVar *_uiContext{};

  debug::DebugUIOpts _opts{
      .showKeyboardEvents = true,
      .showPointerEvents = true,
      .showPointerMoveEvents = false,
  };

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR("Shows the input system debug UI"); }

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
  struct InternalTaggedEvent {
    size_t frameIndex;
    ConsumableEvent evt;
  };
  std::shared_ptr<std::list<InternalTaggedEvent>> _taggedEvents = std::make_shared<std::list<InternalTaggedEvent>>();

  // Tempt storage
  std::list<std::string> _strings;
  std::vector<std::shared_ptr<IInputHandler>> _handlers;
  std::vector<debug::TaggedEvent> _events;
  std::vector<debug::Layer> _layers;

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &master = _inputContext->getMaster();

    _strings.clear();

    // Add new events
    // This needs to happen after all handlers to get the correct consume state
    // which is done using the addPostInputCallback
    master.addPostInputCallback([taggedEvents = _taggedEvents, frameIndex=frameIndex](InputMaster &master) {
      for (auto &evt : master.getEvents()) {
        taggedEvents->emplace_back(InternalTaggedEvent{frameIndex, evt});
      }
    });

    // Structure event list
    _events.clear();
    for (auto &evt : *_taggedEvents) {
      if (isPointerEvent(evt.evt.event) && !_opts.showPointerEvents)
        continue;
      if (std::get_if<PointerMoveEvent>(&evt.evt.event) && !_opts.showPointerMoveEvents)
        continue;
      if (isKeyEvent(evt.evt.event) && !_opts.showKeyboardEvents)
        continue;
      _events.push_back(debug::TaggedEvent{evt.frameIndex, &evt.evt});
    }

    _layers.clear();
    master.getHandlers(_handlers);
    for (auto &handler : _handlers) {
      auto &layer = _layers.emplace_back();
      layer.priority = handler->getPriority();

      if (debug::IDebug *debug = dynamic_cast<debug::IDebug *>(handler.get())) {
        auto &str = _strings.emplace_back(debug->getDebugName());
        layer.name = str.c_str();
        layer.hasFocus = master.getFocusTracker().hasFocus(handler.get());
      }
    }

    debug::DebugUIParams params{
        .opts = _opts,
        .layers = _layers.data(),
        .numLayers = _layers.size(),
        .events = _events.data(),
        .numEvents = _events.size(),
        .currentFrame = frameIndex,
    };
    shards_input_showDebugUI(_uiContext, params);

    if (params.clearEvents) {
      _taggedEvents->clear();
      frameIndex = 0;
    } else {
      ++frameIndex;
    }
    return SHVar{};
  }
};
} // namespace shards::input

SHARDS_REGISTER_FN(debug) {
  using namespace shards::input;
  REGISTER_SHARD("Inputs.DebugUI", DebugUI);
}
