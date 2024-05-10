#include <shards/modules/gfx/gfx.hpp>
#include <shards/modules/gfx/window.hpp>
#include <shards/core/params.hpp>
#include <shards/core/capturing_brancher.hpp>
#include <shards/iterator.hpp>
#include <shards/core/object_var_util.hpp>
#include <shards/iterator.hpp>
#include <shards/modules/core/time.hpp>
#include <shards/input/window_input.hpp>
#include <shards/input/window_mapping.hpp>
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
#include "shards/shards.h"
#include "spdlog/spdlog.h"
#include <boost/lockfree/spsc_queue.hpp>
#include <stdexcept>

namespace shards {
namespace input {

static auto logger = getLogger();

using VarQueue = boost::lockfree::spsc_queue<OwnedVar>;

using CapturedVariables = CapturingBrancher::CapturedVariables;
struct InputThreadInput {
  CapturedVariables variables;
  InputStack::Item inputStackState;

  InputThreadInput() = default;
  InputThreadInput(CapturedVariables &&variables, const InputStack::Item &inputStackState)
      : variables(std::move(variables)), inputStackState(inputStackState) {}
  InputThreadInput(InputThreadInput &&) = default;
  InputThreadInput(const InputThreadInput &) = default;
  InputThreadInput &operator=(InputThreadInput &&) = default;
};
using InputThreadInputQueue = boost::lockfree::spsc_queue<InputThreadInput *>;

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

  const input::InputState &getState() const override { return master->getState(); }
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
  std::optional<std::string> exceptionString;
};
using OutputBuffer = boost::lockfree::spsc_queue<OutputFrame>;

struct InputThreadHandler : public std::enable_shared_from_this<InputThreadHandler>, public IInputHandler, public debug::IDebug {
  CapturingBrancher brancher;
  InputThreadInputQueue inputQueue{32};
  InputThreadInputQueue recycleQueue{32};

  DetachedInputContext inputContext;
  InputStack::Item lastInputStackState;

  std::string name;

  OutputBuffer &outputBuffer;

  bool canReceiveInput{};
  int priority{};
  int4 mappedRegion;
  shards::Time::DeltaTimer deltaTimer;

  OwnedVar inputContextVar;

  CapturingBrancher::CloningContext brancherCloningContext;

  bool warmedUp{};

  InputThreadHandler(OutputBuffer &outputBuffer, int priority) : outputBuffer(outputBuffer), priority(priority) {}
  ~InputThreadHandler() {
    brancher.cleanup();
    auto clean = [&](InputThreadInput *input) { delete input; };
    inputQueue.consume_all(clean);
    recycleQueue.consume_all(clean);
  }

  const char *getDebugName() override { return name.c_str(); }

  int getPriority() const override { return priority; }

  // Run on main thread activation
  void pushInputs(const VariableRefs &variableRefs, const InputStack::Item &inputStackState) {
    InputThreadInput *input{};
    if (!recycleQueue.empty()) {
      recycleQueue.consume_one([&](InputThreadInput *recycled) { input = recycled; });
    }
    if (!input)
      input = new InputThreadInput();
    input->inputStackState = inputStackState;

    for (auto &vr : variableRefs) {
      vr.second.cloneInto(input->variables[vr.first], brancherCloningContext);
    }
    inputQueue.push(input);
  }

  void warmup() {
    inputContextVar = Var::Object(&inputContext, IInputContext::Type);
    inputContextVar.flags = SHVAR_FLAGS_FOREIGN | SHVAR_FLAGS_REF_COUNTED;
    inputContextVar.refcount = 1;
    brancher.mesh()->addRef(toSWL(RequiredInputContext::variableName()), &inputContextVar);

    inputContext.handler = this->weak_from_this();

    brancher.warmup();
    warmedUp = true;
  }

  bool isCursorWithinRegion{};

