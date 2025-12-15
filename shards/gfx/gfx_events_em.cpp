#include "gfx_events_em.hpp"
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

namespace gfx::em {

EventHandler *getEventHandler() {
  static EventHandler eh;
  return &eh;
}

void eventHandlerSetCanvas(EventHandler *eh, const std::string &tag, const std::string &containerTag) {
  eh->canvasContainerTag = containerTag;
  eh->canvasTag = tag;
}
void eventHandlerTryFlush(EventHandler *eh) { eh->tryFlush(); }
void eventHandlerPostKeyEvent(EventHandler *eh, const KeyEvent &ke) { eh->clientPost(ke); }
void eventHandlerPostMouseEvent(EventHandler *eh, const MouseEvent &pe) { eh->clientPost(pe); }
void eventHandlerPostWheelEvent(EventHandler *eh, const MouseWheelEvent &we) { eh->clientPost(we); }
void eventHandlerPostDisplayFormat(EventHandler *eh, int32_t width, int32_t height, int32_t cwidth, int32_t cheight,
                                   float pixelRatio) {
  eh->astate.displayWidth = width;
  eh->astate.displayHeight = height;
  eh->astate.canvasWidth = cwidth;
  eh->astate.canvasHeight = cheight;
  eh->astate.pixelRatio = pixelRatio;
  SPDLOG_INFO("Display format: {}x{}, canvas: {}x{}, pixelRatio: {}", width, height, cwidth, cheight, pixelRatio);
}

} // namespace gfx::em

// C API for JavaScript - replaces embind
extern "C" {

EMSCRIPTEN_KEEPALIVE
gfx::em::EventHandler *gfxGetEventHandler() { return gfx::em::getEventHandler(); }

EMSCRIPTEN_KEEPALIVE
void gfxEventHandlerSetCanvas(gfx::em::EventHandler *eh, const char *canvasId, const char *containerId) {
  gfx::em::eventHandlerSetCanvas(eh, canvasId, containerId);
}

EMSCRIPTEN_KEEPALIVE
void gfxEventHandlerTryFlush(gfx::em::EventHandler *eh) { gfx::em::eventHandlerTryFlush(eh); }

EMSCRIPTEN_KEEPALIVE
void gfxEventHandlerPostKeyEvent(gfx::em::EventHandler *eh, int32_t type, int32_t domKey, uint32_t key,
                                  bool ctrlKey, bool altKey, bool shiftKey, bool repeat) {
  gfx::em::KeyEvent ke{};
  ke.type_ = type;
  ke.domKey_ = domKey;
  ke.key_ = key;
  ke.ctrlKey = ctrlKey;
  ke.altKey = altKey;
  ke.shiftKey = shiftKey;
  ke.repeat = repeat;
  gfx::em::eventHandlerPostKeyEvent(eh, ke);
}

EMSCRIPTEN_KEEPALIVE
void gfxEventHandlerPostMouseEvent(gfx::em::EventHandler *eh, int32_t type, int32_t x, int32_t y,
                                    int32_t button, int32_t movementX, int32_t movementY) {
  gfx::em::MouseEvent me{};
  me.type_ = type;
  me.x = x;
  me.y = y;
  me.button = button;
  me.movementX = movementX;
  me.movementY = movementY;
  gfx::em::eventHandlerPostMouseEvent(eh, me);
}

EMSCRIPTEN_KEEPALIVE
void gfxEventHandlerPostWheelEvent(gfx::em::EventHandler *eh, float deltaY) {
  gfx::em::MouseWheelEvent we{};
  we.deltaY = deltaY;
  gfx::em::eventHandlerPostWheelEvent(eh, we);
}

EMSCRIPTEN_KEEPALIVE
void gfxEventHandlerPostDisplayFormat(gfx::em::EventHandler *eh, int32_t width, int32_t height,
                                       int32_t cwidth, int32_t cheight, float pixelRatio) {
  gfx::em::eventHandlerPostDisplayFormat(eh, width, height, cwidth, cheight, pixelRatio);
}

}