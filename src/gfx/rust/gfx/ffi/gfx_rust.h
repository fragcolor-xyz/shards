#ifndef A1B3E450_AA6A_40D5_823B_6AF9EB4DA411
#define A1B3E450_AA6A_40D5_823B_6AF9EB4DA411

#include "wgpu.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum WGPUNativeSTypeEx {
  // Start at 7 to prevent collisions with webgpu STypes
  WGPUNativeSTypeEx_InstanceDescriptorVK = 0x70000001,
  WGPUNativeSTypeEx_InstancePropertiesVK = 0x70000002,
  WGPUNativeSTypeEx_RequestAdapterOptionsVK = 0x70000003,
  WGPUNativeSTypeEx_DeviceDescriptorVK = 0x70000004,
  WGPUNativeSTypeEx_DevicePropertiesVK = 0x70000005,
  WGPUNativeSTypeEx_ExternalTextureDescriptorVK = 0x70000006,
  WGPUNativeSTypeEx_Force32 = 0x7FFFFFFF
} WGPUNativeSTypeEx;

typedef struct WGPUInstanceProperties {
  WGPUChainedStructOut *nextInChain;
} WGPUInstanceProperties;

typedef struct WGPUInstancePropertiesVK {
  WGPUChainedStructOut chain;
  const void *instance;
  const void *getInstanceProcAddr;
} WGPUInstancePropertiesVK;

typedef struct WGPUInstanceDescriptorVK {
  WGPUChainedStruct chain;
  // List of required vulkan extension for the created instance
  const char **requiredExtensions;
  uint32_t requiredExtensionCount;
} WGPUInstanceDescriptorVK;

typedef struct WGPURequestAdapterOptionsVK {
  WGPUChainedStruct chain;
  // When set, explicitly use this VkPhysicalDevice
  const void *physicalDevice;
} WGPURequestAdapterOptionsVK;

typedef struct WGPUDeviceDescriptorVK {
  WGPUChainedStruct chain;
  // List of required vulkan extension for the created device
  const char **requiredExtensions;
  uint32_t requiredExtensionCount;
} WGPUDeviceDescriptorVK;

typedef struct WGPUDeviceProperties {
  WGPUChainedStructOut *nextInChain;
} WGPUDeviceProperties;

typedef struct WGPUDevicePropertiesVK {
  WGPUChainedStructOut chain;
  const void *device;
  const void *queue;
  uint32_t queueIndex;
  uint32_t queueFamilyIndex;
} WGPUDevicePropertiesVK;

typedef struct WGPUTextureViewDescriptorVK {
  WGPUChainedStruct chain;
  void *handle;
  WGPUExtent3D size;
  uint32_t sampleCount;
  WGPUTextureUsageFlags usage;
} WGPUTextureViewDescriptorVK;

typedef struct WGPUExternalTextureDescriptor {
  WGPUChainedStruct *nextInChain;
} WGPUExternalTextureDescriptor;

typedef struct WGPUExternalTextureDescriptorVK {
  WGPUChainedStruct chain;
  void *image;
} WGPUExternalTextureDescriptorVK;

WGPUInstance wgpuCreateInstanceEx(WGPUInstanceDescriptor const *descriptor);
void wgpuInstanceRequestAdapterEx(WGPUInstance instance, WGPURequestAdapterOptions const *options,
                                  WGPURequestAdapterCallback callback, void *userdata);
void wgpuAdapterRequestDeviceEx(WGPUAdapter adapter, WGPUDeviceDescriptor const *descriptor, WGPURequestDeviceCallback callback,
                                void *userdata);

void wgpuInstanceGetPropertiesEx(WGPUInstance instance, WGPUInstanceProperties *properties);
void wgpuDeviceGetPropertiesEx(WGPUDevice device, WGPUDeviceProperties *properties);

WGPUTextureView wgpuCreateExternalTextureView(WGPUDevice device, WGPUTextureDescriptor const *textureDescriptor,
                                              WGPUTextureViewDescriptor const *viewDescriptor,
                                              WGPUExternalTextureDescriptor const *externalTexture);

typedef struct WGPUExternalPresentVK {
  void *waitSemaphore;
} WGPUExternalPresentVK;

void wgpuPrepareExternalPresentVK(WGPUQueue queue, WGPUExternalPresentVK *present);

#ifdef __cplusplus
}
#endif

#endif /* A1B3E450_AA6A_40D5_823B_6AF9EB4DA411 */
