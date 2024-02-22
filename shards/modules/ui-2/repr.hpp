#ifndef E6AB9BA6_3455_4C20_A509_A3C633D585AF
#define E6AB9BA6_3455_4C20_A509_A3C633D585AF

#include <shards/core/assert.hpp>
#include <memory>
#include <vector>
#include <string>
#include <shards/fast_string/fast_string.hpp>

#ifdef NDEBUG
#define SHARDS_UI_LABELS 0
#else
#define SHARDS_UI_LABELS 1
#endif

namespace ui2::repr {
using FastString = shards::fast_string::FastString;

struct NodeInsertion {};

struct Root;

struct Node : public std::enable_shared_from_this<Node> {
  using Ptr = std::shared_ptr<Node>;
  static inline Ptr create() { return std::make_shared<Node>(); }

  size_t version{};
  std::vector<Node::Ptr> children;

#if SHARDS_UI_LABELS
  FastString label;
#endif

  void enter(Root &);
  void leave(Root &);
  void updateChildren(Root &, std::vector<Node::Ptr> &newChildren);
};

struct NodeStack {
  struct Item {
    Node::Ptr target;
    std::vector<Node::Ptr> tmpChildren;
  };
  std::vector<Item> stack;
  int64_t stackPtr = -1;

  Item &top(int64_t offset = 0) {
    shassert((stackPtr + offset) >= 0);
    shassert((stackPtr + offset) <= stackPtr);
    return stack[stackPtr + offset];
  }

  Item &push(Node::Ptr node) {
    ++stackPtr;
    if (stackPtr > int64_t(stack.size()) - 1) {
      stack.resize(stackPtr + 1);
    }
    top().target = node;
    return top();
  }

  void pop() {
    shassert(stackPtr >= 0);
    --stackPtr;
  }

  bool empty() const { return stackPtr < 0; }

  size_t size() const { return stackPtr + 1; }

  void clear() { stackPtr = -1; }
};

struct Root {
  Node::Ptr container = Node::create();
  NodeStack stack;
  size_t currentVersion{};

  void enter() {
    ++currentVersion;
    shassert(stack.empty());
    container->enter(*this);
  }

  void leave() {
    shassert(stack.size() == 1);
    container->leave(*this);
  }
};

inline void Node::enter(Root &r) {
  auto selfPtr = shared_from_this();

  // Insert into parent node
  if (!r.stack.empty()) {
    r.stack.top().tmpChildren.push_back(selfPtr);
  }

  // Push node and clear children list
  r.stack.push(selfPtr);
}

inline void Node::leave(Root &r) {
  auto &stackItem = r.stack.top();
  updateChildren(r, stackItem.tmpChildren);
  r.stack.pop();
}

inline void Node::updateChildren(Root &r, std::vector<Node::Ptr> &newChildren) {
  std::swap(children, newChildren);
  newChildren.clear();
}

} // namespace ui2::repr

#endif /* E6AB9BA6_3455_4C20_A509_A3C633D585AF */
