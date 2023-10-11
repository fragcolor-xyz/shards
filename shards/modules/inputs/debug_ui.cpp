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
  return evt.consumed;
}
}

namespace shards::input {
struct DebugUI {
  RequiredInputContext _context;
  ExposedInfo _required;
  SHVar *_uiContext{};

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
    _required.push_back(_context.getExposedTypeInfo());
    mergeRequiredUITypes(_required);

    return CoreInfo::NoneType;
  }

  void warmup(SHContext *context) {
    _context.warmup(context);
    _uiContext = referenceVariable(context, EguiContextName);
  }
  void cleanup() {
    _context.cleanup();

    releaseVariable(_uiContext);
    _uiContext = nullptr;
  }

  std::vector<debug::Layer> _layers;
  std::list<std::string> _strings;
  std::list<std::vector<const input::ConsumableEvent *>> _eventVectors;
  std::vector<std::shared_ptr<IInputHandler>> _handlers;

  SHVar activate(SHContext *shContext, const SHVar &input) {
    _layers.clear();
    _strings.clear();
    _eventVectors.clear();

    _context->getMaster()->getHandlers(_handlers);
    for (auto &handler : _handlers) {
      auto &layer = _layers.emplace_back();
      layer.priority = handler->getPriority();

      if (debug::IDebug *debug = dynamic_cast<debug::IDebug *>(handler.get())) {
        auto &str = _strings.emplace_back(debug->getDebugName());
        layer.name = str.c_str();

        auto &vec = _eventVectors.emplace_back();
        size_t head = debug->getLastFrameIndex();
        const size_t historyLength = 32;
        size_t tail = head > historyLength ? (head - historyLength) : 0;
        for (size_t i = tail; i <= head; i++) {
          if (auto frame = debug->getDebugFrame(i)) {
            for (auto &evt : *frame) {
              vec.push_back(&evt);
            }
          }
        }

        layer.debugEvents = (debug::OpaqueEvent *)vec.data();
        layer.numDebugEvents = vec.size();

        layer.consumeFlags = debug->getDebugConsumeFlags();
      }
    }

    shards_input_showDebugUI(_uiContext, _layers.data(), _layers.size());
    return SHVar{};
  }
};
} // namespace shards::input

SHARDS_REGISTER_FN(debug) {
  using namespace shards::input;
  REGISTER_SHARD("Inputs.DebugUI", DebugUI);
}
