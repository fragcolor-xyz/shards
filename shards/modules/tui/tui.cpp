#include <shards/shards.hpp>
#include <shards/utility.hpp>
#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>

#include "ftxui/component/component.hpp"          // for Button, Horizontal, Renderer
#include "ftxui/component/component_base.hpp"     // for ComponentBase
#include "ftxui/component/screen_interactive.hpp" // for ScreenInteractive
#include "ftxui/component/loop.hpp"               // for Loop
#include "ftxui/dom/elements.hpp"                 // for separator, gauge, text, Element, operator|, vbox, border

namespace shards {
namespace tui {

class ScrollerBase : public ftxui::ComponentBase {
public:
  ScrollerBase(ftxui::Component child, int selected, int size) : selected_(selected), size_(size) { Add(child); }

private:
  ftxui::Element OnRender() final {
    auto focused = Focused() ? ftxui::focus : ftxui::select;
    auto style = Focused() ? ftxui::inverted : ftxui::nothing;

    ftxui::Element background = ComponentBase::Render();
    background->ComputeRequirement();
    size_ = background->requirement().min_y;
    return ftxui::dbox({
               std::move(background),
               ftxui::vbox({
                   ftxui::text("") | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, selected_),
                   ftxui::text("") | style | focused,
               }),
           }) |
           ftxui::vscroll_indicator | ftxui::yframe | ftxui::yflex | ftxui::reflect(box_);
  }

  bool OnEvent(ftxui::Event event) final {
    if (event.is_mouse() && box_.Contain(event.mouse().x, event.mouse().y))
      TakeFocus();

    int selected_old = selected_;
    if (event == ftxui::Event::ArrowUp || event == ftxui::Event::Character('k') ||
        (event.is_mouse() && event.mouse().button == ftxui::Mouse::WheelUp)) {
      selected_--;
    }
    if ((event == ftxui::Event::ArrowDown || event == ftxui::Event::Character('j') ||
         (event.is_mouse() && event.mouse().button == ftxui::Mouse::WheelDown))) {
      selected_++;
    }
    if (event == ftxui::Event::PageDown)
      selected_ += box_.y_max - box_.y_min;
    if (event == ftxui::Event::PageUp)
      selected_ -= box_.y_max - box_.y_min;
    if (event == ftxui::Event::Home)
      selected_ = 0;
    if (event == ftxui::Event::End)
      selected_ = size_;

    selected_ = std::max(0, std::min(size_ - 1, selected_));
    return selected_old != selected_;
  }

  bool Focusable() const final { return true; }

  int selected_ = 0;
  int size_ = 0;
  ftxui::Box box_;
};

struct TUIElement {
  ftxui::Component component;

  ~TUIElement() { SHLOG_TRACE("TUIElement destroyed"); }
};

struct TUIInnerElements {
  std::vector<TUIElement *> components;

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
        _container->Add(component->component);                                                                     \
      }                                                                                                            \
      _element->component = _container;                                                                            \
      if (currentInnerElements.valueType == SHType::Object) {                                                      \
        auto &innerElements = varAsObjectChecked<TUIInnerElements>(currentInnerElements, TUITypes::InnerElements); \
        innerElements.components.push_back(_element);                                                              \
      }                                                                                                            \
      return TUITypes::ElementObjectVar.Get(_element);                                                             \
    }                                                                                                              \
  };

DEFINE_BOX_SHARD(VBox, "vertical", "Creates a vertical box", vbox, Vertical)
DEFINE_BOX_SHARD(HBox, "horizontal", "Creates a horizontal box", hbox, Horizontal)

