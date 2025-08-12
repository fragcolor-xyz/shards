#include "ftxui/component/captured_mouse.hpp" // for ftxui
#include "ftxui/component/component.hpp"      // for Button, Horizontal, Renderer
#include "ftxui/component/component_base.hpp" // for ComponentBase

#include "ftxui/component/screen_interactive.hpp" // for ScreenInteractive
#include "ftxui/component/loop.hpp"               // for Loop
#include "ftxui/dom/elements.hpp"                 // for separator, gauge, text, Element, operator|, vbox, border

#include <shards/shards.hpp>
#include <shards/utility.hpp>
#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>

namespace shards {
namespace tui {

struct TUIElement {
  ftxui::Component component;

  ~TUIElement() { SHLOG_TRACE("TUIElement destroyed"); }
};

struct TUIInnerElements {
  ftxui::Components components;

  void reset() { components = {}; }

  void clear() { components.clear(); }
};

struct TUITypes {
  SHVAR_OBJECT_DECL('tuiE', "TUI.Element", Element, TUIElement);
  SHVAR_OBJECT_DECL('tuie', "TUI.InnerElements", InnerElements, TUIInnerElements);
};

#define DEFINE_BOX_SHARD(ClassName, Direction, HelpText, FtxuiFunc, FtxuiContainer)                                \
  struct ClassName {                                                                                               \
    static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }                                                  \
    static SHTypesInfo outputTypes() { return TUITypes::Element; }                                                 \
    static SHOptionalString help() { return SHCCSTR(HelpText); }                                                   \
                                                                                                                   \
    PARAM(ShardsVar, _contents, "Contents", "The contents of the " Direction " box.", {CoreInfo::ShardsOrNone});   \
    PARAM_IMPL(PARAM_IMPL_FOR(_contents));                                                                         \
                                                                                                                   \
    PARAM_REQUIRED_VARIABLES();                                                                                    \
    SHTypeInfo compose(SHInstanceData &data) {                                                                     \
      PARAM_COMPOSE_REQUIRED_VARIABLES(data);                                                                      \
      _contents.compose(data);                                                                                     \
      return TUITypes::Element;                                                                                    \
    }                                                                                                              \
                                                                                                                   \
    TUIElement *_element = nullptr;                                                                                \
                                                                                                                   \
    void warmup(SHContext *context) {                                                                              \
      _innerElementsVar.warmup(context);                                                                           \
      _element = TUITypes::ElementObjectVar.New();                                                                 \
      _contents.warmup(context);                                                                                   \
    }                                                                                                              \
                                                                                                                   \
    void cleanup(SHContext *context) {                                                                             \
      _contents.cleanup(context);                                                                                  \
      _innerElements.reset();                                                                                      \
      if (_element) {                                                                                              \
        TUITypes::ElementObjectVar.Release(_element);                                                              \
        _element = nullptr;                                                                                        \
      }                                                                                                            \
      _innerElementsVar.cleanup(context);                                                                          \
    }                                                                                                              \
                                                                                                                   \
    ParamVar _innerElementsVar{Var::ContextVar("_TUI.InnerElements")};                                             \
    TUIInnerElements _innerElements;                                                                               \
    ShardsVar _action;                                                                                             \
    ftxui::Component _container = ftxui::Container::FtxuiContainer({});                                            \
                                                                                                                   \
    SHVar activate(SHContext *context, const SHVar &input) {                                                       \
      _innerElements.clear();                                                                                      \
      SHVar currentInnerElements = _innerElementsVar.get();                                                        \
      assignVariableValue(_innerElementsVar.get(), Var::Object(&_innerElements, TUITypes::InnerElements));         \
      DEFER(assignVariableValue(_innerElementsVar.get(), currentInnerElements));                                   \
      SHVar output{};                                                                                              \
      _contents.activate(context, input, output);                                                                  \
      SHLOG_TRACE("Inner components: {}", _innerElements.components.size());                                       \
      _container->DetachAllChildren();                                                                             \
      for (auto &component : _innerElements.components) {                                                          \
        _container->Add(component);                                                                                \
      }                                                                                                            \
      _element->component = _container;                                                                            \
      if (currentInnerElements.valueType == SHType::Object) {                                                      \
        auto &innerElements = varAsObjectChecked<TUIInnerElements>(currentInnerElements, TUITypes::InnerElements); \
        innerElements.components.push_back(_element->component);                                                   \
      }                                                                                                            \
      return TUITypes::ElementObjectVar.Get(_element);                                                             \
    }                                                                                                              \
  };

DEFINE_BOX_SHARD(VBox, "vertical", "Creates a vertical box", vbox, Vertical)
DEFINE_BOX_SHARD(HBox, "horizontal", "Creates a horizontal box", hbox, Horizontal)

struct TUIText {
  TUIText() {
    _border = Var(false);
    _flex = Var(false);
    _alignRight = Var(false);
  }

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return TUITypes::Element; }
  static SHOptionalString help() { return SHCCSTR("Adds a text element to the TUI context"); }

