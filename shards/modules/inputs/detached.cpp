#include <shards/modules/gfx/gfx.hpp>
#include <shards/modules/gfx/window.hpp>
#include <shards/core/params.hpp>
#include <shards/core/capturing_brancher.hpp>
#include <shards/iterator.hpp>
#include <shards/core/object_var_util.hpp>
#include <shards/iterator.hpp>
#include <shards/modules/core/time.hpp>
#include <shards/input/window_input.hpp>
#include <shards/input/debug.hpp>
#include <shards/input/state.hpp>
#include <shards/input/detached.hpp>
#include <shards/core/module.hpp>
#include <input/events.hpp>
#include <input/input.hpp>
#include <input/input_stack.hpp>
#include <input/log.hpp>
#include "inputs.hpp"
#include "debug_ui.hpp"
#include <boost/lockfree/spsc_queue.hpp>
#include <stdexcept>

namespace shards {
namespace input {

static auto logger = getLogger();

using VarQueue = boost::lockfree::spsc_queue<OwnedVar>;

using CapturedVariables = CapturingBrancher::CapturedVariables;
struct InputThreadInput {
  CapturedVariables variables;
  InputStack inputStack;
};
using InputThreadInputQueue = boost::lockfree::spsc_queue<InputThreadInput>;

static inline Type WindowContextType = WindowContext::Type;
static inline CapturingBrancher::IgnoredVariables IgnoredVariables{IInputContext::VariableName};

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

using VariableRefs = CapturingBrancher::VariableRefs;

struct DetachedInputContext : public IInputContext {
  std::shared_ptr<gfx::Window> window;
  input::InputMaster *master{};
  std::weak_ptr<IInputHandler> handler;
  DetachedInput detachedState;

  double time{};
  float deltaTime{};
  InputStack inputStack;

  InputStack &getInputStack() override { return inputStack; }
  shards::input::InputMaster &getMaster() const override {
    assert(master);
    return *master;
  }

  const std::weak_ptr<IInputHandler> &getHandler() const override { return handler; }

  const input::InputState &getState() const override { return detachedState.state; }
  std::vector<ConsumableEvent> &getEvents() override { return master->getEvents(); }

  float getTime() const override { return time; }
  float getDeltaTime() const override { return deltaTime; }

  void postMessage(const input::Message &message) override { master->postMessage(message); }

  bool requestFocus() override {
    if (auto ptr = getHandler().lock()) {
      auto &focusTracker = getMaster().getFocusTracker();
      return focusTracker.requestFocus(ptr.get());
    }
    return false;
  }

  bool canReceiveInput() const override {
    if (auto ptr = getHandler().lock()) {
      auto &focusTracker = getMaster().getFocusTracker();
      return focusTracker.canReceiveInput(ptr.get());
    }
    return false;
  }
};

struct OutputFrame {
  OwnedVar data;
  void clear() { data = Var{}; }
};
using OutputBuffer = input::EventBuffer<OutputFrame>;

struct InputThreadHandler : public std::enable_shared_from_this<InputThreadHandler>, public IInputHandler, public debug::IDebug {
  CapturingBrancher brancher;
  InputThreadInputQueue inputQueue{32};
  CapturedVariables variableState;

  DetachedInputContext inputContext;

  std::string name;

  OutputBuffer &outputBuffer;

  bool canReceiveInput{};
  int priority{};
  int4 mappedRegion;
  shards::Time::DeltaTimer deltaTimer;

  InputThreadHandler(OutputBuffer &outputBuffer, int priority) : outputBuffer(outputBuffer), priority(priority) {}

  const char *getDebugName() override { return name.c_str(); }

  int getPriority() const override { return priority; }

  // Run on main thread activation
  void pushInputs(const VariableRefs &variableRefs, InputStack &&inputStack) {
    CapturedVariables newVariables;
    for (auto &vr : variableRefs) {
      newVariables[vr.first] = *vr.second;
    }
    inputQueue.push(InputThreadInput{newVariables, inputStack});
  }

  void warmup(const VariableRefs &initialVariableRefs, InputStack &&inputStack) {
    Var inputContextVar = Var::Object(&inputContext, IInputContext::Type);
    brancher.mesh()->variables.emplace(RequiredInputContext::variableName(), inputContextVar);

    inputContext.handler = this->weak_from_this();

    pushInputs(initialVariableRefs, std::move(inputStack));
    applyCapturedVariables();
    brancher.warmup();
  }

