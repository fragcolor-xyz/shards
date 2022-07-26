#ifndef BFB13E29_4B2D_433E_905E_DAC3525F8FAD
#define BFB13E29_4B2D_433E_905E_DAC3525F8FAD

#include "linalg.hpp"
#include "context.hpp"
#include "window.hpp"

namespace gfx {
struct Context;
struct Renderer;
struct Window;

#ifdef RUST_BINDGEN
/// <div rustbindgen replaces="linalg::aliases::float2"></div>
struct float2 {
  float x, y;
  char padding[16];
};
/// <div rustbindgen replaces="linalg::aliases::float3"></div>
struct float3 {
  float x, y, z;
  char padding[20];
};
/// <div rustbindgen replaces="linalg::aliases::float4"></div>
struct float4 {
  float x, y, z, w;
  char padding[16];
};
/// <div rustbindgen replaces="linalg::aliases::int2"></div>
struct int2 {
  int32_t x, y;
  char padding[16];
};
/// <div rustbindgen replaces="linalg::aliases::int3"></div>
struct int3 {
  int32_t x, y, z;
  char padding[20];
};
/// <div rustbindgen replaces="linalg::aliases::int4"></div>
struct int4 {
  int32_t x, y, z, w;
  char padding[16];
};
#endif
} // namespace gfx

// External functions to work around an issue where using complex
//   return values inside member functions the stack is messed up
extern "C" {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
gfx::int2 gfx_Window_getSize_ext(gfx::Window *window);
gfx::int2 gfx_Window_getDrawableSize_ext(gfx::Window *window);
gfx::float2 gfx_Window_getUIScale_ext(gfx::Window *window);
#pragma clang diagnostic push
}

#endif /* BFB13E29_4B2D_433E_905E_DAC3525F8FAD */
