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

  gfx::SizedView sizedView;

  // Points per world space unit
  float virtualPointScale = 200.0f;

  // Resolution of rendered UI, in actual pixels per virtual UI pixel
  float pixelsPerPoint = 2.0f;

  // Minimum resolution to render UI at from a distance
  const float minResolution = 0.25f;

  // Maxmimum resolution to render UI at up close
  const float maxResolution = 8.0f;

  // Steps that resolution switches at
  const float resolutionGranularity = 0.25f;

  struct PointerInput {
    input::InputBufferIterator iterator;
    gfx::ViewRay ray;

    float hitDistance = FLT_MAX;
    PanelPtr hitPanel;

    gfx::float2 panelCoord;
  };
  std::vector<PointerInput> pointerInputs;
  std::vector<input::InputBufferIterator> otherEvents;
  std::optional<PointerInput> lastPointerInput;

  // Prepare input raycast
  //  inputToViewScale is used to convert coordinates from pointer events to view coordinates
  void prepareInputs(input::InputBuffer &input, gfx::float2 inputToViewScale, const gfx::SizedView &sizedView);

  void renderDebug(gfx::ShapeRenderer &sr);

  // Renders all the UI
  void evaluate(gfx::DrawQueuePtr queue, double time, float deltaTime);

private:
  std::shared_ptr<ContextCachedPanel> getCachedPanel(PanelPtr panel);

  void renderPanel(gfx::DrawQueuePtr queue, PanelPtr panel, egui::Input baseInput);

  float computeRenderResolution(PanelPtr panel) const;
};
} // namespace shards::vui

#endif /* AA53E1AF_30CD_452D_8285_B9C96CC68AD6 */
