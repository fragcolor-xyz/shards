#include <shards/modules/gfx/gfx.hpp>
#include <shards/modules/gfx/window.hpp>
#include <shards/core/params.hpp>
#include <shards/core/brancher.hpp>
#include <shards/core/object_var_util.hpp>
#include <shards/iterator.hpp>
#include <shards/modules/core/time.hpp>
#include <shards/input/window_input.hpp>
#include <shards/input/debug.hpp>
#include <shards/input/state.hpp>
#include <shards/input/detached.hpp>
#include <shards/core/module.hpp>
#include <shards/registry.hpp>
#include <input/events.hpp>
#include <input/input.hpp>
#include <input/input_stack.hpp>
#include <input/log.hpp>
#include "inputs.hpp"
#include "debug_ui.hpp"
#include "modules/inputs/debug_ui.hpp"
#include "modules/inputs/inputs.hpp"
#include <boost/lockfree/spsc_queue.hpp>
#include <stdexcept>

namespace shards {
namespace input {

static auto logger = getLogger();

using VarQueue = boost::lockfree::spsc_queue<OwnedVar>;

struct CapturedVariables {
  std::unordered_map<std::string, OwnedVar> variables;
};
using CapturedVariablesQueue = boost::lockfree::spsc_queue<CapturedVariables>;

static inline Type WindowContextType = WindowContext::Type;

struct GetWindowContext {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return WindowContextType; }

  RequiredWindowContext _context;
  ExposedInfo _required{decltype(_context)::getExposedTypeInfo()};

  SHExposedTypesInfo requiredVariables() { return SHExposedTypesInfo(_required); }

  void warmup(SHContext *context) { _context.warmup(context); }

  void cleanup(SHContext* context) { _context.cleanup(); }

  SHVar activate(SHContext *shContext, const SHVar &input) { return _context.asVar(); }
};

using VariableRefs = std::unordered_map<std::string, SHVar *>;

struct DetachedInputContext : public IInputContext {
  std::shared_ptr<gfx::Window> window;
  input::InputMaster *master{};
  input::DetachedInput detached;

  ConsumeFlags consumeFlags{};

  double time{};
  float deltaTime{};

  InputStack inputStack;

  virtual InputStack &getInputStack() override { return inputStack; }

  virtual shards::input::InputMaster *getMaster() const override { return master; }

  const input::InputState &getState() const override { return detached.state; }
  const std::vector<input::Event> &getEvents() const override { return detached.virtualInputEvents; }

  // Writable, controls how events are consumed
  ConsumeFlags &getConsumeFlags() override { return consumeFlags; }

  float getTime() const override { return time; }
  float getDeltaTime() const override { return deltaTime; }

  void postMessage(const input::Message &message) override { master->postMessage(message); }
};

struct Detached {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  PARAM_PARAMVAR(_context, "Context", "The window context to attach to", {Type::VariableOf(WindowContextType)});
  PARAM(OwnedVar, _inputShards, "HandleInputs", "Runs on input thread to determine what inputs are consumed",
        {CoreInfo::ShardsOrNone});
  PARAM(ShardsVar, _mainShards, "Contents", "Runs on input thread to determine what inputs are consumed",
        {CoreInfo::ShardsOrNone});
  PARAM_VAR(_priority, "Priority", "The order in which this input handler is run", {CoreInfo::IntType});
  PARAM_VAR(_name, "Name", "Name used for logging/debugging purposes", {CoreInfo::NoneType, CoreInfo::StringType});
  PARAM_IMPL(PARAM_IMPL_FOR(_context), PARAM_IMPL_FOR(_inputShards), PARAM_IMPL_FOR(_mainShards), PARAM_IMPL_FOR(_priority),
             PARAM_IMPL_FOR(_name));

  struct Frame {
    std::vector<input::ConsumableEvent> events;
    bool canReceiveInput{};
    bool isCursorWithinRegion{};
    InputRegion region;