  // Runs on input thread callback
  void handle(InputMaster &master) override {
    // Update captured variable references
    applyCapturedVariables();

    if (!warmedUp) {
      warmup();
    }

    auto &inputStack = inputContext.inputStack;
    inputStack.push(InputStack::Item(lastInputStackState));
    DEFER({ inputStack.pop(); });

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

    double deltaTime = deltaTimer.update();
    inputContext.deltaTime = deltaTime;
    inputContext.time += deltaTime;

    auto &state = master.getState();

    float2 pixelCursorPos = state.cursorPosition * (float2(state.region.pixelSize) / state.region.size);
    isCursorWithinRegion = pixelCursorPos.x >= mappedRegion.x && pixelCursorPos.x <= mappedRegion[2] &&
                           pixelCursorPos.y >= mappedRegion.y && pixelCursorPos.y <= mappedRegion[3];

    try {
      brancher.activate();
    } catch (std::exception &ex) {
      outputBuffer.push(OutputFrame{.exceptionString = ex.what()});
    }

    {
      ZoneScopedN("CopyOutputs");
      auto &mainWire = brancher.wires().back();
      if (mainWire->state != SHWire::State::Iterating) {
        outputBuffer.push(OutputFrame{mainWire->previousOutput});
      }
    }
  }

private:
  void applyCapturedVariables() {
    {
      ZoneScopedN("ApplyVariables");
      int avail = inputQueue.read_available();
      if (avail > 0) {
        // Discard extra elements
        for (int i = 0; i < (avail - 1); i++)
          inputQueue.consume_one([&](auto input) { recycleQueue.push(input); });

        // Consume last item
        inputQueue.consume_one([&](InputThreadInput *input) {
          brancher.applyCapturedVariablesSwap(input->variables);
          lastInputStackState = input->inputStackState;
          recycleQueue.push(input);
        });
      }
    }
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
  PARAM_PARAMVAR(_windowRegion, "WindowRegion", "Sets the window region for input handling.",
                 {CoreInfo::NoneType, CoreInfo::Int4Type, CoreInfo::Int4VarType});
  PARAM_VAR(_name, "Name", "Name used for logging/debugging purposes", {CoreInfo::NoneType, CoreInfo::StringType});
  PARAM_IMPL(PARAM_IMPL_FOR(_context), PARAM_IMPL_FOR(_inputShards), PARAM_IMPL_FOR(_mainShards), PARAM_IMPL_FOR(_priority),
             PARAM_IMPL_FOR(_windowRegion), PARAM_IMPL_FOR(_name));

  std::shared_ptr<InputThreadHandler> _handler;
  VariableRefs _capturedVariables;

  OptionalInputContext _inputContext{};

  // The channel that receives input events from the input thread
  boost::lockfree::spsc_queue<OutputFrame> _outputBuffer{256};

  Types _mainDataSeqTypes;
  SeqVar _mainDataSeq;

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

    ExposedInfo branchShared{RequiredInputContext::getExposedTypeInfo()};
    _handler->brancher.compose(data, branchShared, IgnoredVariables);

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

  auto &getWindowContext() { return varAsObjectChecked<WindowContext>(_context.get(), WindowContextType); }

  void cleanup(SHContext *context) {
    if (_inputContext) {
      auto &master = _inputContext->getMaster();
      master.removeHandler(_handler);
    }

    PARAM_CLEANUP(context);
    _inputContext.cleanup(context);
    cleanupCaptures();
  }

  void init(SHContext *context, InputStack::Item inputStackState) {
    auto &windowCtx = getWindowContext();

    // Setup input thread callback
    _handler->brancher.intializeCaptures(_capturedVariables, context, IgnoredVariables);
    _handler->pushInputs(_capturedVariables, inputStackState);
    _handler->name = !_name->isNone() ? SHSTRVIEW(*_name) : "";
    _handler->inputContext.window = windowCtx.window;
    _handler->inputContext.master = &windowCtx.inputMaster;

    windowCtx.inputMaster.addHandler(_handler);

    _initialized = true;
  }

  void cleanupCaptures() { _capturedVariables.clear(); }

  SHVar activate(SHContext *context, const SHVar &input) {
    ZoneScoped;
#if TRACY_ENABLE
    auto name = fmt::format("Detached {}", SHSTRVIEW(_name));
    ZoneName(name.data(), name.size());
#endif

    IInputContext *parentContext{};
    if (_inputContext) {
      parentContext = _inputContext.get();
    }

    {
      ZoneScopedN("CopyInputs");

      // Push root input region
      InputStack::Item inputStackState;
      if (parentContext) {
        // Inherit parent context's input stack
        inputStackState = parentContext->getInputStack().getTop();
      } else {
        inputStackState = input::InputStack::Item{
            .windowMapping = input::WindowSubRegion::fromEntireWindow(*getWindowContext().window.get()),
        };
      }

      Var &windowRegionVar = (Var &)_windowRegion.get();
      if (!windowRegionVar.isNone()) {
        int4 region = toVec<int4>(windowRegionVar);
        inputStackState.windowMapping = input::WindowSubRegion{.region = region};
      }

      if (!_initialized) {
        init(context, inputStackState);
      } else {
        // Update variables used by input thread callback
        _handler->pushInputs(_capturedVariables, inputStackState);
      }
    }
    {
      ZoneScopedN("CopyOutputs");
      if (_outputBuffer.read_available() > 0) {
        _mainDataSeq.clear();
        _outputBuffer.consume_all([&](OutputFrame &frame) {
          if (frame.exceptionString) {
            throw ActivationError(fmt::format("Input branch had errors: {}", frame.exceptionString.value()));
          }
          _mainDataSeq.push_back(frame.data);
        });
      } else if (_mainDataSeq.size() > 1) {
        auto lastOutput = std::move(_mainDataSeq.back());
        _mainDataSeq.resize(1);
        _mainDataSeq[0] = std::move(lastOutput);
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