  PARAM_PARAMVAR(_border, "Border", "Whether to draw a border around the text", {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_flex, "Flex", "Whether to expand proportionally to the space left in a container",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_alignRight, "AlignRight", "Whether to align the text to the right",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_border), PARAM_IMPL_FOR(_flex), PARAM_IMPL_FOR(_alignRight));

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return TUITypes::Element;
  }

  ParamVar _innerElementsVar{Var::ContextVar("_TUI.InnerElements")};

  TUIElement *_element = nullptr;

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _innerElementsVar.warmup(context);
    _element = TUITypes::ElementObjectVar.New();
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    _innerElementsVar.cleanup(context);
    if (_element) {
      TUITypes::ElementObjectVar.Release(_element);
      _element = nullptr;
    }
  }

  std::string _text;

  SHVar activate(SHContext *context, const SHVar &input) {
    auto text = SHSTRVIEW(input);
    _text.assign(text.data(), text.data() + text.size());

    auto border = _border.get().payload.boolValue;
    auto flex = _flex.get().payload.boolValue;
    auto alignRight = _alignRight.get().payload.boolValue;
    _element->component = ftxui::Renderer([this, border, flex, alignRight]() {
      auto element = ftxui::text(_text);
      if (alignRight) {
        element = element | ftxui::align_right;
      }
      if (border) {
        element = element | ftxui::border;
      }
      if (flex) {
        element = element | ftxui::flex;
      }
      return element;
    });

    if (_innerElementsVar.get().valueType == SHType::Object) {
      auto &innerElements = varAsObjectChecked<TUIInnerElements>(_innerElementsVar.get(), TUITypes::InnerElements);
      innerElements.components.push_back(_element->component);
    }

    return TUITypes::ElementObjectVar.Get(_element);
  }
};

struct Separator {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return TUITypes::Element; }
  static SHOptionalString help() { return SHCCSTR("Adds a separator element to the TUI context"); }

  ParamVar _innerElementsVar{Var::ContextVar("_TUI.InnerElements")};

  TUIElement *_element = nullptr;

  void warmup(SHContext *context) {
    _innerElementsVar.warmup(context);
    _element = TUITypes::ElementObjectVar.New();
  }

  void cleanup(SHContext *context) {
    _innerElementsVar.cleanup(context);
    if (_element) {
      TUITypes::ElementObjectVar.Release(_element);
      _element = nullptr;
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    _element->component = ftxui::Renderer([]() { return ftxui::separator(); });
    if (_innerElementsVar.get().valueType == SHType::Object) {
      auto &innerElements = varAsObjectChecked<TUIInnerElements>(_innerElementsVar.get(), TUITypes::InnerElements);
      innerElements.components.push_back(_element->component);
    }
    return TUITypes::ElementObjectVar.Get(_element);
  }
};

