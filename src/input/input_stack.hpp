#ifndef D3596C46_E45A_44BD_BDE7_3985CFEC40A3
#define D3596C46_E45A_44BD_BDE7_3985CFEC40A3

#include "types.hpp"
#include "window_mapping.hpp"
#include "../gfx/checked_stack.hpp"
#include <optional>

namespace shards::input {
using gfx::CheckedStack;
struct InputStack {
  /// <div rustbindgen hide></div>
  struct Item {
    // Set the input mapping, if unsed, not input will be handled inside this view
    std::optional<WindowMapping> windowMapping;
  };

  typedef CheckedStack<Item>::Marker Marker;

private:
  CheckedStack<Item> items;

public:
  InputStack();

  /// <div rustbindgen hide></div>
  void push(Item &&item);

  /// <div rustbindgen hide></div>
  void pop();

  /// <div rustbindgen hide></div>
  const Item &getTop() const;

  // see CheckedStack
  Marker getMarker() const { return items.getMarker(); }

  // see CheckedStack
  void restoreMarkerChecked(Marker marker) { items.restoreMarkerChecked(marker); }

  // see CheckedStack
  void reset() { items.reset(); }
};
} // namespace shards::input

#endif /* D3596C46_E45A_44BD_BDE7_3985CFEC40A3 */