  bool isCursorWithinRegion{};

  // Runs on input thread callback
  void handle(InputMaster &master) override {
    auto &inputStack = inputContext.inputStack;
    auto baseRegion = getWindowInputRegion(*inputContext.window.get());
    mappedRegion = int4(0, 0, baseRegion.pixelSize.x, baseRegion.pixelSize.y);
    if (inputStack.getTop().windowMapping) {
      auto &windowRegion = inputStack.getTop().windowMapping.value();
      std::visit(
          [&](const auto &arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, input::WindowSubRegion>) {
              mappedRegion = arg.region;
            } else {
              throw std::logic_error("Unsupported window region");
            }
          },
          windowRegion);
    }

    auto canReceiveInput = inputContext.canReceiveInput();

    inputContext.detachedState.update([&](auto apply) {
      for (auto &evt : master.getEvents()) {
        apply(evt, canReceiveInput);
      }
    });
    inputContext.detachedState.state.region = master.getState().region;

    double deltaTime = deltaTimer.update();
    inputContext.deltaTime = deltaTime;
    inputContext.time += deltaTime;

    auto &state = master.getState();

    float2 pixelCursorPos = state.cursorPosition * (float2(state.region.pixelSize) / state.region.size);
    isCursorWithinRegion = pixelCursorPos.x >= mappedRegion.x && pixelCursorPos.x <= mappedRegion[2] &&
                           pixelCursorPos.y >= mappedRegion.y && pixelCursorPos.y <= mappedRegion[3];

    // Only request focus if cursor is within region or we already have focus
    // if (/* requestFocus && */ (isCursorWithinRegion || focusTracker.hasFocus(this))) {
    //   canReceiveInput = focusTracker.requestFocus(this);
    // } else if (isCursorWithinRegion) {
    //   canReceiveInput = focusTracker.canReceiveInput(this);
    // } else {
    //   canReceiveInput = false;
    // }

    // Update captured variable references
    applyCapturedVariables();
    brancher.activate();

    auto &mainWire = brancher.wires().back();

    auto &frame = outputBuffer.getNextFrame();
    frame.data = mainWire->previousOutput;
    outputBuffer.nextFrame();
  }

private:
  void applyCapturedVariables() {
    inputQueue.consume_all([this](InputThreadInput &input) {
      variableState = std::move(input.variables);
      brancher.applyCapturedVariables(variableState);
      inputContext.inputStack = std::move(input.inputStack);
    });
  }
};

struct Detached {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static SHOptionalString help() {
    return SHCCSTR("Runs the contents on the input thread, and it's continuation on the current thread with the last data from "
                   "the input thread");
  }

  PARAM_PARAMVAR(_context, "Context", "The window context to attach to", {Type::VariableOf(WindowContextType)});
  PARAM_VAR(_inputShards, "Input", "Runs detached on the input loop", IntoWire::RunnableTypes);
  PARAM(ShardsVar, _mainShards, "Then", "Runs inline after data has been output from the Input callback",
        {CoreInfo::ShardsOrNone});
  PARAM_VAR(_priority, "Priority", "The order in which this input handler is run", {CoreInfo::IntType});
  PARAM_VAR(_name, "Name", "Name used for logging/debugging purposes", {CoreInfo::NoneType, CoreInfo::StringType});
  PARAM_IMPL(PARAM_IMPL_FOR(_context), PARAM_IMPL_FOR(_inputShards), PARAM_IMPL_FOR(_mainShards), PARAM_IMPL_FOR(_priority),
             PARAM_IMPL_FOR(_name));

  std::shared_ptr<InputThreadHandler> _handler;
  VariableRefs _capturedVariables;

  OptionalInputContext _inputContext{};

  // The channel that receives input events from the input thread
  OutputBuffer _outputBuffer;
  size_t _lastReceivedEventBufferFrame{};

  Types _mainDataSeqTypes;
  SeqVar _mainDataSeq;
  size_t _lastGeneration;

  bool _initialized{};