    void clear() {
      canReceiveInput = false;
      events.clear();
    }
  };
  using EventBuffer = input::EventBuffer<Frame>;

  struct InputThreadHandler : public IInputHandler, public debug::IDebug {
    Brancher brancher;
    CapturedVariablesQueue variables{32};
    CapturedVariables variableState;

    std::string name;

    // This is the main thread's event buffer
    EventBuffer &eventBuffer;

    std::atomic<bool> requestFocus{};
    std::atomic<bool> wantsPointerInput{};
    std::atomic<bool> wantsKeyboardInput{};

    bool canReceiveInput{};

    int priority{};

    int4 mappedRegion;

    InputThreadHandler(EventBuffer &eventBuffer, int priority) : eventBuffer(eventBuffer), priority(priority) {}

    const std::vector<input::ConsumableEvent> *getDebugFrame(size_t frameIndex) override {
      for (auto &[gen, frame] : eventBuffer.getEvents(frameIndex, frameIndex + 1)) {
        return &frame.events;
      }
      return nullptr;
    }
    size_t getLastFrameIndex() const override { return eventBuffer.head > 0 ? eventBuffer.head - 1 : 0; }
    const char *getDebugName() override { return name.c_str(); }
    debug::ConsumeFlags getDebugConsumeFlags() const override {
      return debug::ConsumeFlags{
          .canReceiveInput = canReceiveInput,
          .requestFocus = requestFocus,
          .wantsPointerInput = wantsPointerInput,
          .wantsKeyboardInput = wantsKeyboardInput,
      };
    }

    int getPriority() const override { return priority; }

    // Run on main thread activation
    void pushCapturedVariables(const VariableRefs &variableRefs) {
      CapturedVariables newVariables;
      for (auto &vr : variableRefs) {
        newVariables.variables[vr.first] = *vr.second;
      }
      variables.push(newVariables);
    }

    void warmup(const VariableRefs &initialVariableRefs) {
      pushCapturedVariables(initialVariableRefs);
      applyCapturedVariables();
      brancher.schedule();
    }

    bool wantToConsumeEvent(const Event &event) {
      if (wantsPointerInput) {
        if (std::get_if<PointerMoveEvent>(&event) || std::get_if<PointerButtonEvent>(&event) ||
            std::get_if<ScrollEvent>(&event)) {
          return true;
        }
      }
      if (wantsKeyboardInput) {
        if (std::get_if<KeyEvent>(&event) || std::get_if<TextEvent>(&event) || std::get_if<TextCompositionEvent>(&event) ||
            std::get_if<TextCompositionEndEvent>(&event)) {
          return true;
        }
      }
      return false;
    }

    bool isCursorWithinRegion{};

    // Runs on input thread callback
    void handle(const InputState &state, std::vector<ConsumableEvent> &events, FocusTracker &focusTracker) override {

      float2 pixelCursorPos = state.cursorPosition * (float2(state.region.pixelSize) / state.region.size);
      isCursorWithinRegion = pixelCursorPos.x >= mappedRegion.x && pixelCursorPos.x <= mappedRegion[2] &&
                             pixelCursorPos.y >= mappedRegion.y && pixelCursorPos.y <= mappedRegion[3];

      // Only request focus if cursor is within region or we already have focus
      if (requestFocus && (isCursorWithinRegion || focusTracker.hasFocus(this))) {
        canReceiveInput = focusTracker.requestFocus(this);
      } else if (isCursorWithinRegion) {
        canReceiveInput = focusTracker.canReceiveInput(this);
      } else {
        canReceiveInput = false;
      }

      auto &frame = eventBuffer.getNextFrame();
      frame.region = state.region;
      frame.canReceiveInput = canReceiveInput;
      frame.isCursorWithinRegion = isCursorWithinRegion;
      for (auto &event : events) {
        frame.events.push_back(event);

        if (wantToConsumeEvent(event.event) && canReceiveInput) {
          frame.canReceiveInput = true;
          event.consumed = true;
        }
      }

      eventBuffer.nextFrame();

      if (!brancher.wires.empty()) {
        // Update captured variable references
        applyCapturedVariables();

        brancher.activate();
      }
    }

