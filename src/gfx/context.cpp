#include "context.hpp"
#include "context_data.hpp"
#include "error_utils.hpp"
#include "sdl_native_window.hpp"
#include "window.hpp"
#include <SDL_events.h>
#include <SDL_video.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace gfx {

void ErrorScope::push(WGPUErrorFilter filter) {
#ifndef WEBGPU_NATIVE
	wgpuDevicePushErrorScope(context->wgpuDevice, filter);
#endif
}
void ErrorScope::pop(ErrorScope::Function &&function) {
	this->function = [=](WGPUErrorType type, char const *message) {
		function(type, message);
		processed = true;
	};

#ifndef WEBGPU_NATIVE
	wgpuDevicePopErrorScope(context->wgpuDevice, &staticCallback, this);
#else
	processed = true;
#endif
}
void ErrorScope::staticCallback(WGPUErrorType type, char const *message, void *userData) {
	ErrorScope *self = (ErrorScope *)userData;
	self->function(type, message);
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

	void *surfaceHandle = options.overrideNativeWindowHandle;
	if (!surfaceHandle)
		surfaceHandle = SDL_GetNativeWindowPtr(window.window);

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

#ifdef WEBGPU_NATIVE
	wgpuSetLogCallback([](WGPULogLevel level, const char *msg) {
		switch (level) {
		case WGPULogLevel_Error:
			spdlog::error("WEBGPU: {}", msg);
			break;
		case WGPULogLevel_Warn:
			spdlog::warn("WEBGPU: {}", msg);
			break;
		case WGPULogLevel_Info:
			spdlog::info("WEBGPU: {}", msg);
			break;
		case WGPULogLevel_Debug:
			spdlog::debug("WEBGPU: {}", msg);
			break;
		case WGPULogLevel_Trace:
			spdlog::trace("WEBGPU: {}", msg);
			break;
		default:
			break;
		}
	});
	if (options.debug) {
		wgpuSetLogLevel(WGPULogLevel_Debug);
	} else {
		wgpuSetLogLevel(WGPULogLevel_Info);
	}
#endif

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

	wgpuDeviceSetUncapturedErrorCallback(
		wgpuDevice,
		[](WGPUErrorType type, char const *message, void *userdata) {
			spdlog::error("WEBGPU: {} ({})", message, type);
			;
		},
		this);

	wgpuQueue = wgpuDeviceGetQueue(wgpuDevice);

	swapchainFormat = wgpuSurfaceGetPreferredFormat(wgpuSurface, wgpuAdapter);
	resizeMainOutput(mainOutputSize);

	initialized = true;
}

void Context::cleanup() {
	spdlog::debug("GFX.Context cleanup");
	initialized = false;

	releaseAllContextDataObjects();

	cleanupSwapchain();
	WGPU_SAFE_RELEASE(wgpuQueueRelease, wgpuQueue);
	WGPU_SAFE_RELEASE(wgpuDeviceRelease, wgpuDevice);
	wgpuSurface = nullptr; // TODO: drop once c binding exists
	wgpuAdapter = nullptr; // TODO: drop once c binding exists
}

void Context::resizeMainOutput(const int2 &newSize) {
	spdlog::debug("GFX.Context resized width: {} height: {}", newSize.x, newSize.y);
	this->mainOutputSize = newSize;

	cleanupSwapchain();

	WGPUSwapChainDescriptor swapchainDesc = {};
	swapchainDesc.format = swapchainFormat;
	swapchainDesc.width = newSize.x;
	swapchainDesc.height = newSize.y;
	swapchainDesc.presentMode = WGPUPresentMode_Mailbox;
	swapchainDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopyDst;
	wgpuSwapchain = wgpuDeviceCreateSwapChain(wgpuDevice, wgpuSurface, &swapchainDesc);
	if (!wgpuSwapchain) {
		throw formatException("Failed to create swapchain");
	}
}

void Context::resizeMainOutputConditional(const int2 &newSize) {
	if (mainOutputSize != newSize) {
		resizeMainOutput(newSize);
	}
}

void Context::cleanupSwapchain() { WGPU_SAFE_RELEASE(wgpuSwapChainRelease, wgpuSwapchain); }

ErrorScope &Context::pushErrorScope(WGPUErrorFilter filter) {
	auto errorScope = std::make_shared<ErrorScope>(this);
	errorScopes.push_back(errorScope);
	errorScope->push(filter);
	return *errorScope.get();
}

void Context::addContextDataObjectInternal(std::weak_ptr<WithContextData> ptr) {
	std::shared_ptr<WithContextData> sharedPtr = ptr.lock();
	if (sharedPtr) {
		contextDataObjects.insert_or_assign(sharedPtr.get(), ptr);
	}
}

void Context::removeContextDataObjectInternal(WithContextData *ptr) { contextDataObjects.erase(ptr); }

void Context::collectContextDataObjects() {
	for (auto it = contextDataObjects.begin(); it != contextDataObjects.end();) {
		if (it->second.expired()) {
			it = contextDataObjects.erase(it);
		} else {
			it++;
		}
	}
}

void Context::releaseAllContextDataObjects() {
	for (auto &obj : contextDataObjects) {
		if (!obj.second.expired()) {
			obj.first->releaseContextDataCondtional();
		}
	}
	contextDataObjects.clear();
}

void Context::beginFrame() {
	sync();
	collectContextDataObjects();
	errorScopes.clear();
}
void Context::endFrame() { present(); }

void Context::sync() {
#ifdef WEBGPU_NATIVE
	wgpuDevicePoll(wgpuDevice, true);
#endif
}

void Context::present() { wgpuSwapChainPresent(wgpuSwapchain); }

} // namespace gfx
