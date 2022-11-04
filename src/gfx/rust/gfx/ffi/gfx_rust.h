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


WGPUInstance wgpuCreateInstanceEx(WGPUInstanceDescriptor const *descriptor);
void wgpuInstanceGetPropertiesEx(WGPUInstance instance, WGPUInstanceProperties *properties);

void wgpuInstanceRequestAdapterEx(WGPUInstance instance, WGPURequestAdapterOptions const * options, WGPURequestAdapterCallback callback, void * userdata);
void wgpuAdapterRequestDeviceEx(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor, WGPURequestDeviceCallback callback, void * userdata);

#ifdef __cplusplus
}
#endif

#endif /* A1B3E450_AA6A_40D5_823B_6AF9EB4DA411 */