struct TUIText {
  TUIText() {
    _color = Var::ColorFromInt(0xFFFFFFFF);
    _backgroundColor = Var::ColorFromInt(0x00000000);
  }

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return TUITypes::Element; }
  static SHOptionalString help() { return SHCCSTR("Adds a text element to the TUI context"); }

  PARAM_PARAMVAR(_color, "Color", "The color of the text", {CoreInfo::ColorType, CoreInfo::ColorVarType});
  PARAM_PARAMVAR(_backgroundColor, "BackgroundColor", "The background color of the text",
                 {CoreInfo::ColorType, CoreInfo::ColorVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_color), PARAM_IMPL_FOR(_backgroundColor));

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

    auto colorValue = _color.get().payload.colorValue;
    ftxui::Color color = ftxui::Color::RGBA(colorValue.r, colorValue.g, colorValue.b, colorValue.a);
    auto backgroundColorValue = _backgroundColor.get().payload.colorValue;
    ftxui::Color backgroundColor =
        ftxui::Color::RGBA(backgroundColorValue.r, backgroundColorValue.g, backgroundColorValue.b, backgroundColorValue.a);
    _element->component = ftxui::Renderer([this, color, backgroundColor]() {
      auto element = ftxui::text(_text);
      element = element | ftxui::color(color);
      element = element | ftxui::bgcolor(backgroundColor);
      return element;
    });

    if (_innerElementsVar.get().valueType == SHType::Object) {
      auto &innerElements = varAsObjectChecked<TUIInnerElements>(_innerElementsVar.get(), TUITypes::InnerElements);
      innerElements.components.push_back(_element);
    }

    return TUITypes::ElementObjectVar.Get(_element);
  }
};
struct TUIParagraph {
  TUIParagraph() {
    _color = Var::ColorFromInt(0xFFFFFFFF);
    _backgroundColor = Var::ColorFromInt(0x00000000);
  }

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return TUITypes::Element; }
  static SHOptionalString help() { return SHCCSTR("Adds a paragraph element with text wrapping to the TUI context"); }

  PARAM_PARAMVAR(_color, "Color", "The color of the text", {CoreInfo::ColorType, CoreInfo::ColorVarType});
  PARAM_PARAMVAR(_backgroundColor, "BackgroundColor", "The background color of the text",
                 {CoreInfo::ColorType, CoreInfo::ColorVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_color), PARAM_IMPL_FOR(_backgroundColor));

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

    auto colorValue = _color.get().payload.colorValue;
    ftxui::Color color = ftxui::Color::RGBA(colorValue.r, colorValue.g, colorValue.b, colorValue.a);
    auto backgroundColorValue = _backgroundColor.get().payload.colorValue;
    ftxui::Color backgroundColor =
        ftxui::Color::RGBA(backgroundColorValue.r, backgroundColorValue.g, backgroundColorValue.b, backgroundColorValue.a);

    _element->component = ftxui::Renderer([this, color, backgroundColor]() {
      auto element = ftxui::paragraph(_text);
      element = element | ftxui::color(color);
      element = element | ftxui::bgcolor(backgroundColor);
      return element;
    });

    if (_innerElementsVar.get().valueType == SHType::Object) {
      auto &innerElements = varAsObjectChecked<TUIInnerElements>(_innerElementsVar.get(), TUITypes::InnerElements);
      innerElements.components.push_back(_element);
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
      innerElements.components.push_back(_element);
    }
    return TUITypes::ElementObjectVar.Get(_element);
  }
};

struct Filler {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return TUITypes::Element; }
  static SHOptionalString help() { return SHCCSTR("Adds a filler element to the TUI context"); }

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
    _element->component = ftxui::Renderer([]() { return ftxui::filler(); });
    if (_innerElementsVar.get().valueType == SHType::Object) {
      auto &innerElements = varAsObjectChecked<TUIInnerElements>(_innerElementsVar.get(), TUITypes::InnerElements);
      innerElements.components.push_back(_element);
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
      innerElements.components.push_back(_element);
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
    ftxui::InputOption option{};

    option.transform = [](ftxui::InputState state) {
      state.element |= ftxui::borderEmpty;
      state.element |= ftxui::color(ftxui::Color::White);

      if (state.is_placeholder) {
        state.element |= ftxui::dim;
      }

      if (state.focused) {
        state.element |= ftxui::bgcolor(ftxui::Color::Black);
      }

      if (state.hovered) {
        state.element |= ftxui::bgcolor(ftxui::Color::GrayDark);
      }

      return state.element;
    };

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
      innerElements.components.push_back(_element);
    }

    return TUITypes::ElementObjectVar.Get(_element);
  }
};

