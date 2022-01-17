#pragma once

#include "enums.hpp"
#include "gfx_wgpu.hpp"
#include "types.hpp"
#include <SDL_events.h>
#include <SDL_video.h>
#include <cassert>
#include <memory>
#include <stdexcept>

namespace gfx {
struct ContextCreationOptions {
	bool debug = false;
	void *overrideNativeWindowHandle = nullptr;

	ContextCreationOptions();
};

struct ImguiContext;
struct Primitive;
struct MaterialBuilderContext;
struct FrameRenderer;
struct Window;
struct ContextImpl;
struct Context {
public:
private:
	std::shared_ptr<ContextImpl> impl;
	FrameRenderer *currentFrameRenderer = nullptr;
	Window *window;
	int2 mainOutputSize;
	bool initialized = false;

	WGPUInstance wgpuInstance;
	WGPUSurface wgpuSurface;
	WGPUAdapter wgpuAdapter;
	WGPUDevice wgpuDevice;
	WGPUQueue wgpuQueue;
	WGPUSwapChain wgpuSwapchain;

	WGPUTextureFormat swapchainFormat;

public:
	Context();
	~Context();

	void init(Window &window, const ContextCreationOptions &options = ContextCreationOptions{});
	void cleanup();
	bool isInitialized() const { return initialized; }

	Window &getWindow() {
		assert(window);
		return *window;
	}

	int2 getMainOutputSize() const { return mainOutputSize; }
	void resizeMainOutput(const int2 &newSize);
	void resizeMainOutputConditional(const int2 &newSize);

	void beginFrame(FrameRenderer *frameRenderer);
	void endFrame(FrameRenderer *frameRenderer);
	FrameRenderer *getFrameRenderer() { return currentFrameRenderer; }
};

} // namespace gfx
