#include "view_stack.hpp"
#include <spdlog/fmt/fmt.h>

namespace gfx {

ViewStack::ViewStack() { items.reserve(16); }

void ViewStack::push(Item &&item) { items.emplace_back(std::move(item)); }

void ViewStack::pop() {
  if (items.size() == 0)
    throw std::runtime_error("Can't pop from empty view stack");

  items.pop_back();
}

ViewStack::Output ViewStack::getOutput() const {
  Output result{};

  bool isViewportSet = false;
  bool hasReferenceSize = false;
  for (size_t i = 0; i < items.size(); i++) {
    const Item &item = items[i];
    if (item.renderTarget) {
      result.renderTarget = item.renderTarget;
      isViewportSet = false;
    }
    if (item.viewport) {
      result.viewport = item.viewport.value();
      isViewportSet = true;
    }
    if (item.referenceSize) {
      result.referenceSize = item.referenceSize.value();
      hasReferenceSize = true;
    }
    result.windowMapping = item.windowMapping;
  }

  if (result.renderTarget && !hasReferenceSize) {
    throw std::runtime_error(fmt::format("Failed to find render target reference size in view stack"));
  }

  // Derive viewport from output
  if (!isViewportSet) {
    if (!result.renderTarget)
      throw std::runtime_error("Can not derive viewport, no render target in view stack");

    auto it = result.renderTarget->attachments.begin();
    if (it == result.renderTarget->attachments.end())
      throw std::runtime_error("Can not derive viewport from render target");

    result.viewport = Rect(result.renderTarget->computeSize(result.referenceSize));
  }

  return result;
}

// Marker used for checking view stack balance
ViewStack::Marker ViewStack::getMarker() const { return items.size(); }

// Validates check balance compared to marker start
// throws on unmatched push/pop
void ViewStack::restoreMarkerChecked(Marker barrier) {
  size_t numItemsBeforeFixup = items.size();

  // Fixup before throw in case error is handled
  items.resize(barrier);

  if (numItemsBeforeFixup != barrier) {
    throw std::runtime_error(fmt::format("View stack imbalance ({} elements, expected {})", numItemsBeforeFixup, barrier));
  }
}

void ViewStack::reset() { restoreMarkerChecked(Marker(0)); }
} // namespace gfx
