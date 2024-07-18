#include "window.hpp"
#include "fmt.hpp"
#include "gfx_events_em.hpp"
#include "log.hpp"

namespace gfx {

void Window::init(const WindowCreationOptions &options) {
  if (em::getEventHandler()->canvasTag.empty())
    throw std::logic_error("Can not create window, Canvas tag not set");
  if (em::getEventHandler()->canvasContainerTag.empty())
    throw std::logic_error("Can not create window, Canvas container tag not set");
  // noop
}

void Window::cleanup() {
  // noop
}

void Window::update() {
  // noop
}

void *Window::getNativeWindowHandle() { return (void *)em::getEventHandler()->canvasTag.c_str(); }

int2 Window::getDrawableSize() const {
  auto &astate = em::getEventHandler()->astate;
  return int2(astate.canvasWidth, astate.canvasHeight);
}

float2 Window::getInputScale() const { return float2(getDrawableSize()) / float2(getSize()); }

int2 Window::getSize() const {
  auto &astate = em::getEventHandler()->astate;
  return int2(astate.displayWidth, astate.displayHeight);
}

void Window::resize(int2 targetSize) {
  // noop
}

int2 Window::getPosition() const {
  // noop
  return int2();
}

void Window::move(int2 targetPosition) {
  // noop
}

float Window::getUIScale() const {
  auto &astate = em::getEventHandler()->astate;
  return astate.pixelRatio;
}

bool Window::isWindowSizeInPixels() { return false; }

Window::~Window() { cleanup(); }
} // namespace gfx
