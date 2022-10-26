#ifndef D3596C46_E45A_44BD_BDE7_3985CFEC40A3
#define D3596C46_E45A_44BD_BDE7_3985CFEC40A3

#include "types.hpp"
#include "window_mapping.hpp"
#include "../gfx/checked_stack.hpp"
#include <optional>

namespace shards::input {
struct InputStack {
  /// <div rustbindgen hide></div>
  struct Item {
    // Set the input mapping, if unsed, not input will be handled inside this view
    std::optional<WindowMapping> windowMapping;
  };

private:
  std::vector<Item> items;

public:
  InputStack();

  /// <div rustbindgen hide></div>
  void push(Item &&item);

  /// <div rustbindgen hide></div>
  void pop();

  /// <div rustbindgen hide></div>
  const Item &getTop() const;

  void reset() { gfx::ensureEmptyStack(items); }
};
} // namespace shards::input

#endif /* D3596C46_E45A_44BD_BDE7_3985CFEC40A3 */
