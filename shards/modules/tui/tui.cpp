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
  ftxui::Element element;
  ftxui::Components components;

  ~TUIElement() { SHLOG_TRACE("TUIElement destroyed"); }
};

struct TUIInnerElements {
  ftxui::Elements elements;
  ftxui::Components components;

  void reset() {
    elements = {};
    components = {};
  }

  void clear() {
    elements.clear();
    components.clear();
  }
};

struct TUITypes {
  SHVAR_OBJECT_DECL('tuiE', "TUI.Element", Element, TUIElement);
  SHVAR_OBJECT_DECL('tuie', "TUI.InnerElements", InnerElements, TUIInnerElements);
};

#define DEFINE_BOX_SHARD(ClassName, Direction, HelpText, FtxuiFunc)                                                \
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
                                                                                                                   \
    SHVar activate(SHContext *context, const SHVar &input) {                                                       \
      _innerElements.clear();                                                                                      \
      SHVar currentInnerElements = _innerElementsVar.get();                                                        \
      assignVariableValue(_innerElementsVar.get(), Var::Object(&_innerElements, TUITypes::InnerElements));         \
      DEFER(assignVariableValue(_innerElementsVar.get(), currentInnerElements));                                   \
      SHVar output{};                                                                                              \
      _contents.activate(context, input, output);                                                                  \
      SHLOG_TRACE("Inner elements: {}", _innerElements.elements.size());                                           \
      _element->element = ftxui::FtxuiFunc(_innerElements.elements);                                               \
      if (currentInnerElements.valueType == SHType::Object) {                                                      \
        auto &innerElements = varAsObjectChecked<TUIInnerElements>(currentInnerElements, TUITypes::InnerElements); \
        innerElements.elements.push_back(_element->element);                                                       \
      }                                                                                                            \
      _element->components = _innerElements.components;                                                            \
      return TUITypes::ElementObjectVar.Get(_element);                                                             \
    }                                                                                                              \
  };

DEFINE_BOX_SHARD(VBox, "vertical", "Creates a vertical box", vbox)
DEFINE_BOX_SHARD(HBox, "horizontal", "Creates a horizontal box", hbox)

struct TUIText {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return TUITypes::Element; }
  static SHOptionalString help() { return SHCCSTR("Adds a text element to the TUI context"); }

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

  std::string _text;

  SHVar activate(SHContext *context, const SHVar &input) {
    auto text = SHSTRVIEW(input);
    _text.assign(text.data(), text.data() + text.size());

    _element->element = ftxui::text(_text);

    if (_innerElementsVar.get().valueType == SHType::Object) {
      auto &innerElements = varAsObjectChecked<TUIInnerElements>(_innerElementsVar.get(), TUITypes::InnerElements);
      innerElements.elements.push_back(_element->element);
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
    _element->element = ftxui::separator();
    if (_innerElementsVar.get().valueType == SHType::Object) {
      auto &innerElements = varAsObjectChecked<TUIInnerElements>(_innerElementsVar.get(), TUITypes::InnerElements);
      innerElements.elements.push_back(_element->element);
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
    _element->element = (*_button)->Render();
    if (_innerElementsVar.get().valueType == SHType::Object) {
      auto &innerElements = varAsObjectChecked<TUIInnerElements>(_innerElementsVar.get(), TUITypes::InnerElements);
      innerElements.elements.push_back(_element->element);
      innerElements.components.push_back(*_button);
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

  void warmup(SHContext *context) {
    shards::logging::setStdErrLogLevel(spdlog::level::off);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto &element = varAsObjectChecked<TUIElement>(input, TUITypes::Element);
    _output.assign(_resetPosition);
    ftxui::Render(_screen, element.element);
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
  std::optional<TUIElement *> _element;

  ftxui::Component _rootComponent = ftxui::Container::Vertical({});

  ftxui::Element getElement() { return _element.value()->element; }

  void warmup(SHContext *context) {
    auto component = ftxui::Renderer(_rootComponent, [&]() { return getElement(); });
    _loop = std::make_unique<ftxui::Loop>(&_screen, std::move(component));

    // Disable terminal output
    shards::logging::setStdErrLogLevel(spdlog::level::off);
  }

  void cleanup(SHContext *context) {
    _loop.reset();
    _element.reset();
  }

  void activate(SHContext *context, const SHVar &input) {
    if (_loop->HasQuitted()) {
      SHLOG_DEBUG("Application quit");
      context->stopFlow(Var::Empty);
      return;
    }
    auto &element = varAsObjectChecked<TUIElement>(input, TUITypes::Element);
    _element = &element;
    _rootComponent->DetachAllChildren();
    for (auto &component : element.components) {
      _rootComponent->Add(component);
    }
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
  REGISTER_SHARD("TUI.Tick", Tick);
  REGISTER_SHARD("TUI.Button", Button);
}
} // namespace shards