struct TUISplit {
  TUISplit() { _size = Var(20, 20, 20, 20); }

  static SHTypesInfo inputTypes() { return TUITypes::Element; }
  static SHTypesInfo outputTypes() { return TUITypes::Element; }
  static SHOptionalString help() { return SHCCSTR("Surrounds the input element with multiple panes"); }

  PARAM_PARAMVAR(_top, "Top", "The contents of the top part",
                 {CoreInfo::NoneType, TUITypes::Element, Type::VariableOf(TUITypes::Element)});
  PARAM_PARAMVAR(_left, "Left", "The contents of the left part",
                 {CoreInfo::NoneType, TUITypes::Element, Type::VariableOf(TUITypes::Element)});
  PARAM_PARAMVAR(_right, "Right", "The contents of the right part",
                 {CoreInfo::NoneType, TUITypes::Element, Type::VariableOf(TUITypes::Element)});
  PARAM_PARAMVAR(_bottom, "Bottom", "The contents of the bottom part",
                 {CoreInfo::NoneType, TUITypes::Element, Type::VariableOf(TUITypes::Element)});
  PARAM_VAR(_size, "InitialSize", "The initial size of the panes (left, right, top, bottom as Int4)", {CoreInfo::Int4Type});
  PARAM_IMPL(PARAM_IMPL_FOR(_top), PARAM_IMPL_FOR(_left), PARAM_IMPL_FOR(_right), PARAM_IMPL_FOR(_bottom), PARAM_IMPL_FOR(_size));

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return TUITypes::Element;
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    left_size = _size.payload.int4Value[0];
    right_size = _size.payload.int4Value[1];
    top_size = _size.payload.int4Value[2];
    bottom_size = _size.payload.int4Value[3];

    _element = TUITypes::ElementObjectVar.New();
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    if (_element) {
      TUITypes::ElementObjectVar.Release(_element);
      _element = nullptr;
    }
  }

  ftxui::Component _splitComponent;

  TUIElement *_element = nullptr;

  int left_size = 0;
  int right_size = 0;
  int top_size = 0;
  int bottom_size = 0;

  SHVar activate(SHContext *context, const SHVar &input) {
    auto &middleElem = varAsObjectChecked<TUIElement>(input, TUITypes::Element);
    _splitComponent = middleElem.component;

    if (!_left.isNone()) {
      auto &leftElem = varAsObjectChecked<TUIElement>(_left.get(), TUITypes::Element);
      _splitComponent = ftxui::ResizableSplitLeft(leftElem.component, _splitComponent, &left_size);
    }

    if (!_right.isNone()) {
      auto &rightElem = varAsObjectChecked<TUIElement>(_right.get(), TUITypes::Element);
      _splitComponent = ftxui::ResizableSplitRight(rightElem.component, _splitComponent, &right_size);
    }

    if (!_top.isNone()) {
      auto &topElem = varAsObjectChecked<TUIElement>(_top.get(), TUITypes::Element);
      _splitComponent = ftxui::ResizableSplitTop(topElem.component, _splitComponent, &top_size);
    }

    if (!_bottom.isNone()) {
      auto &bottomElem = varAsObjectChecked<TUIElement>(_bottom.get(), TUITypes::Element);
      _splitComponent = ftxui::ResizableSplitBottom(bottomElem.component, _splitComponent, &bottom_size);
    }

    _element->component = _splitComponent;
    return TUITypes::ElementObjectVar.Get(_element);
  }
};

struct TUIModifierBase {
  static SHTypesInfo inputTypes() { return TUITypes::Element; }
  static SHTypesInfo outputTypes() { return TUITypes::Element; }
};

struct TUIFlex : TUIModifierBase {
  static SHOptionalString help() { return SHCCSTR("Wraps the input element in a flex container"); }

  void activate(SHContext *context, const SHVar &input) {
    auto &elem = varAsObjectChecked<TUIElement>(input, TUITypes::Element);
    elem.component = elem.component | ftxui::flex;
  }
};

