#include "context.hpp"
#include "error_utils.hpp"
#include "sdl_native_window.hpp"
#include "window.hpp"
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

namespace gfx {

ContextCreationOptions::ContextCreationOptions() {
#ifdef GFX_DEBUG
	debug = true;
#endif
}

Context::Context() {}
Context::~Context() { cleanup(); }

struct WGPUPlatformSurfaceDescriptor : public WGPUSurfaceDescriptor {
	union {
		WGPUChainedStruct chain;
		WGPUSurfaceDescriptorFromXlib x11;
		WGPUSurfaceDescriptorFromCanvasHTMLSelector html;
		WGPUSurfaceDescriptorFromMetalLayer mtl;
		WGPUSurfaceDescriptorFromWaylandSurface wayland;
		WGPUSurfaceDescriptorFromWindowsHWND win;
	} platformDesc;

	WGPUPlatformSurfaceDescriptor(void *nativeSurfaceHandle) {
		memset(this, 0, sizeof(WGPUPlatformSurfaceDescriptor));
#if defined(_WIN32)
		platformDesc.chain.sType = WGPUSType_SurfaceDescriptorFromWindowsHWND;
		platformDesc.win.hinstance = GetModuleHandle(nullptr);
		platformDesc.win.hwnd = (HWND)nativeSurfaceHandle;
#elif defined(__EMSCRIPTEN__)
		platformDesc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
		platformDesc.html.selector = (const char *)nativeSurfaceHandle;
#else
#error "Unsupported platform"
#endif
		nextInChain = &platformDesc.chain;
	}
};

void Context::init(Window &window, const ContextCreationOptions &options) {
	spdlog::debug("GFX.Context init");

	assert(!isInitialized());

	this->window = &window;
	mainOutputSize = window.getDrawableSize();

	// TODO
	void *surfaceHandle = SDL_GetNativeWindowPtr(window.window);
	WGPUPlatformSurfaceDescriptor surfDesc(surfaceHandle);

	wgpuSurface = wgpuInstanceCreateSurface(wgpuInstance, &surfDesc);

	WGPURequestAdapterOptions requestAdapter = {};
	requestAdapter.powerPreference = WGPUPowerPreference_HighPerformance;
	requestAdapter.compatibleSurface = wgpuSurface;
	requestAdapter.forceFallbackAdapter = false;

#ifdef WEBGPU_NATIVE
	WGPUAdapterExtras adapterExtras = {};
	requestAdapter.nextInChain = &adapterExtras.chain;
	adapterExtras.chain.sType = (WGPUSType)WGPUSType_AdapterExtras;
	adapterExtras.backend = WGPUBackendType_D3D12;
#endif

	WGPUAdapterReceiverData adapterReceiverData = {};
	wgpuAdapter = wgpuInstanceRequestAdapterSync(wgpuInstance, &requestAdapter, &adapterReceiverData);
	if (adapterReceiverData.status != WGPURequestAdapterStatus_Success) {
		throw formatException("Failed to create wgpuAdapter: {} {}", adapterReceiverData.status, adapterReceiverData.message);
	}
	spdlog::debug("Created wgpuAdapter");

	WGPURequiredLimits requiredLimits = {};
	auto &limits = requiredLimits.limits;
	limits.maxTextureDimension1D = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxTextureDimension2D = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxTextureDimension3D = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxTextureArrayLayers = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxBindGroups = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxDynamicUniformBuffersPerPipelineLayout = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxDynamicStorageBuffersPerPipelineLayout = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxSampledTexturesPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxSamplersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxStorageBuffersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxStorageTexturesPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxUniformBuffersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxUniformBufferBindingSize = WGPU_LIMIT_U64_UNDEFINED;
	limits.maxStorageBufferBindingSize = WGPU_LIMIT_U64_UNDEFINED;
	limits.minUniformBufferOffsetAlignment = WGPU_LIMIT_U32_UNDEFINED;
	limits.minStorageBufferOffsetAlignment = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxVertexBuffers = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxVertexAttributes = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxVertexBufferArrayStride = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxInterStageShaderComponents = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxComputeWorkgroupStorageSize = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxComputeInvocationsPerWorkgroup = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxComputeWorkgroupSizeX = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxComputeWorkgroupSizeY = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxComputeWorkgroupSizeZ = WGPU_LIMIT_U32_UNDEFINED;
	limits.maxComputeWorkgroupsPerDimension = WGPU_LIMIT_U32_UNDEFINED;

	WGPUDeviceDescriptor deviceDesc = {};
	deviceDesc.requiredLimits = &requiredLimits;

#ifdef WEBGPU_NATIVE
	WGPUDeviceExtras deviceExtras = {};
	deviceDesc.nextInChain = &deviceExtras.chain;
#endif

	WGPUDeviceReceiverData deviceReceiverData = {};
	wgpuDevice = wgpuAdapterRequestDeviceSync(wgpuAdapter, &deviceDesc, &deviceReceiverData);
	if (deviceReceiverData.status != WGPURequestDeviceStatus_Success) {
		throw formatException("Failed to create device: {} {}", deviceReceiverData.status, deviceReceiverData.message);
	}
	spdlog::debug("Create wgpuDevice");

	wgpuQueue = wgpuDeviceGetQueue(wgpuDevice);

	swapchainFormat = wgpuSurfaceGetPreferredFormat(wgpuSurface, wgpuAdapter);
}

void Context::cleanup() {
	spdlog::debug("GFX.Context cleanup");

	if (isInitialized()) {
	}
}

void Context::resizeMainOutput(const int2 &newSize) {
	spdlog::debug("GFX.Context resized width: {} height: {}", newSize.x, newSize.y);
	this->mainOutputSize = newSize;
	// TODO
}

void Context::resizeMainOutputConditional(const int2 &newSize) {
	if (mainOutputSize != newSize) {
		resizeMainOutput(newSize);
	}
}

void Context::beginFrame(FrameRenderer *frameRenderer) {
	if (currentFrameRenderer != nullptr)
		throw std::runtime_error("Frame already being rendered");
	currentFrameRenderer = frameRenderer;
}

void Context::endFrame(FrameRenderer *frameRenderer) {
	assert(currentFrameRenderer == frameRenderer);
	currentFrameRenderer = nullptr;
}

} // namespace gfx