struct Button {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return TUITypes::Element; }
  static SHOptionalString help() { return SHCCSTR("Adds a button element to the TUI context."); }

  ParamVar _innerElementsVar{Var::ContextVar("_TUI.InnerElements")};

  PARAM(ShardsVar, _action, "Action", "The action to perform when the button is pressed.", {CoreInfo::ShardsOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_action));

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    _action.compose(data);
    return TUITypes::Element;
  }

  TUIElement *_element = nullptr;

  void warmup(SHContext *context) {
    _innerElementsVar.warmup(context);
    _action.warmup(context);
    _element = TUITypes::ElementObjectVar.New();
  }

  void cleanup(SHContext *context) {
    _innerElementsVar.cleanup(context);
    if (_element) {
      TUITypes::ElementObjectVar.Release(_element);
      _element = nullptr;
    }
    _action.cleanup(context);
  }

  std::string _text;
  std::optional<ftxui::Component> _button;

  SHVar activate(SHContext *context, const SHVar &input) {
    auto text = SHSTRVIEW(input);
    _text.assign(text.data(), text.data() + text.size());
    auto option = ftxui::ButtonOption::Animated();
    _button = ftxui::Button(
        _text,
        [=, this]() {
          SHVar output{};
          _action.activate(context, input, output);
        },
        option);
    _element->component = *_button;
    if (_innerElementsVar.get().valueType == SHType::Object) {
      auto &innerElements = varAsObjectChecked<TUIInnerElements>(_innerElementsVar.get(), TUITypes::InnerElements);
      innerElements.components.push_back(_element->component);
    }
    return TUITypes::ElementObjectVar.Get(_element);
  }
};

struct TUIInput {
  TUIInput() {
    _placeholder = Var("Type here...");
    _password = Var(false);
    _multiline = Var(true);
  }

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return TUITypes::Element; }
  static SHOptionalString help() { return SHCCSTR("Adds a separator element to the TUI context"); }

  PARAM_PARAMVAR(_value, "Value", "The value of the input", {CoreInfo::StringVarType});
  PARAM_PARAMVAR(_placeholder, "Placeholder", "The placeholder text for the input",
                 {CoreInfo::StringType, CoreInfo::StringVarType});
  PARAM_PARAMVAR(_password, "Password", "Whether the input is a password", {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_multiline, "Multiline", "Whether the input is multiline", {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM(ShardsVar, _onEnter, "OnEnter", "The action to perform when the input is submitted", {CoreInfo::ShardsOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_value), PARAM_IMPL_FOR(_placeholder), PARAM_IMPL_FOR(_password), PARAM_IMPL_FOR(_multiline),
             PARAM_IMPL_FOR(_onEnter));

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    if (_value.isNone()) {
      throw ComposeError("TUI.Input requires a Value variable");
    }

    _onEnter.compose(data);

    return TUITypes::Element;
  }

  ParamVar _innerElementsVar{Var::ContextVar("_TUI.InnerElements")};

  TUIElement *_element = nullptr;

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);

    _innerElementsVar.warmup(context);
    _element = TUITypes::ElementObjectVar.New();
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);

    _innerElementsVar.cleanup(context);
    if (_element) {
      TUITypes::ElementObjectVar.Release(_element);
      _element = nullptr;
    }

    _buffer = "";
    _cursorPosition = 0;
  }

  std::string _buffer;
  int _cursorPosition = 0;

  SHVar activate(SHContext *context, const SHVar &input) {
    ftxui::InputOption option = ftxui::InputOption::Spacious();
    auto placeholderStr = SHSTRVIEW(_placeholder.get());
    option.placeholder->assign(placeholderStr.data(), placeholderStr.data() + placeholderStr.size());
    option.password = _password.get().payload.boolValue;
    option.multiline = _multiline.get().payload.boolValue;
    option.on_change = [this]() {
      auto tmp = Var(std::string_view(_buffer.data(), _buffer.size()));
      cloneVar(_value.get(), tmp);
      SHLOG_TRACE("on_change: {}", _value.get());
    };
    if (_onEnter) {
      option.on_enter = [&, context, input]() {
        // Remove trailing newlines (handles both \n and \r\n)
        while (!_buffer.empty() && (_buffer.back() == '\n' || _buffer.back() == '\r')) {
          _buffer.pop_back();
        }

        // Update value variable with cleaned buffer content
        auto tmp = Var(std::string_view(_buffer.data(), _buffer.size()));
        cloneVar(_value.get(), tmp);

        // Execute the OnEnter action
        SHVar output{};
        _onEnter.activate(context, input, output);

        // Clear buffer for next input
        _buffer.clear();
        _cursorPosition = 0;

        // Update value variable to reflect cleared state
        tmp = Var("");
        cloneVar(_value.get(), tmp);
      };
    }
    option.content = &_buffer;
    option.cursor_position = &_cursorPosition;
    auto currentValue = SHSTRVIEW(_value.get());
    if (!currentValue.empty()) {
      _buffer.assign(currentValue.data(), currentValue.data() + currentValue.size());
    }
    _element->component = ftxui::Input(option);
    if (_innerElementsVar.get().valueType == SHType::Object) {
      auto &innerElements = varAsObjectChecked<TUIInnerElements>(_innerElementsVar.get(), TUITypes::InnerElements);
      innerElements.components.push_back(_element->component);
    }
    return TUITypes::ElementObjectVar.Get(_element);
  }
};