struct TUICentered : TUIModifierBase {
  static SHOptionalString help() { return SHCCSTR("Centers the input element"); }

  void activate(SHContext *context, const SHVar &input) {
    auto &elem = varAsObjectChecked<TUIElement>(input, TUITypes::Element);
    elem.component = elem.component | ftxui::center;
  }
};

struct TUIAlignedRight : TUIModifierBase {
  static SHOptionalString help() { return SHCCSTR("Aligns the input element to the right"); }

  void activate(SHContext *context, const SHVar &input) {
    auto &elem = varAsObjectChecked<TUIElement>(input, TUITypes::Element);
    elem.component = elem.component | ftxui::align_right;
  }
};

struct TUIBorder : TUIModifierBase {
  static SHOptionalString help() { return SHCCSTR("Wraps the input element in a border"); }

  void activate(SHContext *context, const SHVar &input) {
    auto &elem = varAsObjectChecked<TUIElement>(input, TUITypes::Element);
    elem.component = elem.component | ftxui::border;
  }
};

struct TUIFrame : TUIModifierBase {
  static SHOptionalString help() { return SHCCSTR("Wraps the input element in a frame"); }

  void activate(SHContext *context, const SHVar &input) {
    auto &elem = varAsObjectChecked<TUIElement>(input, TUITypes::Element);
    elem.component = elem.component | ftxui::frame;
  }
};

struct TUIScrollable : TUIModifierBase {
  static SHOptionalString help() { return SHCCSTR("Wraps the input element in a scrollable container"); }

  int _selected = 0;
  int _size = 0;

  void warmup(SHContext *context) {
    _selected = 0;
    _size = 0;
  }

  void activate(SHContext *context, const SHVar &input) {
    auto &elem = varAsObjectChecked<TUIElement>(input, TUITypes::Element);
    elem.component = ftxui::Make<ScrollerBase>(elem.component, _selected, _size);
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

  ftxui::ScreenInteractive _screen = ftxui::ScreenInteractive::Fullscreen();
  std::unique_ptr<ftxui::Loop> _loop;

  ftxui::Component _currentComponent;

  void warmup(SHContext *context) {
    _loop = std::make_unique<ftxui::Loop>(&_screen, _currentComponent);

    // Disable terminal output
    shards::logging::setStdErrLogLevel(spdlog::level::off);
  }

  void cleanup(SHContext *context) { _loop.reset(); }

  void activate(SHContext *context, const SHVar &input) {
    auto &element = varAsObjectChecked<TUIElement>(input, TUITypes::Element);

    if (_currentComponent != element.component) {
      _currentComponent = element.component;
      _loop = std::make_unique<ftxui::Loop>(&_screen, _currentComponent);
      SHLOG_DEBUG("New component: {}", input);
    }

    if (!_currentComponent) {
      throw ActivationError("TUI.RunOnce requires a TUI.Element input");
    }

    if (_loop->HasQuitted()) {
      SHLOG_DEBUG("Application quit");
      context->stopFlow(Var::Empty);
      return;
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
  REGISTER_SHARD("TUI.Paragraph", TUIParagraph);
  REGISTER_SHARD("TUI.Render", Render);
  REGISTER_SHARD("TUI.Separator", Separator);
  REGISTER_SHARD("TUI.RunOnce", Tick);
  REGISTER_SHARD("TUI.Button", Button);
  REGISTER_SHARD("TUI.Input", TUIInput);
  REGISTER_SHARD("TUI.Split", TUISplit);
  REGISTER_SHARD("TUI.Filler", Filler);
  REGISTER_SHARD("TUI.Flex", TUIFlex);
  REGISTER_SHARD("TUI.Centered", TUICentered);
  REGISTER_SHARD("TUI.AlignedRight", TUIAlignedRight);
  REGISTER_SHARD("TUI.Border", TUIBorder);
  REGISTER_SHARD("TUI.Scrollable", TUIScrollable);
  REGISTER_SHARD("TUI.Frame", TUIFrame);
}
} // namespace shards