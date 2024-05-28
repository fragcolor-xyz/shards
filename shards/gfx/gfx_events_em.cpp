#include "gfx_events_em.hpp"
#include <emscripten/bind.h>

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

EMSCRIPTEN_BINDINGS(gfx_events) {
  emscripten::class_<EventHandler>("EventHandler");
  emscripten::value_object<KeyEvent>("KeyEvent") //
      .field("type_", &KeyEvent::type_)
      .field("key_", &KeyEvent::key_)
      .field("scan_", &KeyEvent::scan_)
      .field("ctrlKey", &KeyEvent::ctrlKey)
      .field("altKey", &KeyEvent::altKey)
      .field("shiftKey", &KeyEvent::shiftKey)
      .field("repeat", &KeyEvent::repeat);
  emscripten::value_object<MouseEvent>("MouseEvent") //
      .field("type_", &MouseEvent::type_)
      .field("x", &MouseEvent::x)
      .field("y", &MouseEvent::y)
      .field("button", &MouseEvent::button)
      .field("movementX", &MouseEvent::movementX)
      .field("movementY", &MouseEvent::movementY);
  emscripten::value_object<MouseWheelEvent>("MouseWheelEvent") //
      .field("deltaY", &MouseWheelEvent::deltaY);

  emscripten::function("getEventHandler", &getEventHandler, emscripten::allow_raw_pointers());
  emscripten::function("eventHandlerSetCanvas", &eventHandlerSetCanvas, emscripten::allow_raw_pointers());
  emscripten::function("eventHandlerPostKeyEvent", &eventHandlerPostKeyEvent, emscripten::allow_raw_pointers());
  emscripten::function("eventHandlerPostMouseEvent", &eventHandlerPostMouseEvent, emscripten::allow_raw_pointers());
  emscripten::function("eventHandlerPostWheelEvent", &eventHandlerPostWheelEvent, emscripten::allow_raw_pointers());
  emscripten::function("eventHandlerPostDisplayFormat", &eventHandlerPostDisplayFormat, emscripten::allow_raw_pointers());
  emscripten::function("eventHandlerTryFlush", &eventHandlerTryFlush, emscripten::allow_raw_pointers());
}
} // namespace gfx::em