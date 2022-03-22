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

static void adapterReceiver(WGPURequestAdapterStatus status, WGPUAdapter adapter, char const *message,
                            WGPUAdapterReceiverData *data) {
  data->status = status;
  data->completed = true;
  data->adapter = adapter;
  data->message = message;
}

WGPUAdapter wgpuInstanceRequestAdapterSync(WGPUInstance instance, const WGPURequestAdapterOptions *options,
                                           WGPUAdapterReceiverData *receiverData) {
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

WGPUDevice wgpuAdapterRequestDeviceSync(WGPUAdapter adapter, const WGPUDeviceDescriptor *descriptor,
                                        WGPUDeviceReceiverData *receiverData) {
  wgpuAdapterRequestDevice(adapter, descriptor, (WGPURequestDeviceCallback)&deviceReceiver, receiverData);
  while (!receiverData->completed) {
    tickAsyncRequests();
  }
  return receiverData->device;
}

WGPULimits wgpuGetDefaultLimits() {
  return WGPULimits{
      .maxTextureDimension1D = 0,
      .maxTextureDimension2D = 0,
      .maxTextureDimension3D = 0,
      .maxTextureArrayLayers = 0,
      .maxBindGroups = 0,
      .maxDynamicUniformBuffersPerPipelineLayout = 0,
      .maxDynamicStorageBuffersPerPipelineLayout = 0,
      .maxSampledTexturesPerShaderStage = 0,
      .maxSamplersPerShaderStage = 0,
      .maxStorageBuffersPerShaderStage = 0,
      .maxStorageTexturesPerShaderStage = 0,
      .maxUniformBuffersPerShaderStage = 0,
      .maxUniformBufferBindingSize = 0,
      .maxStorageBufferBindingSize = 0,
      .minUniformBufferOffsetAlignment = 0,
      .minStorageBufferOffsetAlignment = 0,
      .maxVertexBuffers = 0,
      .maxVertexAttributes = 0,
      .maxVertexBufferArrayStride = 0,
      .maxInterStageShaderComponents = 0,
      .maxComputeWorkgroupStorageSize = 0,
      .maxComputeInvocationsPerWorkgroup = 0,
      .maxComputeWorkgroupSizeX = 0,
      .maxComputeWorkgroupSizeY = 0,
      .maxComputeWorkgroupSizeZ = 0,
      .maxComputeWorkgroupsPerDimension = 0,
  };
}

WGPULimits wgpuGetUndefinedLimits() {
  return WGPULimits{
      .maxTextureDimension1D = WGPU_LIMIT_U32_UNDEFINED,
      .maxTextureDimension2D = WGPU_LIMIT_U32_UNDEFINED,
      .maxTextureDimension3D = WGPU_LIMIT_U32_UNDEFINED,
      .maxTextureArrayLayers = WGPU_LIMIT_U32_UNDEFINED,
      .maxBindGroups = WGPU_LIMIT_U32_UNDEFINED,
      .maxDynamicUniformBuffersPerPipelineLayout = WGPU_LIMIT_U32_UNDEFINED,
      .maxDynamicStorageBuffersPerPipelineLayout = WGPU_LIMIT_U32_UNDEFINED,
      .maxSampledTexturesPerShaderStage = WGPU_LIMIT_U32_UNDEFINED,
      .maxSamplersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED,
      .maxStorageBuffersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED,
      .maxStorageTexturesPerShaderStage = WGPU_LIMIT_U32_UNDEFINED,
      .maxUniformBuffersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED,
      .maxUniformBufferBindingSize = WGPU_LIMIT_U64_UNDEFINED,
      .maxStorageBufferBindingSize = WGPU_LIMIT_U64_UNDEFINED,
      .minUniformBufferOffsetAlignment = WGPU_LIMIT_U32_UNDEFINED,
      .minStorageBufferOffsetAlignment = WGPU_LIMIT_U32_UNDEFINED,
      .maxVertexBuffers = WGPU_LIMIT_U32_UNDEFINED,
      .maxVertexAttributes = WGPU_LIMIT_U32_UNDEFINED,
      .maxVertexBufferArrayStride = WGPU_LIMIT_U32_UNDEFINED,
      .maxInterStageShaderComponents = WGPU_LIMIT_U32_UNDEFINED,
      .maxComputeWorkgroupStorageSize = WGPU_LIMIT_U32_UNDEFINED,
      .maxComputeInvocationsPerWorkgroup = WGPU_LIMIT_U32_UNDEFINED,
      .maxComputeWorkgroupSizeX = WGPU_LIMIT_U32_UNDEFINED,
      .maxComputeWorkgroupSizeY = WGPU_LIMIT_U32_UNDEFINED,
      .maxComputeWorkgroupSizeZ = WGPU_LIMIT_U32_UNDEFINED,
      .maxComputeWorkgroupsPerDimension = WGPU_LIMIT_U32_UNDEFINED,
  };
}

void gfxWgpuDeviceGetLimits(WGPUDevice device, WGPUSupportedLimits *outLimits) {
#ifdef WEBGPU_NATIVE
  wgpuDeviceGetLimits(device, outLimits);
#else
  // NOTE: EMSCRIPTEN Doesn't implement limits at this moment, assume default everywhere
  outLimits->limits = wgpuGetDefaultLimits();
#endif
}
