#ifndef BFB13E29_4B2D_433E_905E_DAC3525F8FAD
#define BFB13E29_4B2D_433E_905E_DAC3525F8FAD

#include "linalg.hpp"
#include "context.hpp"
#include "window.hpp"
#include "fwd.hpp"

namespace gfx {
struct Context;
struct Renderer;
struct Window;

#ifdef RUST_BINDGEN
/// <div rustbindgen replaces="linalg::aliases::float2"></div>
struct float2 {
  float x, y;
};
/// <div rustbindgen replaces="linalg::aliases::float3"></div>
struct float3 {
  float x, y, z;
};
/// <div rustbindgen replaces="linalg::aliases::float4"></div>
struct float4 {
  float x, y, z, w;
};
/// <div rustbindgen replaces="linalg::aliases::int2"></div>
struct int2 {
  int32_t x, y;
};
/// <div rustbindgen replaces="linalg::aliases::int3"></div>
struct int3 {
  int32_t x, y, z;
};
/// <div rustbindgen replaces="linalg::aliases::int4"></div>
struct int4 {
  int32_t x, y, z, w;
};
#endif
} // namespace gfx

// External functions to work around an issue where using complex
//   return values inside member functions the stack is messed up
extern "C" {
void gfx_Window_getSize_ext(gfx::Window *window, gfx::int2 *out);
void gfx_Window_getDrawableSize_ext(gfx::Window *window, gfx::int2 *out);
float gfx_Window_getUIScale_ext(gfx::Window *window);
void gfx_TexturePtr_getResolution_ext(gfx::TexturePtr *texture, gfx::int2 *out);
}

#endif /* BFB13E29_4B2D_433E_905E_DAC3525F8FAD */