  private:
    void applyCapturedVariables() {
      variables.consume_all([this](CapturedVariables &vars) {
        variableState = vars;
        for (auto &vr : variableState.variables) {
          brancher.mesh->addRef(ToSWL(vr.first), &vr.second);
        }
      });
    }
  };

  std::shared_ptr<InputThreadHandler> _handler;
  VariableRefs _capturedVariables;
  SHVar *_contextVarRef{};
  DetachedInputContext _inputContext{};

  // The channel that receives input events from the input thread
  EventBuffer _eventBuffer;
  size_t _lastReceivedEventBufferFrame{};

  // Separate timer for this input context
  shards::Time::DeltaTimer _deltaTimer;

  bool _hasParentInputContext{};

  Detached() { _priority = Var(0); }

  bool hasInputThreadCallback() const { return _inputShards.valueType != SHType::None; }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    _handler = std::make_shared<InputThreadHandler>(_eventBuffer, (int)*_priority);
    if (hasInputThreadCallback()) {
      _handler->brancher.addRunnable(_inputShards);
      _handler->brancher.compose(data);
    }

    SHTypeInfo outputType{};
    if (_mainShards) {
      SHInstanceData innerData = data;

      ExposedInfo innerExposedInfo{data.shared};
      innerExposedInfo.push_back(IInputContext::VariableInfo);
      innerData.shared = (SHExposedTypesInfo)innerExposedInfo;

      auto result = _mainShards.compose(innerData);
      outputType = result.outputType;
    } else {
      outputType = data.inputType;
    }

    if (findExposedVariable(data.shared, decltype(_inputContext)::VariableName)) {
      _hasParentInputContext = true;
    }

    return outputType;
  }

  void warmup(SHContext *context) {
    _contextVarRef = referenceVariable(context, IInputContext::VariableName);

    withObjectVariable(*_contextVarRef, &_inputContext, IInputContext::Type, [&] { PARAM_WARMUP(context); });

    _deltaTimer.reset();
    _inputContext.time = 0.0f;
    _lastReceivedEventBufferFrame = 0;
  }

  void cleanup(SHContext* context) {
    if (_contextVarRef)
      withObjectVariable(*_contextVarRef, &_inputContext, IInputContext::Type, [&] { PARAM_CLEANUP(context); });

    cleanupCaptures();

    if (_contextVarRef) {
      releaseVariable(_contextVarRef);
      _contextVarRef = nullptr;
    }
  }

  auto &getWindowContext() { return varAsObjectChecked<WindowContext>(_context.get(), WindowContextType); }

  void init(SHContext *context) {
    assert(_handler && "Handler not initialized");

    auto &windowCtx = getWindowContext();
    _inputContext.window = windowCtx.window;
    _inputContext.master = &windowCtx.inputMaster;

    // Initial window size
    _inputContext.detached.state.region = getWindowInputRegion(*_inputContext.window.get());

    if (hasInputThreadCallback()) {
      // Setup input thread callback
      intializeCaptures(context);

      _handler->warmup(_capturedVariables);
    }

    _handler->name = !_name->isNone() ? SHSTRVIEW(*_name) : "";

    windowCtx.inputMaster.addHandler(_handler);
  }

  void intializeCaptures(SHContext *context) {
    assert(_handler && "Handler not initialized");

    auto &brancher = _handler->brancher;
    for (const auto &req : brancher.getMergedRequirements()._innerInfo) {
      if (!_capturedVariables.contains(req.name)) {
        SHVar *vp = referenceVariable(context, req.name);
        _capturedVariables.insert_or_assign(req.name, vp);
      }
    }
  }