struct Render {
  static SHTypesInfo inputTypes() { return TUITypes::Element; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }
  static SHOptionalString help() { return SHCCSTR("Renders a TUI element into a string"); }

  ftxui::Screen _screen = ftxui::Screen::Create(ftxui::Dimension::Full(), ftxui::Dimension::Full());

  std::string _resetPosition;
  std::string _output;

  void cleanup(SHContext *context) {
    _resetPosition = "";
    _output = "";
  }

  void warmup(SHContext *context) { shards::logging::setStdErrLogLevel(spdlog::level::off); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto &element = varAsObjectChecked<TUIElement>(input, TUITypes::Element);
    _output.assign(_resetPosition);
    ftxui::Render(_screen, element.component->Render());
    _output = _screen.ToString();
    _resetPosition = _screen.ResetPosition();
    return Var(_output);
  }
};

struct Tick {
  static SHTypesInfo inputTypes() { return TUITypes::Element; }
  static SHTypesInfo outputTypes() { return TUITypes::Element; }
  static SHOptionalString help() { return SHCCSTR("Handles interactive rendering and event loop management for a TUI element."); }

  ftxui::ScreenInteractive _screen = ftxui::ScreenInteractive::TerminalOutput();
  std::unique_ptr<ftxui::Loop> _loop;

  ftxui::Component _rootComponent = ftxui::Container::Vertical({});

  void warmup(SHContext *context) {
    _loop = std::make_unique<ftxui::Loop>(&_screen, _rootComponent);

    // Disable terminal output
    shards::logging::setStdErrLogLevel(spdlog::level::off);
  }

  void cleanup(SHContext *context) { _loop.reset(); }

  void activate(SHContext *context, const SHVar &input) {
    if (_loop->HasQuitted()) {
      SHLOG_DEBUG("Application quit");
      context->stopFlow(Var::Empty);
      return;
    }
    auto &element = varAsObjectChecked<TUIElement>(input, TUITypes::Element);
    _rootComponent->DetachAllChildren();
    _rootComponent->Add(element.component);
    _loop->RunOnce();
  }
};
} // namespace tui
SHARDS_REGISTER_FN(tui) {
  using namespace tui;
  REGISTER_SHARD("TUI.HBox", HBox);
  REGISTER_SHARD("TUI.VBox", VBox);
  REGISTER_SHARD("TUI.Text", TUIText);
  REGISTER_SHARD("TUI.Render", Render);
  REGISTER_SHARD("TUI.Separator", Separator);
  REGISTER_SHARD("TUI.RunOnce", Tick);
  REGISTER_SHARD("TUI.Button", Button);
  REGISTER_SHARD("TUI.Input", TUIInput);
}
} // namespace shards