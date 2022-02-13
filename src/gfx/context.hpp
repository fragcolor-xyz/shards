#pragma once

#include "enums.hpp"
#include "gfx_wgpu.hpp"
#include "types.hpp"
#include <cassert>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

namespace gfx {
struct ContextCreationOptions {
	bool debug = true;
	void *overrideNativeWindowHandle = nullptr;
};

struct CopyBuffer {
	std::vector<uint8_t> data;
};

struct Window;
struct ContextData;
struct ContextMainOutput;
struct Context {
private:
	std::shared_ptr<ContextMainOutput> mainOutput;
	bool initialized = false;

public:
	WGPUInstance wgpuInstance = nullptr;
	WGPUAdapter wgpuAdapter = nullptr;
	WGPUDevice wgpuDevice = nullptr;
	WGPUQueue wgpuQueue = nullptr;

	std::unordered_map<ContextData *, std::weak_ptr<ContextData>> contextDatas;

	size_t numPendingCommandsSubmitted = 0;

public:
	Context();
	~Context();

	// Initialize a context on a window's surface
	void init(Window &window, const ContextCreationOptions &options = ContextCreationOptions{});
	// Initialize headless context
	void init(const ContextCreationOptions &options = ContextCreationOptions{});

	void cleanup();
	bool isInitialized() const { return initialized; }

	Window &getWindow();
	void resizeMainOutputConditional(const int2 &newSize);
	int2 getMainOutputSize() const;
	WGPUTextureView getMainOutputTextureView();
	WGPUTextureFormat getMainOutputFormat() const;
	bool isHeadless() const;

	void beginFrame();
	void endFrame();
	void sync();

	void submit(WGPUCommandBuffer cmdBuffer);

	// start tracking an object implementing WithContextData so it's data is released with this context
	void addContextDataInternal(std::weak_ptr<ContextData> ptr);
	void removeContextDataInternal(ContextData *ptr);

private:
	void initCommon(const ContextCreationOptions &options);

	void present();

	void collectContextData();
	void releaseAllContextData();
};

} // namespace gfx
