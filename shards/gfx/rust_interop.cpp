#include "rust_interop.hpp"
#include "texture.hpp"
#include "window.hpp"

using namespace gfx;
extern "C" {
gfx::int2 gfx_Window_getSize_ext(gfx::Window *window) { return window->getSize(); }
gfx::int2 gfx_Window_getDrawableSize_ext(gfx::Window *window) { return window->getDrawableSize(); }
gfx::float2 gfx_Window_getUIScale_ext(gfx::Window *window) { return window->getUIScale(); }
gfx::int2 gfx_TexturePtr_getResolution_ext(gfx::TexturePtr *texture) { return (*texture)->getResolution(); }
}
