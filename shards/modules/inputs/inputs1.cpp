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
#include "inputs.hpp"
#include "debug_ui.hpp"
#include <boost/lockfree/spsc_queue.hpp>

namespace shards {
namespace input {

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

  void cleanup() { _context.cleanup(); }

  SHVar activate(SHContext *shContext, const SHVar &input) { return _context.asVar(); }
};

using VariableRefs = std::unordered_map<std::string, SHVar *>;

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

    void clear() {
      canReceiveInput = false;
      events.clear();
    }
  };
  using EventBuffer = input::EventBuffer<Frame>;

  struct InputThreadHandler : public IInputHandler {
    Brancher brancher;
    CapturedVariablesQueue variables{32};
    CapturedVariables variableState;

    std::string name;

    // This is the main thread's event buffer
    EventBuffer &eventBuffer;

    std::atomic<bool> requestFocus{};
    std::atomic<bool> wantsPointerInput{};
    std::atomic<bool> wantsKeyboardInput{};

    int priority{};

    InputThreadHandler(EventBuffer &eventBuffer, int priority) : eventBuffer(eventBuffer), priority(priority) {}

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

    // Runs on input thread callback
    void handle(const InputState &state, std::vector<ConsumableEvent> &events, FocusTracker &focusTracker) override {
      bool canReceiveInput{};
      bool requestedFocus{};
      if (requestFocus) {
        canReceiveInput = requestedFocus = focusTracker.requestFocus(this);
      } else {
        canReceiveInput = focusTracker.canReceiveInput(this);
      }

      auto &frame = eventBuffer.getNextFrame();
      frame.canReceiveInput = canReceiveInput;
      for (auto &event : events) {
        frame.events.push_back(event);

        if (wantToConsumeEvent(event.event) && canReceiveInput) {
          requestedFocus = true;
          frame.canReceiveInput = true;
          event.consumed = true;
        }
      }

      if (!frame.canReceiveInput || requestedFocus) {
        SPDLOG_INFO("DI {}, canReceiveInput {}, requestedFocus {}", name, frame.canReceiveInput, requestedFocus);
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
          brancher.mesh->refs[vr.first] = &vr.second;
        }
      });
    }
  };

  std::shared_ptr<InputThreadHandler> _handler;
  VariableRefs _capturedVariables;
  SHVar *_contextVarRef{};
  InputContext _inputContext{};

  // The channel that receives input events from the input thread
  EventBuffer _eventBuffer;
  size_t _lastReceivedEventBufferFrame{};

  // Separate timer for this input context
  shards::Time::DeltaTimer _deltaTimer;

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
      innerExposedInfo.push_back(InputContext::VariableInfo);
      innerData.shared = (SHExposedTypesInfo)innerExposedInfo;

      auto result = _mainShards.compose(innerData);
      outputType = result.outputType;
    } else {
      outputType = data.inputType;
    }

    return outputType;
  }

  void warmup(SHContext *context) {
    _contextVarRef = referenceVariable(context, InputContext::VariableName);

    withObjectVariable(*_contextVarRef, &_inputContext, InputContext::Type, [&] { PARAM_WARMUP(context); });

    _deltaTimer.reset();
    _inputContext.time = 0.0f;
    _lastReceivedEventBufferFrame = 0;
  }

  void cleanup() {
    withObjectVariable(*_contextVarRef, &_inputContext, InputContext::Type, [&] { PARAM_CLEANUP(); });

    _handler.reset();
    cleanupCaptures();

    if (_contextVarRef) {
      releaseVariable(_contextVarRef);
    }
  }

  auto &getWindowContext() { return varAsObjectChecked<WindowContext>(_context.get(), WindowContextType); }

  void init(SHContext *context) {
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

    _handler->name = !_name->isNone() ? (const char *)*_name : "";

    windowCtx.inputMaster.addHandler(_handler);
  }

  void intializeCaptures(SHContext *context) {
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
    if (!_inputContext.window) {
      init(context);
    } else {
      // Update variables used by input thread callback
      _handler->pushCapturedVariables(_capturedVariables);
    }

    SHVar output{};
    if (_mainShards) {
      withObjectVariable(*_contextVarRef, &_inputContext, InputContext::Type, [&] {
        double deltaTime = _deltaTimer.update();
        _inputContext.deltaTime = deltaTime;
        _inputContext.time += deltaTime;

        _inputContext.detached.update([&](auto &apply) {
          bool logStarted{};
          auto eventUpdate = _eventBuffer.getEvents(_lastReceivedEventBufferFrame);
          for (auto &[gen, frame] : eventUpdate.frames) {
            for (ConsumableEvent &event : frame.events) {
              if (!logStarted) {
                SPDLOG_INFO("== Detached input begin == ");
                logStarted = true;
              }
              SPDLOG_INFO("Detached event IN: {}, gen: {}, consumed: {}, focus: {}", debugFormat(event.event), gen,
                          event.consumed, frame.canReceiveInput);
              apply(event, frame.canReceiveInput);
            }
          }
          _lastReceivedEventBufferFrame = eventUpdate.lastGeneration;
        });

        for (auto &evt : _inputContext.detached.virtualInputEvents) {
          if (!std::get_if<PointerMoveEvent>(&evt))
            SPDLOG_INFO("Detached event OUT: {}", debugFormat(evt));
        }

        _mainShards.activate(context, input, output); //

        // Update consume callbacks
        _handler->wantsKeyboardInput = _inputContext.wantsKeyboardInput;
        _handler->wantsPointerInput = _inputContext.wantsPointerInput;
        _handler->requestFocus = _inputContext.requestFocus;
      });
    } else {
      output = input;
    }

    // _handler->updateCapturedVariables(SHContext * mainContext)

    // Need to capture variables
    // _handler->capturedVariables

    // _inputContext.input.

    // windowContext.window->pollEvents(_events);
    // windowContext.
    // _state.update();
    // Poll here
    // windowContext

    return output;
  }
};

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
  std::vector<std::shared_ptr<IInputHandler>> _handlers;

  SHVar activate(SHContext *shContext, const SHVar &input) {
    _layers.clear();
    _strings.clear();

    _context->master->getHandlers(_handlers);
    for (auto &handler : _handlers) {
      auto &layer = _layers.emplace_back();
      layer.priority = handler->getPriority();

      if (debug::IDebug *debug = dynamic_cast<debug::IDebug *>(handler.get())) {
        auto &str = _strings.emplace_back(debug->getDebugName());
        layer.name = str.c_str();
      }
    }

    showInputDebugUI(_uiContext, _layers.data(), _layers.size());
    return SHVar{};
  }
};
} // namespace input
} // namespace shards

SHARDS_REGISTER_FN(inputs1) {
  using namespace shards::input;
  REGISTER_SHARD("Inputs.GetContext", GetWindowContext);
  REGISTER_SHARD("Inputs.Detached", Detached);
  REGISTER_SHARD("Inputs.DebugUI", DebugUI);
}
