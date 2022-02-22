#pragma once

#include "gfx_wgpu.hpp"
#include "platform.hpp"
#include "sdl_native_window.hpp"

#if GFX_WINDOWS
#include <windows.h>
#endif

namespace gfx {

struct MetalViewContainer {
  SDL_MetalView view{};
  void *layer{};

  MetalViewContainer(SDL_Window *window) {
    view = SDL_Metal_CreateView(window);
    layer = SDL_Metal_GetLayer(view);
  }
  ~MetalViewContainer() { SDL_Metal_DestroyView(view); }
  MetalViewContainer(const MetalViewContainer &other) = delete;
  operator void *() const { return layer; }
};

struct WGPUPlatformSurfaceDescriptor : public WGPUSurfaceDescriptor {
  union {
    WGPUChainedStruct chain;
#if GFX_EMSCRIPTEN
    WGPUSurfaceDescriptorFromCanvasHTMLSelector html;
#else
    WGPUSurfaceDescriptorFromXlib x11;
    WGPUSurfaceDescriptorFromMetalLayer mtl;
    WGPUSurfaceDescriptorFromWaylandSurface wayland;
    WGPUSurfaceDescriptorFromWindowsHWND win;
#endif
  } platformDesc;

  WGPUPlatformSurfaceDescriptor(SDL_Window *sdlWindow, void *nativeSurfaceHandle) {
    if (!nativeSurfaceHandle)
      nativeSurfaceHandle = SDL_GetNativeWindowPtr(sdlWindow);

    memset(this, 0, sizeof(WGPUPlatformSurfaceDescriptor));
#if GFX_WINDOWS
    platformDesc.chain.sType = WGPUSType_SurfaceDescriptorFromWindowsHWND;
    platformDesc.win.hinstance = GetModuleHandle(nullptr);
    platformDesc.win.hwnd = (HWND)nativeSurfaceHandle;
#elif GFX_LINUX
    platformDesc.chain.sType = WGPUSType_SurfaceDescriptorFromXlib;
    platformDesc.x11.window = uint32_t(size_t(nativeSurfaceHandle));
    platformDesc.x11.display = SDL_GetNativeDisplayPtr(sdlWindow);
#elif GFX_APPLE
    platformDesc.chain.sType = WGPUSType_SurfaceDescriptorFromMetalLayer;
    platformDesc.mtl.layer = nativeSurfaceHandle;
#elif GFX_EMSCRIPTEN
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
