#ifndef B18F03CA_D90A_4E3B_957F_F6A50A4B2089
#define B18F03CA_D90A_4E3B_957F_F6A50A4B2089

#include "types.hpp"
#include "render_target.hpp"
#include "checked_stack.hpp"

namespace gfx {

/// <div rustbindgen opaque></div>
struct ViewStack {
  /// <div rustbindgen hide></div>
  struct Item {
    // Override the viewport
    std::optional<Rect> viewport;
    // Override render target reference size
    std::optional<int2> referenceSize;
    // Sets the render target to render to inside this view, will render to main output if unset
    RenderTargetPtr renderTarget;
  };

  /// <div rustbindgen hide></div>
  struct Output {
    Rect viewport;
    RenderTargetPtr renderTarget;
    int2 referenceSize;
  };

private:
  std::vector<Item> items;

public:
  ViewStack();

  /// <div rustbindgen hide></div>
  void push(Item &&item);

  /// <div rustbindgen hide></div>
  void pop();

  /// <div rustbindgen hide></div>
  Output getOutput() const;

  void reset() { ensureEmptyStack(items); }
};
} // namespace gfx

#endif /* B18F03CA_D90A_4E3B_957F_F6A50A4B2089 */
