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

struct HBox {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return TUITypes::Element; }
  static SHOptionalString help() { return SHCCSTR("Creates a horizontal box"); }

  PARAM(ShardsVar, _contents, "Contents", "The contents of the horizontal box.", {CoreInfo::ShardsOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_contents));

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return TUITypes::Element;
  }

  TUIElement *_element = nullptr;

  void warmup(SHContext *shContext) {
    _innerElementsVar.warmup(shContext);
    assignVariableValue(_innerElementsVar.get(), Var::Object(&_innerElements, TUITypes::InnerElements));

    _element = TUITypes::ElementObjectVar.New();

    _contents.warmup(shContext);
  }

  void cleanup(SHContext *shContext) {
    _contents.cleanup(shContext);

    _innerElements.reset();

    if (_element) {
      TUITypes::ElementObjectVar.Release(_element);
      _element = nullptr;
    }

    _innerElementsVar.cleanup(shContext);
  }

  ParamVar _innerElementsVar{Var::ContextVar("_TUI.InnerElements")};
  TUIInnerElements _innerElements;
  ShardsVar _action;

  SHVar activate(SHContext *shContext, const SHVar &input) {
    _innerElements.clear();
    SHVar output{};
    _contents.activate(shContext, input, output);
    _element->element = ftxui::hbox(_innerElements);
    return TUITypes::ElementObjectVar.Get(_element);
  }
};

struct VBox {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return TUITypes::Element; }
  static SHOptionalString help() { return SHCCSTR("Creates a vertical box"); }

  PARAM(ShardsVar, _contents, "Contents", "The contents of the horizontal box.", {CoreInfo::ShardsOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_contents));

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    _contents.compose(data);
    return TUITypes::Element;
  }

  TUIElement *_element = nullptr;

  void warmup(SHContext *shContext) {
    _innerElementsVar.warmup(shContext);
    assignVariableValue(_innerElementsVar.get(), Var::Object(&_innerElements, TUITypes::InnerElements));

    _element = TUITypes::ElementObjectVar.New();

    _contents.warmup(shContext);
  }

  void cleanup(SHContext *shContext) {
    _contents.cleanup(shContext);

    _innerElements.reset();

    if (_element) {
      TUITypes::ElementObjectVar.Release(_element);
      _element = nullptr;
    }

    _innerElementsVar.cleanup(shContext);
  }

  ParamVar _innerElementsVar{Var::ContextVar("_TUI.InnerElements")};
  TUIInnerElements _innerElements;
  ShardsVar _action;

  SHVar activate(SHContext *shContext, const SHVar &input) {
    _innerElements.clear();
    SHVar output{};
    _contents.activate(shContext, input, output);
    _element->element = ftxui::vbox(_innerElements);
    return TUITypes::ElementObjectVar.Get(_element);
  }
};

struct TUIText {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }
  static SHOptionalString help() { return SHCCSTR("Adds a text element to the TUI context"); }

  ParamVar _innerElementsVar{Var::ContextVar("_TUI.InnerElements")};

  void warmup(SHContext *shContext) { _innerElementsVar.warmup(shContext); }

  void cleanup(SHContext *shContext) { _innerElementsVar.cleanup(shContext); }

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
  OwnedVar _output;

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &element = varAsObjectChecked<TUIElement>(input, TUITypes::Element);
    ftxui::Render(_screen, element.element);
    _output = shards::Var(_screen.ToString());
    return _output;
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