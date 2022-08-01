#include "input_stack.hpp"
#include <spdlog/fmt/fmt.h>
#include <stdexcept>

namespace shards::input {
InputStack::InputStack() { items.reserve(16); }

void InputStack::push(Item &&item) { items.emplace_back(std::move(item)); }

void InputStack::pop() {
  if (items.size() == 0)
    throw std::runtime_error("Can't pop from empty input stack");

  items.pop_back();
}

const InputStack::Item& InputStack::getTop() const {
  if (items.size() == 0)
    throw std::runtime_error("Input stack empty");

  return items.back();
}
} // namespace shards::input
