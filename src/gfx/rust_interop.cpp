#include "rust_interop.hpp"

namespace gfx {} // namespace gfx

using namespace gfx;
extern "C" {
gfx::float2 gfx_Window_getVirtualDrawableSize_ext(gfx::Window *window) { return window->getVirtualDrawableSize(); }
gfx::float2 gfx_Window_getDrawScale_ext(gfx::Window *window) { return window->getDrawScale(); }
}
