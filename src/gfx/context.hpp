#pragma once

#include "enums.hpp"
#include "gfx_wgpu.hpp"
#include "types.hpp"
#include <cassert>
#include <list>
#include <map>
#include <memory>
#include <set>

namespace gfx {
struct ContextCreationOptions {
	bool debug = true;
	void *overrideNativeWindowHandle = nullptr;
};

struct CopyBuffer {
	std::vector<uint8_t> data;
};

struct Context;
struct ErrorScope {
	typedef std::function<void(WGPUErrorType type, char const *message)> Function;

	bool processed = false;
	Function function;
	Context *context;

	ErrorScope(Context *context) : context(context){};
	void push(WGPUErrorFilter filter);
	void pop(ErrorScope::Function &&function);
	static void staticCallback(WGPUErrorType type, char const *message, void *userData);
};

struct ImguiContext;
struct Primitive;
struct MaterialBuilderContext;
struct Window;
struct ContextBackend;
struct WithContextData;
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

	std::vector<std::shared_ptr<ErrorScope>> errorScopes;
	std::unordered_map<WithContextData *, std::weak_ptr<WithContextData>> contextDataObjects;

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

	// pushes state attached to a popErrorScope callback
	ErrorScope &pushErrorScope(WGPUErrorFilter filter = WGPUErrorFilter::WGPUErrorFilter_Validation);

	void beginFrame();
	void endFrame();

	// start tracking an object implementing WithContextData so it's data is released with this context
	void addContextDataObjectInternal(std::weak_ptr<WithContextData> ptr);
	void removeContextDataObjectInternal(WithContextData *ptr);

private:
	void initCommon(const ContextCreationOptions &options);

	void present();
	void sync();

	void collectContextDataObjects();
	void releaseAllContextDataObjects();
};

} // namespace gfx
