#ifndef BC4DC7AC_C239_479E_A100_F79DA6EC68B6
#define BC4DC7AC_C239_479E_A100_F79DA6EC68B6

#include "repr.hpp"
#include <shards/core/ops_internal.hpp>
#include <string>

namespace ui2::repr {
template <typename T> inline void debugDump(T &output, Node::Ptr node) {
  struct Item {
    Node::Ptr node;
    size_t index;
  };
  std::vector<Item> stack;
  stack.push_back(Item{node, 0});

  while (!stack.empty()) {
    auto &item = stack.back();
    auto node = item.node;
    size_t idx = item.index++;

    if (idx == 0) {
      output << std::string(stack.size() - 1, ' ') << "|-";
      output << fmt::format("[{}] {} - {}", node->version,
#if SHARDS_UI_LABELS
                            node->label,
#else
                            "no label",
#endif
                            (void *)node.get());
      output << std::endl;
    }

    if (idx < item.node->children.size()) {
      stack.push_back(Item{item.node->children[idx], 0});
    } else {
      stack.pop_back();
    }
  }
}
} // namespace ui2::repr

#endif /* BC4DC7AC_C239_479E_A100_F79DA6EC68B6 */
