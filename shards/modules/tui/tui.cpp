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

struct TUITypes {
  SHVAR_OBJECT_DECL('tuiE', "TUI.Element", Element, TUIElement);
};

struct HBox {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return TUITypes::Element; }
  static SHOptionalString help() { return SHCCSTR("Creates a horizontal box"); }

  TUIElement *_element = nullptr;

  void warmup(SHContext *shContext) { _element = TUITypes::ElementObjectVar.New(); }

  void cleanup(SHContext *shContext) {
    if (_element) {
      TUITypes::ElementObjectVar.Release(_element);
      _element = nullptr;
    }
  }

  ftxui::Elements _innerElements;
  ShardsVar _action;

  SHVar activate(SHContext *shContext, const SHVar &input) {
    _innerElements.clear();
    // push _innerElements to context
    _element->element = ftxui::hbox(_innerElements);
    return TUITypes::ElementObjectVar.Get(_element);
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
SHARDS_REGISTER_FN(tui) { using namespace tui; }
} // namespace shards