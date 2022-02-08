#pragma once

#include "gfx_wgpu.hpp"
#include "platform.hpp"

#if GFX_WINDOWS
#include <windows.h>
#endif

namespace gfx {
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

	WGPUPlatformSurfaceDescriptor(void *nativeSurfaceHandle) {
		memset(this, 0, sizeof(WGPUPlatformSurfaceDescriptor));
#if GFX_WINDOWS
		platformDesc.chain.sType = WGPUSType_SurfaceDescriptorFromWindowsHWND;
		platformDesc.win.hinstance = GetModuleHandle(nullptr);
		platformDesc.win.hwnd = (HWND)nativeSurfaceHandle;
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
