#include "gfx_wgpu.hpp"
#include "stdio.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif
static void tickAsyncRequests() {
#ifdef __EMSCRIPTEN__
	printf("tickAsyncRequests\n");
	emscripten_sleep(1);
#endif
}

static void adapterReceiver(WGPURequestAdapterStatus status, WGPUAdapter adapter, char const *message, WGPUAdapterReceiverData *data) {
	data->status = status;
	data->completed = true;
	data->adapter = adapter;
	data->message = message;
}

WGPUAdapter wgpuInstanceRequestAdapterSync(WGPUInstance instance, const WGPURequestAdapterOptions *options, WGPUAdapterReceiverData *receiverData) {
	wgpuInstanceRequestAdapter(instance, options, (WGPURequestAdapterCallback)&adapterReceiver, receiverData);
	while (!receiverData->completed) {
		tickAsyncRequests();
	}
	return receiverData->adapter;
}

static void deviceReceiver(WGPURequestDeviceStatus status, WGPUDevice device, char const *message, WGPUDeviceReceiverData *data) {
	data->status = status;
	data->completed = true;
	data->device = device;
	data->message = message;
}

WGPUDevice wgpuAdapterRequestDeviceSync(WGPUAdapter adapter, const WGPUDeviceDescriptor *descriptor, WGPUDeviceReceiverData *receiverData) {
	wgpuAdapterRequestDevice(adapter, descriptor, (WGPURequestDeviceCallback)&deviceReceiver, receiverData);
	while (!receiverData->completed) {
		tickAsyncRequests();
	}
	return receiverData->device;
}