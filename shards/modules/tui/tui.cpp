#include "ftxui/component/captured_mouse.hpp"     // for ftxui
#include "ftxui/component/component.hpp"          // for Button, Horizontal, Renderer
#include "ftxui/component/component_base.hpp"     // for ComponentBase
#include "ftxui/component/screen_interactive.hpp" // for ScreenInteractive
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

  ~TUIElement() { SHLOG_TRACE("TUIElement destroyed"); }
};

struct TUIInnerElements {
  ftxui::Elements elements;

  void reset() { elements = {}; }
  void clear() { elements.clear(); }
};

struct TUITypes {
  SHVAR_OBJECT_DECL('tuiE', "TUI.Element", Element, TUIElement);
  SHVAR_OBJECT_DECL('tuie', "TUI.InnerElements", InnerElements, TUIInnerElements);
};

#define DEFINE_BOX_SHARD(ClassName, Direction, HelpText, FtxuiFunc)                                              \
  struct ClassName {                                                                                             \
    static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }                                                \
    static SHTypesInfo outputTypes() { return TUITypes::Element; }                                               \
    static SHOptionalString help() { return SHCCSTR(HelpText); }                                                 \
                                                                                                                 \
    PARAM(ShardsVar, _contents, "Contents", "The contents of the " Direction " box.", {CoreInfo::ShardsOrNone}); \
    PARAM_IMPL(PARAM_IMPL_FOR(_contents));                                                                       \
                                                                                                                 \
    PARAM_REQUIRED_VARIABLES();                                                                                  \
    SHTypeInfo compose(SHInstanceData &data) {                                                                   \
      PARAM_COMPOSE_REQUIRED_VARIABLES(data);                                                                    \
      _contents.compose(data);                                                                                   \
      return TUITypes::Element;                                                                                  \
    }                                                                                                            \
                                                                                                                 \
    TUIElement *_element = nullptr;                                                                              \
                                                                                                                 \
    void warmup(SHContext *context) {                                                                            \
      _innerElementsVar.warmup(context);                                                                         \
      _element = TUITypes::ElementObjectVar.New();                                                               \
      _contents.warmup(context);                                                                                 \
    }                                                                                                            \
                                                                                                                 \
    void cleanup(SHContext *context) {                                                                           \
      _contents.cleanup(context);                                                                                \
      _innerElements.reset();                                                                                    \
      if (_element) {                                                                                            \
        TUITypes::ElementObjectVar.Release(_element);                                                            \
        _element = nullptr;                                                                                      \
      }                                                                                                          \
      _innerElementsVar.cleanup(context);                                                                        \
    }                                                                                                            \
                                                                                                                 \
    ParamVar _innerElementsVar{Var::ContextVar("_TUI.InnerElements")};                                           \
    TUIInnerElements _innerElements;                                                                             \
    ShardsVar _action;                                                                                           \
                                                                                                                 \
    SHVar activate(SHContext *context, const SHVar &input) {                                                     \
      _innerElements.clear();                                                                                    \
      SHVar currentInnerElements = _innerElementsVar.get();                                                      \
      assignVariableValue(_innerElementsVar.get(), Var::Object(&_innerElements, TUITypes::InnerElements));       \
      DEFER(assignVariableValue(_innerElementsVar.get(), currentInnerElements));                                 \
      SHVar output{};                                                                                            \
      _contents.activate(context, input, output);                                                                \
      SHLOG_TRACE("Inner elements: {}", _innerElements.elements.size());                                         \
      _element->element = ftxui::FtxuiFunc(_innerElements);                                                      \
      return TUITypes::ElementObjectVar.Get(_element);                                                           \
    }                                                                                                            \
  };

DEFINE_BOX_SHARD(VBox, "vertical", "Creates a vertical box", vbox)
DEFINE_BOX_SHARD(HBox, "horizontal", "Creates a horizontal box", hbox)

struct TUIText {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }
  static SHOptionalString help() { return SHCCSTR("Adds a text element to the TUI context"); }

  ParamVar _innerElementsVar{Var::ContextVar("_TUI.InnerElements")};

  void warmup(SHContext *context) { _innerElementsVar.warmup(context); }

  void cleanup(SHContext *context) { _innerElementsVar.cleanup(context); }

  std::string _text;

  void activate(SHContext *context, const SHVar &input) {
    auto text = SHSTRVIEW(input);
    _text.assign(text.data(), text.data() + text.size());
    auto &innerElements = varAsObjectChecked<TUIInnerElements>(_innerElementsVar.get(), TUITypes::InnerElements);
    innerElements.elements.push_back(ftxui::text(_text));
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

  SHVar activate(SHContext *context, const SHVar &input) {
    auto &element = varAsObjectChecked<TUIElement>(input, TUITypes::Element);
    _output.assign(_resetPosition);
    ftxui::Render(_screen, element.element);
    _output = _screen.ToString();
    _resetPosition = _screen.ResetPosition();
    return Var(_output);
  }
};
} // namespace tui
SHARDS_REGISTER_FN(tui) {
  using namespace tui;
  REGISTER_SHARD("TUI.HBox", HBox);
  REGISTER_SHARD("TUI.VBox", VBox);
  REGISTER_SHARD("TUI.Text", TUIText);
  REGISTER_SHARD("TUI.Render", Render);
}
} // namespace shards