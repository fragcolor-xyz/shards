#include "rust_interop.hpp"
#include "texture.hpp"
#include "window.hpp"

using namespace gfx;
extern "C" {
void gfx_Window_getSize_ext(gfx::Window *window, gfx::int2 *out) { *out = window->getSize(); }
void gfx_Window_getDrawableSize_ext(gfx::Window *window, gfx::int2 *out) { *out = window->getDrawableSize(); }
float gfx_Window_getUIScale_ext(gfx::Window *window) { return window->getUIScale(); }
void gfx_TexturePtr_getResolution_ext(gfx::TexturePtr *texture, gfx::int2 *out) { *out = (*texture)->getResolution(); }
}
