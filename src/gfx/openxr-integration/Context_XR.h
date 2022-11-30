#pragma once

#include <vulkan/vulkan.h>

#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/include/openxr/openxr.h>
#include <openxr/include/openxr/openxr_platform.h>


class Context_XR final
{
public:
  Context_XR();
  ~Context_XR();

  void checkXRDeviceReady(
                            XrViewConfigurationType viewType,
                            XrEnvironmentBlendMode environmentBlendMode,
                            XrFormFactor xrFormFactor
                          );
  void getVulkanExtensionsFromOpenXRInstance();
  bool createDevice(VkSurfaceKHR mirrorSurface);
  void sync() const;

  bool isValid() const;

  XrViewConfigurationType getXrViewType() const;
  XrInstance getXrInstance() const;
  XrSystemId getXrSystemId() const;

  VkInstance getVkInstance() const;
  VkPhysicalDevice getVkPhysicalDevice() const;
  uint32_t getVkDrawQueueFamilyIndex() const;
  VkDevice getVkDevice() const;
  VkQueue getVkDrawQueue() const;
  VkQueue getVkPresentQueue() const;

private:
  bool valid = true;

  // Extension function pointers
  PFN_xrGetVulkanInstanceExtensionsKHR xrGetVulkanInstanceExtensionsKHR = nullptr;
  PFN_xrGetVulkanGraphicsDeviceKHR xrGetVulkanGraphicsDeviceKHR = nullptr;
  PFN_xrGetVulkanDeviceExtensionsKHR xrGetVulkanDeviceExtensionsKHR = nullptr;
  PFN_xrGetVulkanGraphicsRequirementsKHR xrGetVulkanGraphicsRequirementsKHR = nullptr;

  XrInstance xrInstance = nullptr;
  XrSystemId systemId = 0u;

  VkInstance vkInstance = nullptr;
  VkPhysicalDevice physicalDevice = nullptr;
  uint32_t drawQueueFamilyIndex = 0u, presentQueueFamilyIndex = 0u;
  VkDevice vkDevice = nullptr;
  VkQueue drawQueue = nullptr, presentQueue = nullptr;

#ifdef DEBUG
  PFN_xrCreateDebugUtilsMessengerEXT xrCreateDebugUtilsMessengerEXT = nullptr;
  PFN_xrDestroyDebugUtilsMessengerEXT xrDestroyDebugUtilsMessengerEXT = nullptr;
  XrDebugUtilsMessengerEXT xrDebugUtilsMessenger = nullptr;

  PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = nullptr;
  PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = nullptr;
  VkDebugUtilsMessengerEXT vkDebugUtilsMessenger = nullptr;
#endif
};