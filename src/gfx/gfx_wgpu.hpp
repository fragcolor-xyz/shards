#pragma once

#ifdef WEBGPU_NATIVE
#define wgpuBufferRelease(...) wgpuBufferDrop(__VA_ARGS__)
#define wgpuCommandEncoderRelease(...) wgpuCommandEncoderDrop(__VA_ARGS__)
#define wgpuDeviceRelease(...) wgpuDeviceDrop(__VA_ARGS__)
#define wgpuQuerySetRelease(...) wgpuQuerySetDrop(__VA_ARGS__)
#define wgpuRenderPipelineRelease(...) wgpuRenderPipelineDrop(__VA_ARGS__)
#define wgpuTextureRelease(...) wgpuTextureDrop(__VA_ARGS__)
#define wgpuTextureViewRelease(...) wgpuTextureViewDrop(__VA_ARGS__)
#define wgpuSamplerRelease(...) wgpuSamplerDrop(__VA_ARGS__)
#define wgpuBindGroupLayoutRelease(...) wgpuBindGroupLayoutDrop(__VA_ARGS__)
#define wgpuPipelineLayoutRelease(...) wgpuPipelineLayoutDrop(__VA_ARGS__)
#define wgpuBindGroupRelease(...) wgpuBindGroupDrop(__VA_ARGS__)
#define wgpuShaderModuleRelease(...) wgpuShaderModuleDrop(__VA_ARGS__)
#define wgpuCommandBufferRelease(...) wgpuCommandBufferDrop(__VA_ARGS__)
#define wgpuRenderBundleRelease(...) wgpuRenderBundleDrop(__VA_ARGS__)
#define wgpuComputePipelineRelease(...) wgpuComputePipelineDrop(__VA_ARGS__)
#define wgpuRenderPassEncoderRelease(...)
#define wgpuSwapChainRelease(...)
#define wgpuQueueRelease(...)
extern "C" {
#include <wgpu.h>
}
#else
extern "C" {
#include <webgpu/webgpu.h>
}
#endif

struct WGPUAdapterReceiverData {
  bool completed = false;
  WGPURequestAdapterStatus status;
  const char *message;
  WGPUAdapter adapter;
};
WGPUAdapter wgpuInstanceRequestAdapterSync(WGPUInstance instance, const WGPURequestAdapterOptions *options,
                                           WGPUAdapterReceiverData *receiverData);

struct WGPUDeviceReceiverData {
  bool completed = false;
  WGPURequestDeviceStatus status;
  const char *message;
  WGPUDevice device;
};
WGPUDevice wgpuAdapterRequestDeviceSync(WGPUAdapter adapter, const WGPUDeviceDescriptor *descriptor,
                                        WGPUDeviceReceiverData *receiverData);

#define WGPU_SAFE_RELEASE(_fn, _x) \
  if (_x) {                        \
    _fn(_x);                       \
    _x = nullptr;                  \
  }
