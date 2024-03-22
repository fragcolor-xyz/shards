#include "rust_interop.hpp"
#include "texture.hpp"
#include "window.hpp"

using namespace gfx;
extern "C" {
void gfx_Window_getSize_ext(gfx::Window *window, gfx::int2 *out) { *out = window->getSize(); }
void gfx_Window_getDrawableSize_ext(gfx::Window *window, gfx::int2 *out) { *out = window->getDrawableSize(); }
float gfx_Window_getUIScale_ext(gfx::Window *window) { return window->getUIScale(); }
void gfx_TexturePtr_getResolution_ext(gfx::TexturePtr *texture, gfx::int2 *out) {
  if (!*texture)
    *out = gfx::int2{0, 0};
  else
    *out = (*texture)->getResolution();
}

void gfx_TexturePtr_refAt(GenericSharedPtr *dst, const gfx::TexturePtr *texture) {
  static_assert(sizeof(gfx::TexturePtr) <= sizeof(GenericSharedPtr), "Size mismatch");
  std::construct_at(reinterpret_cast<gfx::TexturePtr *>(dst), *texture);
}
void gfx_TexturePtr_unrefAt(GenericSharedPtr *dst) { std::destroy_at(reinterpret_cast<gfx::TexturePtr *>(dst)); }
}
