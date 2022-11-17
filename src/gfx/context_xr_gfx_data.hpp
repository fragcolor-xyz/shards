#pragma once



#include "wgpu_handle.hpp"


#include <Vulkan-Headers/include/vulkan/vulkan.hpp>
#include <Vulkan-Headers/include/vulkan/vulkan_raii.hpp>

#include "linalg.hpp"
#include <optional>

namespace gfx {
  struct WGPUVulkanShared {
    //[t] So this vkLoader thingy + vkDevice is a loader for any and all vulkan native functions we might need to call. 
    //[t] The way you call a vulkan function is by having a reference to wgpuVulkanShared first.
    //[t] Then if for example you need to call a vkfunction like vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr)
    //[t] if it's in a specific class like vkDevice, you use wgpuVulkanShared->vkDevice.enumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr, wgpuVulkanShared->vkLoader)
    //[t] if it's global, you use vk::enumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr, wgpuVulkanShared->vkLoader)
    //[t] note you have to send the wgpuVulkanShared->vkLoader as last argument.
    vk::DispatchLoaderDynamic vkLoader;
    vk::PhysicalDevice vkPhysicalDevice;//[t] VkPhysicalDevice
    vk::Instance vkInstance;//[t] VkInstance
    vk::Device vkDevice;//[t] VkDevice
    vk::Queue vkQueue;//[t] VkQueue, draw queue. The present queue is wgpuPresent afaiks

    uint32_t queueIndex;
    uint32_t queueFamilyIndex;

    WGPUInstance wgpuInstance{};
    WGPUAdapter wgpuAdapter{};
    WGPUDevice wgpuDevice{};
    WGPUQueue wgpuQueue{};
  };

} // namespace gfx
