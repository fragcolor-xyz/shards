#include "rust_interop.hpp"

using namespace gfx;
extern "C" {
gfx::int2 gfx_Window_getSize_ext(gfx::Window *window) { return window->getSize(); }
gfx::int2 gfx_Window_getDrawableSize_ext(gfx::Window *window) { return window->getDrawableSize(); }
gfx::float2 gfx_Window_getUIScale_ext(gfx::Window *window) { return window->getUIScale(); }
}
