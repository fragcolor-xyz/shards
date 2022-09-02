#include "gizmos.hpp"
#include "../drawable.hpp"

namespace gfx {
namespace gizmos {
void Context::begin(const InputState &inputState, ViewPtr view) {
  input.begin(inputState, view);
  renderer.begin(view, inputState.viewSize);
}
void Context::updateGizmo(IGizmo &gizmo) {
  gizmo.update(input);
  gizmos.push_back(&gizmo);
}
void Context::end(DrawQueuePtr drawQueue) {
  // Required update order:
  // - Handle update
  // - Context update
  //   - Camera projection
  //   - Grab/release events
  //   - Movement events
  // - Drawing (last to ensure drawing the most up-to-date info after movement events)

  input.end();

  for (auto &gizmo : gizmos) {
    gizmo->render(input, renderer);
  }
  gizmos.clear();

  renderer.end(drawQueue);
}
} // namespace gizmos
} // namespace gfx