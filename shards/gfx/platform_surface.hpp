#ifndef GFX_PLATFORM_SURFACE
#define GFX_PLATFORM_SURFACE

#include "../core/platform.hpp"
#if SH_APPLE
#include <SDL3/SDL_metal.h>
#endif
#include "gfx_wgpu.hpp"
#if !SH_EMSCRIPTEN
#include "sdl_native_window.hpp"
#endif
#include "window.hpp"

#if SH_WINDOWS
#include <windows.h>
#endif

#if SH_APPLE
extern "C" {
void gfx_metal_get_surface_size(void *surface, uint32_t *width, uint32_t *height);
}
#endif

namespace gfx {

struct WGPUPlatformSurfaceDescriptor : public WGPUSurfaceDescriptor {
  union {
    WGPUChainedStruct chain;
#if SH_EMSCRIPTEN
    WGPUSurfaceDescriptorFromCanvasHTMLSelector html;
#else
    WGPUSurfaceDescriptorFromXlibWindow x11;
    WGPUSurfaceDescriptorFromMetalLayer mtl;
    WGPUSurfaceDescriptorFromWaylandSurface wayland;
    WGPUSurfaceDescriptorFromWindowsHWND win;
    WGPUSurfaceDescriptorFromAndroidNativeWindow android;
#endif
  } platformDesc;

  WGPUPlatformSurfaceDescriptor(gfx::Window *window, void *nativeSurfaceHandle) {
#if !SH_EMSCRIPTEN
    SDL_Window *sdlWindow = window ? window->window : nullptr;
    if (!nativeSurfaceHandle) {
#if SH_LINUX
      nativeSurfaceHandle = SDL_GetNativeWindowPtr(sdlWindow, window->useWayland);
#else
      nativeSurfaceHandle = SDL_GetNativeWindowPtr(sdlWindow, false);
#endif
    }
#endif

    memset(this, 0, sizeof(WGPUPlatformSurfaceDescriptor));

#if SH_WINDOWS
    platformDesc.chain.sType = WGPUSType_SurfaceDescriptorFromWindowsHWND;
    platformDesc.win.hinstance = GetModuleHandle(nullptr);
    platformDesc.win.hwnd = (HWND)nativeSurfaceHandle;
#elif SH_LINUX
    if (window->useWayland) {
      platformDesc.chain.sType = WGPUSType_SurfaceDescriptorFromWaylandSurface;
      platformDesc.wayland.surface = nativeSurfaceHandle;
      auto waylandWindow =
          SDL_GetProperty(SDL_GetWindowProperties(window->window), SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL);
      platformDesc.wayland.display = waylandWindow;
    } else {
      platformDesc.chain.sType = WGPUSType_SurfaceDescriptorFromXlibWindow;
      platformDesc.x11.window = uint64_t(nativeSurfaceHandle);
      auto x11Window = SDL_GetProperty(SDL_GetWindowProperties(window->window), SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
      platformDesc.x11.display = x11Window;
    }
#elif SH_ANDROID
    platformDesc.chain.sType = WGPUSType_SurfaceDescriptorFromAndroidNativeWindow;
    platformDesc.android.window = nativeSurfaceHandle;
#elif SH_APPLE
    platformDesc.chain.sType = WGPUSType_SurfaceDescriptorFromMetalLayer;
    platformDesc.mtl.layer = nativeSurfaceHandle;
#elif SH_EMSCRIPTEN
    platformDesc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
    if (nativeSurfaceHandle)
      platformDesc.html.selector = (const char *)nativeSurfaceHandle;
    else
      platformDesc.html.selector = "#canvas";
#else
#error "Unsupported platform"
#endif
    nextInChain = &platformDesc.chain;
  }
};
} // namespace gfx

#endif // GFX_PLATFORM_SURFACE
