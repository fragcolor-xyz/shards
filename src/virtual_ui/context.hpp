#ifndef AA53E1AF_30CD_452D_8285_B9C96CC68AD6
#define AA53E1AF_30CD_452D_8285_B9C96CC68AD6

#include "panel.hpp"
#include <input/input.hpp>
#include <gfx/view.hpp>
#include <gfx/view_raycast.hpp>
#include <gfx/egui/input.hpp>
#include <gfx/egui/renderer.hpp>
#include <gfx/view_raycast.hpp>
#include <vector>
#include <memory>
#include <map>

namespace gfx {
struct ShapeRenderer;
}

namespace shards::vui {
typedef std::shared_ptr<Panel> PanelPtr;
struct ContextCachedPanel;
struct Context {
  std::vector<PanelPtr> panels;
  std::map<Panel *, std::shared_ptr<ContextCachedPanel>> cachedPanels;

  gfx::EguiInputTranslator eguiInputTranslator;
  gfx::EguiRenderer eguiRenderer;

  PanelPtr lastFocusedPanel;
  PanelPtr focusedPanel;

  struct PointerInput {
    input::InputBufferIterator iterator;
    gfx::ViewRay ray;

    float hitDistance = FLT_MAX;
    PanelPtr hitPanel;

    gfx::float2 panelCoord;
  };
  std::vector<PointerInput> pointerInputs;
  std::vector<input::InputBufferIterator> otherEvents;

  // Prepare input raycast
  //  inputToViewScale is used to convert coordinates from pointer events to view coordinates
  void prepareInputs(input::InputBuffer &input, gfx::float2 inputToViewScale, const gfx::SizedView &sizedView);

  void renderDebug(gfx::ShapeRenderer &sr);

  // Renders all the UI
  void evaluate(gfx::DrawQueuePtr queue, double time, float deltaTime);

  std::shared_ptr<ContextCachedPanel> getCachedPanel(PanelPtr panel);
};
} // namespace shards::vui

#endif /* AA53E1AF_30CD_452D_8285_B9C96CC68AD6 */