  void cleanupCaptures() {
    for (auto &[key, value] : _capturedVariables) {
      releaseVariable(value);
    }
    _capturedVariables.clear();
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    assert(_handler && "Handler not initialized");

    IInputContext *parentContext{};
    if (_hasParentInputContext) {
      parentContext = &varAsObjectChecked<IInputContext>(*_contextVarRef, IInputContext::Type);
    }

    if (!_inputContext.window) {
      init(context);
    } else {
      // Update variables used by input thread callback
      _handler->pushCapturedVariables(_capturedVariables);
    }

    SHVar output{};
    if (_mainShards) {
      withObjectVariable(*_contextVarRef, &_inputContext, IInputContext::Type, [&] {
        double deltaTime = _deltaTimer.update();
        _inputContext.deltaTime = deltaTime;
        _inputContext.time += deltaTime;
        _inputContext.consumeFlags = ConsumeFlags{};

        // Push root input region
        auto &inputStack = _inputContext.inputStack;
        inputStack.reset();
        if (parentContext) {
          // Inherit parent context's input stack
          inputStack.push(InputStack::Item(parentContext->getInputStack().getTop()));
        } else {
          inputStack.push(input::InputStack::Item{
              .windowMapping = input::WindowSubRegion::fromEntireWindow(*_inputContext.window.get()),
          });
        }

        if (inputStack.getTop().windowMapping) {
          auto &windowRegion = inputStack.getTop().windowMapping.value();
          std::visit(
              [&](const auto &arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, input::WindowSubRegion>) {
                  _handler->mappedRegion = arg.region;
                } else {
                  throw std::logic_error("Unsupported window region");
                }
              },
              windowRegion);
        }

        Frame *mostRecentFrame{};
        _inputContext.detached.update([&](auto &apply) {
          bool logStarted{};
          auto eventUpdate = _eventBuffer.getEvents(_lastReceivedEventBufferFrame);
          for (auto &[gen, frame] : eventUpdate.frames) {
            for (ConsumableEvent &event : frame.events) {
              if (!logStarted) {
                SPDLOG_LOGGER_DEBUG(logger, "== Detached input begin {} == ", _handler->name);
                logStarted = true;
              }
              SPDLOG_LOGGER_DEBUG(logger, "Detached event IN: {}, gen: {}, consumed: {}, focus: {}", debugFormat(event.event),
                                  gen, event.consumed, frame.canReceiveInput);
              apply(event, frame.canReceiveInput);
            }

            mostRecentFrame = &frame;
          }
          _lastReceivedEventBufferFrame = eventUpdate.lastGeneration;
        });

        // Update input region
        if (mostRecentFrame) {
          _inputContext.detached.state.region = mostRecentFrame->region;
        }

        for (auto &evt : _inputContext.detached.virtualInputEvents) {
          if (!std::get_if<PointerMoveEvent>(&evt))
            SPDLOG_LOGGER_DEBUG(logger, "Detached event OUT: {}", debugFormat(evt));
        }

        _mainShards.activate(context, input, output); //

        inputStack.pop();

        // Update consume callbacks
        _handler->wantsKeyboardInput = _inputContext.consumeFlags.wantsKeyboardInput;
        _handler->wantsPointerInput = _inputContext.consumeFlags.wantsPointerInput;
        _handler->requestFocus = _inputContext.consumeFlags.requestFocus;
      });
    } else {
      output = input;
    }

    return output;
  }
};

#if SHARDS_WITH_EGUI
#define HAS_INPUT_DEBUG_UI 1
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
  void cleanup(SHContext* context) {
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
#endif
} // namespace input
} // namespace shards

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

SHARDS_REGISTER_FN(inputs1) {
  using namespace shards::input;
  REGISTER_SHARD("Inputs.GetContext", GetWindowContext);
  REGISTER_SHARD("Inputs.Detached", Detached);

#if HAS_INPUT_DEBUG_UI
  REGISTER_SHARD("Inputs.DebugUI", DebugUI);
#endif
}