  Detached() { _priority = Var(0); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    _handler = std::make_shared<InputThreadHandler>(_outputBuffer, (int)*_priority);
    auto name = fmt::format("detached {}", SHSTRVIEW(_name));
    _handler->brancher.addRunnable(_inputShards, name.c_str());
    if (_handler->brancher.wires().empty()) {
      throw ComposeError(fmt::format("Detached input must an Input runnable"));
    }

    ExposedInfo branchShared{data.shared};
    branchShared.push_back(RequiredInputContext::getExposedTypeInfo());
    auto branchInstanceData = data;
    branchInstanceData.shared = (SHExposedTypesInfo)branchShared;
    _handler->brancher.compose(branchInstanceData, IgnoredVariables);

    for (auto req : _handler->brancher.requiredVariables()) {
      _requiredVariables.push_back(req);
    }

    auto &inputWire = _handler->brancher.wires().back();
    if (!inputWire->composeResult)
      throw ComposeError(fmt::format("Failed to compose input wire"));
    if (inputWire->composeResult->failed)
      throw ComposeError(fmt::format("Failed to compose input wire: {}", inputWire->composeResult->failureMessage));

    _mainDataSeqTypes = Types{inputWire->outputType};
    auto mainDataSeqType = Type::SeqOf(_mainDataSeqTypes);

    auto mainInstanceData = data;
    mainInstanceData.inputType = mainDataSeqType;
    auto mainCr = _mainShards.compose(mainInstanceData);
    for (auto req : mainCr.requiredInfo) {
      _requiredVariables.push_back(req);
    }

    return mainDataSeqType;
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _inputContext.warmup(context);
  }

  void cleanup() {
    PARAM_CLEANUP();
    _inputContext.cleanup();
    _handler.reset();
    cleanupCaptures();
  }

  auto &getWindowContext() { return varAsObjectChecked<WindowContext>(_context.get(), WindowContextType); }

  void init(SHContext *context, InputStack &&inputStack) {
    auto &windowCtx = getWindowContext();

    // Setup input thread callback
    _handler->brancher.intializeCaptures(_capturedVariables, context, IgnoredVariables);
    _handler->warmup(_capturedVariables, std::move(inputStack));
    _handler->name = !_name->isNone() ? SHSTRVIEW(*_name) : "";
    _handler->inputContext.window = windowCtx.window;
    _handler->inputContext.master = &windowCtx.inputMaster;

    windowCtx.inputMaster.addHandler(_handler);

    _initialized = true;
  }

  void cleanupCaptures() {
    for (auto &[key, value] : _capturedVariables) {
      releaseVariable(value);
    }
    _capturedVariables.clear();
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    IInputContext *parentContext{};
    if (_inputContext) {
      parentContext = _inputContext.get();
    }

    // Push root input region
    InputStack inputStack;
    if (parentContext) {
      // Inherit parent context's input stack
      inputStack.push(InputStack::Item(parentContext->getInputStack().getTop()));
    } else {
      inputStack.push(input::InputStack::Item{
          .windowMapping = input::WindowSubRegion::fromEntireWindow(*getWindowContext().window.get()),
      });
    }

    if (!_initialized) {
      init(context, std::move(inputStack));
    } else {
      // Update variables used by input thread callback
      _handler->pushInputs(_capturedVariables, std::move(inputStack));
    }

    auto handlerInputContext = _handler->inputContext;

    if (!_outputBuffer.empty()) {
      _mainDataSeq.clear();
      auto result = _outputBuffer.getEvents(_lastGeneration);
      for (auto &[idx, frame] : result.frames) {
        _mainDataSeq.push_back(frame.data);
      }
      _lastGeneration = result.lastGeneration;

      // When empty, recycle last output
      if (_mainDataSeq.empty()) {
        _mainDataSeq.push_back(_outputBuffer.getFrame(_lastGeneration).data);
      }
    }

    if (_mainShards && !_mainDataSeq.empty()) {
      SHVar _unused{};
      _mainShards.activate(context, _mainDataSeq, _unused);
    }

    return _mainDataSeq;
  }
};
} // namespace input
} // namespace shards

SHARDS_REGISTER_FN(inputs_detached) {
  using namespace shards::input;
  REGISTER_SHARD("Inputs.GetContext", GetWindowContext);
  REGISTER_SHARD("Inputs.Detached", Detached);
}
