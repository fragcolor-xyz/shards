#include "rust_interop.hpp"

using namespace gfx;
extern "C" {
gfx::float2 gfx_Window_getVirtualDrawableSize_ext(gfx::Window *window) { return window->getVirtualDrawableSize(); }
gfx::float2 gfx_Window_getDrawScale_ext(gfx::Window *window) { return window->getDrawScale(); }
}
