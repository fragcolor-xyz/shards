#ifndef B18F03CA_D90A_4E3B_957F_F6A50A4B2089
#define B18F03CA_D90A_4E3B_957F_F6A50A4B2089

#include "types.hpp"
#include "render_target.hpp"
#include "window_mapping.hpp"

namespace gfx {

/// <div rustbindgen opaque></div>
struct ViewStack {
  /// <div rustbindgen hide></div>
  struct Item {
    // Override the viewport
    std::optional<Rect> viewport;
    // Override render target reference size
    std::optional<int2> referenceSize;
    // Set the input mapping, if unsed, not input will be handled inside this view
    std::optional<WindowMapping> windowMapping;
    // Sets the render target to render to inside this view, will render to main output if unset
    RenderTargetPtr renderTarget;
  };

  /// <div rustbindgen hide></div>
  struct Output {
    Rect viewport;
    std::optional<WindowMapping> windowMapping;
    RenderTargetPtr renderTarget;
    int2 referenceSize;
  };

  typedef size_t Marker;

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

  /// Marker used for checking view stack balance
  /// <div rustbindgen hide></div>
  Marker getMarker() const;

  /// Validates check balance compared to marker start
  /// throws on unmatched push/pop
  /// <div rustbindgen hide></div>
  void restoreMarkerChecked(Marker barrier);

  // Call at the end of the render loop, checks and throws if stack is not empty
  /// <div rustbindgen hide></div>
  void reset();
};
} // namespace gfx

#endif /* B18F03CA_D90A_4E3B_957F_F6A50A4B2089 */
