#include "Context_XR.h"

#include "Util.h"


#include <sstream>

#ifdef DEBUG
  #include <array>
  #include <iostream>
#endif



Context_XR::Context_XR()
{
  

  // Get all supported OpenXR instance extensions
  std::vector<XrExtensionProperties> supportedOpenXRInstanceExtensions;
  {
    uint32_t instanceExtensionCount;
    XrResult result = xrEnumerateInstanceExtensionProperties(nullptr, 0u, &instanceExtensionCount, nullptr);
    if (XR_FAILED(result))
    {
      util::error(Error::GenericOpenXR);
      valid = false;
      return;
    }

    supportedOpenXRInstanceExtensions.resize(instanceExtensionCount);
    for (XrExtensionProperties& extensionProperty : supportedOpenXRInstanceExtensions)
    {
      extensionProperty.type = XR_TYPE_EXTENSION_PROPERTIES;
      extensionProperty.next = nullptr;
    }

    result = xrEnumerateInstanceExtensionProperties(nullptr, instanceExtensionCount, &instanceExtensionCount,
                                                    supportedOpenXRInstanceExtensions.data());
    if (XR_FAILED(result))
    {
      util::error(Error::GenericOpenXR);
      valid = false;
      return;
    }
  }

  // Create an OpenXR instance
  {
    XrApplicationInfo applicationInfo;
    applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    applicationInfo.applicationVersion = static_cast<uint32_t>(XR_MAKE_VERSION(0, 1, 0));
    applicationInfo.engineVersion = static_cast<uint32_t>(XR_MAKE_VERSION(0, 1, 0));
    strncpy_s(applicationInfo.applicationName, "OpenXR Vulkan-WGPU Shards App", XR_MAX_APPLICATION_NAME_SIZE);
    strncpy_s(applicationInfo.engineName, "OpenXR Vulkan-WGPU Shards", XR_MAX_ENGINE_NAME_SIZE);

    std::vector<const char*> extensions = { XR_KHR_VULKAN_ENABLE_EXTENSION_NAME };

    #ifdef DEBUG
    // Add the OpenXR debug instance extension
    extensions.push_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
    #endif

    // Check that all OpenXR instance extensions are supported
    for (const char* extension : extensions)
    {
      bool extensionSupported = false;
      for (const XrExtensionProperties& supportedExtension : supportedOpenXRInstanceExtensions)
      {
        if (strcmp(extension, supportedExtension.extensionName) == 0)
        {
          extensionSupported = true;
          break;
        }
      }

      if (!extensionSupported)
      {
        std::stringstream s;
        s << "OpenXR instance extension \"" << extension << "\"";
        util::error(Error::FeatureNotSupported, s.str());
        valid = false;
        return;
      }
    }

    XrInstanceCreateInfo instanceCreateInfo{ XR_TYPE_INSTANCE_CREATE_INFO }; 
    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instanceCreateInfo.enabledExtensionNames = extensions.data(); 
    instanceCreateInfo.applicationInfo = applicationInfo;
    const XrResult result = xrCreateInstance(&instanceCreateInfo, &xrInstance); 
    if (XR_FAILED(result))
    {
      util::error(Error::HeadsetNotConnected);
      valid = false;
      return;
    }
  }

  // Load the required OpenXR extension functions
  if (!util::loadXrExtensionFunction(xrInstance, "xrGetVulkanInstanceExtensionsKHR",
                                     reinterpret_cast<PFN_xrVoidFunction*>(&xrGetVulkanInstanceExtensionsKHR)))
  {
    util::error(Error::FeatureNotSupported, "OpenXR extension function \"xrGetVulkanInstanceExtensionsKHR\"");
    valid = false;
    return;
  }

  if (!util::loadXrExtensionFunction(xrInstance, "xrGetVulkanGraphicsDeviceKHR",
                                     reinterpret_cast<PFN_xrVoidFunction*>(&xrGetVulkanGraphicsDeviceKHR)))
  {
    util::error(Error::FeatureNotSupported, "OpenXR extension function \"xrGetVulkanGraphicsDeviceKHR\"");
    valid = false;
    return;
  }

  if (!util::loadXrExtensionFunction(xrInstance, "xrGetVulkanDeviceExtensionsKHR",
                                     reinterpret_cast<PFN_xrVoidFunction*>(&xrGetVulkanDeviceExtensionsKHR)))
  {
    util::error(Error::FeatureNotSupported, "OpenXR extension function \"xrGetVulkanDeviceExtensionsKHR\"");
    valid = false;
    return;
  }

  if (!util::loadXrExtensionFunction(xrInstance, "xrGetVulkanGraphicsRequirementsKHR",
                                     reinterpret_cast<PFN_xrVoidFunction*>(&xrGetVulkanGraphicsRequirementsKHR)))
  {
    util::error(Error::FeatureNotSupported, "OpenXR extension function \"xrGetVulkanGraphicsRequirementsKHR\"");
    valid = false;
    return;
  }

#ifdef DEBUG
  // Create an OpenXR debug utils messenger for validation
  {
    if (!util::loadXrExtensionFunction(xrInstance, "xrCreateDebugUtilsMessengerEXT",
                                       reinterpret_cast<PFN_xrVoidFunction*>(&xrCreateDebugUtilsMessengerEXT)))
    {
      util::error(Error::FeatureNotSupported, "OpenXR extension function \"xrCreateDebugUtilsMessengerEXT\"");
      valid = false;
      return;
    }

    if (!util::loadXrExtensionFunction(xrInstance, "xrDestroyDebugUtilsMessengerEXT",
                                       reinterpret_cast<PFN_xrVoidFunction*>(&xrDestroyDebugUtilsMessengerEXT)))
    {
      util::error(Error::FeatureNotSupported, "OpenXR extension function \"xrDestroyDebugUtilsMessengerEXT\"");
      valid = false;
      return;
    }

    constexpr XrDebugUtilsMessageTypeFlagsEXT typeFlags =
      XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT;

    constexpr XrDebugUtilsMessageSeverityFlagsEXT severityFlags =
      XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
      XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    const auto callback = [](XrDebugUtilsMessageSeverityFlagsEXT messageSeverity,
                             XrDebugUtilsMessageTypeFlagsEXT messageTypes,
                             const XrDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData) -> XrBool32
    {
      if (messageSeverity >= XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
      {
        std::cerr << "[OpenXR] " << callbackData->message << "\n";
      }

      return XR_FALSE; // Returning XR_TRUE will force the calling function to fail
    };

    XrDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo{ XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    debugUtilsMessengerCreateInfo.messageTypes = typeFlags;
    debugUtilsMessengerCreateInfo.messageSeverities = severityFlags;
    debugUtilsMessengerCreateInfo.userCallback = callback;
    const XrResult result =
      xrCreateDebugUtilsMessengerEXT(xrInstance, &debugUtilsMessengerCreateInfo, &xrDebugUtilsMessenger);
    if (XR_FAILED(result))
    {
      util::error(Error::GenericOpenXR);
      valid = false;
      return;
    }
  }
#endif


//[t] TODO: TODO:

  
  //[t] we don't need this
  /*
  // Get the required Vulkan instance extensions from GLFW
  std::vector<const char*> vulkanInstanceExtensions;
  {
    uint32_t requiredExtensionCount;
    const char** buffer = glfwGetRequiredInstanceExtensions(&requiredExtensionCount);
    if (!buffer)
    {
      util::error(Error::GenericGLFW);
      valid = false;
      return;
    }

    for (uint32_t i = 0u; i < requiredExtensionCount; ++i)
    {
      vulkanInstanceExtensions.push_back(buffer[i]);
    }
  }*/

 


  //[t] boilerplate for gfx vk instance, which we should (already) have in context_xr_gfx.hpp
  /*
  // Create a Vulkan instance with all required extensions
  {
    VkApplicationInfo applicationInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    applicationInfo.apiVersion = VK_API_VERSION_1_3;
    applicationInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    applicationInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    applicationInfo.pApplicationName = "OpenXR Vulkan Example";
    applicationInfo.pEngineName = "OpenXR Vulkan Example";

    VkInstanceCreateInfo instanceCreateInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    instanceCreateInfo.pApplicationInfo = &applicationInfo;
    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(vulkanInstanceExtensions.size());
    instanceCreateInfo.ppEnabledExtensionNames = vulkanInstanceExtensions.data();

    #ifdef DEBUG
    constexpr std::array layers = { "VK_LAYER_KHRONOS_validation" };

    // Get all supported Vulkan instance layers
    std::vector<VkLayerProperties> supportedInstanceLayers;
    uint32_t instanceLayerCount;
    if (vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr) != VK_SUCCESS)
    {
      util::error(Error::GenericVulkan);
      valid = false;
      return;
    }

    supportedInstanceLayers.resize(instanceLayerCount);
    if (vkEnumerateInstanceLayerProperties(&instanceLayerCount, supportedInstanceLayers.data()) != VK_SUCCESS)
    {
      util::error(Error::GenericVulkan);
      valid = false;
      return;
    }

    // Check that all Vulkan instance layers are supported
    for (const char* layer : layers)
    {
      bool layerSupported = false;
      for (const VkLayerProperties& supportedLayer : supportedInstanceLayers)
      {
        if (strcmp(layer, supportedLayer.layerName) == 0)
        {
          layerSupported = true;
          break;
        }
      }

      if (!layerSupported)
      {
        std::stringstream s;
        s << "Vulkan instance layer \"" << layer << "\"";
        util::error(Error::FeatureNotSupported, s.str());
        valid = false;
        return;
      }
    }

    instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    instanceCreateInfo.ppEnabledLayerNames = layers.data();
    #endif

    if (vkCreateInstance(&instanceCreateInfo, nullptr, &vkInstance) != VK_SUCCESS)
    {
      util::error(Error::GenericVulkan);
      valid = false;
      return;
    }
  }

  #ifdef DEBUG
  // Create a Vulkan debug utils messenger for validation
  {
    vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
      util::loadVkExtensionFunction(vkInstance, "vkCreateDebugUtilsMessengerEXT"));
    if (!vkCreateDebugUtilsMessengerEXT)
    {
      util::error(Error::FeatureNotSupported, "Vulkan extension function \"vkCreateDebugUtilsMessengerEXT\"");
      valid = false;
      return;
    }

    vkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
      util::loadVkExtensionFunction(vkInstance, "vkDestroyDebugUtilsMessengerEXT"));
    if (!vkDestroyDebugUtilsMessengerEXT)
    {
      util::error(Error::FeatureNotSupported, "Vulkan extension function \"vkDestroyDebugUtilsMessengerEXT\"");
      valid = false;
      return;
    }

    constexpr XrDebugUtilsMessageTypeFlagsEXT typeFlags = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                                          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                                          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    constexpr XrDebugUtilsMessageSeverityFlagsEXT severityFlags =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    const auto callback = [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                             VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                             const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData) -> VkBool32
    {
      if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
      {
        std::cerr << "[Vulkan] " << callbackData->pMessage << "\n";
      }

      return VK_FALSE; // Returning VK_TRUE will force the calling function to fail
    };

    VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo{
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT
    };
    debugUtilsMessengerCreateInfo.messageType = typeFlags;
    debugUtilsMessengerCreateInfo.messageSeverity = severityFlags;
    debugUtilsMessengerCreateInfo.pfnUserCallback = callback;
    if (vkCreateDebugUtilsMessengerEXT(vkInstance, &debugUtilsMessengerCreateInfo, nullptr, &vkDebugUtilsMessenger) !=
        VK_SUCCESS)
    {
      util::error(Error::GenericVulkan);
      valid = false;
      return;
    }
  }
  #endif
  */
}

bool Context_XR::checkXRDeviceReady(
                                    XrViewConfigurationType viewType,
                                    XrEnvironmentBlendMode environmentBlendMode,
                                    XrFormFactor xrFormFactor
                                  )
{
  // what we probably want:
  // XrViewConfigurationType viewType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  // XrEnvironmentBlendMode environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  // XrFormFactor xrFormFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

  viewType = viewType;

  // Get the system ID
  XrSystemGetInfo systemGetInfo{ XR_TYPE_SYSTEM_GET_INFO };
  systemGetInfo.formFactor = xrFormFactor;
  XrResult result = xrGetSystem(xrInstance, &systemGetInfo, &systemId);
  if (XR_FAILED(result))
  {
    util::error(Error::HeadsetNotConnected);
    //[t] TODO: Perhaps here instead of throwing error, do something else? Wait for headset to be plugged in?
    valid = false;
    return false;
  }

  // Check the supported environment blend modes
  {
    uint32_t environmentBlendModeCount;
    result = xrEnumerateEnvironmentBlendModes(xrInstance, systemId, viewType, 0u, &environmentBlendModeCount, nullptr);
    if (XR_FAILED(result))
    {
      util::error(Error::GenericOpenXR);
      valid = false;
      return false;
    }

    std::vector<XrEnvironmentBlendMode> supportedEnvironmentBlendModes(environmentBlendModeCount);
    result = xrEnumerateEnvironmentBlendModes(xrInstance, systemId, viewType, environmentBlendModeCount,
                                              &environmentBlendModeCount, supportedEnvironmentBlendModes.data());
    if (XR_FAILED(result))
    {
      util::error(Error::GenericOpenXR);
      valid = false;
      return false;
    }

    bool modeFound = false;
    for (const XrEnvironmentBlendMode& mode : supportedEnvironmentBlendModes)
    {
      if (mode == environmentBlendMode)
      {
        modeFound = true;
        break;
      }
    }

    if (!modeFound)
    {
      util::error(Error::FeatureNotSupported, "Environment blend mode");
      valid = false;
      return false;
    }
  }

  return true;
}

void Context_XR::getVulkanExtensionsFromOpenXRInstance()
{
  std::vector<const char*> vulkanInstanceExtensions;
  
  //[t] The folloing are extension checks for vulkan instance from system and from openxr
  // Get all supported Vulkan instance extensions
  std::vector<VkExtensionProperties> supportedVulkanInstanceExtensions;
  {
    uint32_t instanceExtensionCount;
    
    if (vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr) != VK_SUCCESS)
    {
      util::error(Error::GenericVulkan);
      valid = false;
      return;
    }

    supportedVulkanInstanceExtensions.resize(instanceExtensionCount);
    if (vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount,
                                               supportedVulkanInstanceExtensions.data()) != VK_SUCCESS)
    {
      util::error(Error::GenericVulkan);
      valid = false;
      return;
    }
  }

  
  // Get the required Vulkan instance extensions from OpenXR and add them
  {
    uint32_t count;
    XrResult result = xrGetVulkanInstanceExtensionsKHR(xrInstance, systemId, 0u, &count, nullptr);
    if (XR_FAILED(result))
    {
      util::error(Error::GenericOpenXR);
      valid = false;
      return;
    }

    std::string buffer;
    buffer.resize(count);
    result = xrGetVulkanInstanceExtensionsKHR(xrInstance, systemId, count, &count, buffer.data());
    if (XR_FAILED(result))
    {
      util::error(Error::GenericOpenXR);
      valid = false;
      return;
    }

    const std::vector<const char*> instanceExtensions = util::unpackExtensionString(buffer);
    for (const char* extension : instanceExtensions)
    {
      vulkanInstanceExtensions.push_back(extension);
    }
  }

  #ifdef DEBUG
  // Add the Vulkan debug instance extension
  vulkanInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  #endif

  // Check that all required Vulkan instance extensions are supported
  for (const char* extension : vulkanInstanceExtensions)
  {
    bool extensionSupported = false;

    for (const VkExtensionProperties& supportedExtension : supportedVulkanInstanceExtensions)
    {
      if (strcmp(extension, supportedExtension.extensionName) == 0)
      {
        extensionSupported = true;
        break;
      }
    }

    if (!extensionSupported)
    {
      std::stringstream s;
      s << "Vulkan instance extension \"" << extension << "\"";
      util::error(Error::FeatureNotSupported, s.str());
      valid = false;
      return;
    }
  }
}

Context_XR::~Context_XR()
{
  // Clean up OpenXR
#ifdef DEBUG
  xrDestroyDebugUtilsMessengerEXT(xrDebugUtilsMessenger);
#endif

  xrDestroyInstance(xrInstance);
//[t] this is boilerplate for gfx vk isntance
/*
  // Clean up Vulkan
  vkDestroyDevice(vkDevice, nullptr);

#ifdef DEBUG
  if (vkInstance)
  {
    vkDestroyDebugUtilsMessengerEXT(vkInstance, vkDebugUtilsMessenger, nullptr);
  }
#endif

  vkDestroyInstance(vkInstance, nullptr);
  */
}

//[t] we're not using this, we use the device data in WGPUVulkanShared instead. But it's good reference!
// especially for multiview
bool Context_XR::createDevice(VkSurfaceKHR mirrorSurface)//mirrorSurface to check compatibility
{
  // Retrieve the physical device from OpenXR
  XrResult result = xrGetVulkanGraphicsDeviceKHR(xrInstance, systemId, vkInstance, &physicalDevice);
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    return false;
  }

  // Pick the draw queue family index
  {
    // Retrieve the queue families
    std::vector<VkQueueFamilyProperties> queueFamilies;
    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    queueFamilies.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    bool drawQueueFamilyIndexFound = false;
    for (size_t queueFamilyIndexCandidate = 0u; queueFamilyIndexCandidate < queueFamilies.size();
         ++queueFamilyIndexCandidate)
    {
      const VkQueueFamilyProperties& queueFamilyCandidate = queueFamilies.at(queueFamilyIndexCandidate);

      // Check that the queue family includes actual queues
      if (queueFamilyCandidate.queueCount == 0u)
      {
        continue;
      }

      // Check the queue family for drawing support
      if (queueFamilyCandidate.queueFlags & VK_QUEUE_GRAPHICS_BIT)
      {
        drawQueueFamilyIndex = static_cast<uint32_t>(queueFamilyIndexCandidate);
        drawQueueFamilyIndexFound = true;
        break;
      }
    }

    if (!drawQueueFamilyIndexFound)
    {
      util::error(Error::FeatureNotSupported, "Graphics queue family index");
      return false;
    }
  }

  // Pick the present queue family index
  {
    // Retrieve the queue families
    std::vector<VkQueueFamilyProperties> queueFamilies;
    uint32_t queueFamilyCount = 0u;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    queueFamilies.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    bool presentQueueFamilyIndexFound = false;
    for (size_t queueFamilyIndexCandidate = 0u; queueFamilyIndexCandidate < queueFamilies.size();
         ++queueFamilyIndexCandidate)
    {
      const VkQueueFamilyProperties& queueFamilyCandidate = queueFamilies.at(queueFamilyIndexCandidate);

      // Check that the queue family includes actual queues
      if (queueFamilyCandidate.queueCount == 0u)
      {
        continue;
      }

      // Check the queue family for presenting support
      VkBool32 presentSupport = false;
      if (vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, static_cast<uint32_t>(queueFamilyIndexCandidate),
                                               mirrorSurface, &presentSupport) != VK_SUCCESS)
      {
        continue;
      }

      if (!presentQueueFamilyIndexFound && presentSupport)
      {
        presentQueueFamilyIndex = static_cast<uint32_t>(queueFamilyIndexCandidate);
        presentQueueFamilyIndexFound = true;
        break;
      }
    }

    if (!presentQueueFamilyIndexFound)
    {
      util::error(Error::FeatureNotSupported, "Present queue family index");
      return false;
    }
  }

  // Get all supported Vulkan device extensions
  std::vector<VkExtensionProperties> supportedVulkanDeviceExtensions;
  {
    uint32_t deviceExtensionCount;
    if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, nullptr) != VK_SUCCESS)
    {
      util::error(Error::GenericVulkan);
      return false;
    }

    supportedVulkanDeviceExtensions.resize(deviceExtensionCount);
    if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount,
                                             supportedVulkanDeviceExtensions.data()) != VK_SUCCESS)
    {
      util::error(Error::GenericVulkan);
      return false;
    }
  }

  // Get the required Vulkan device extensions from OpenXR
  std::vector<const char*> vulkanDeviceExtensions;
  {
    uint32_t count;
    result = xrGetVulkanDeviceExtensionsKHR(xrInstance, systemId, 0u, &count, nullptr);
    if (XR_FAILED(result))
    {
      util::error(Error::GenericOpenXR);
      return false;
    }

    std::string buffer;
    buffer.resize(count);
    result = xrGetVulkanDeviceExtensionsKHR(xrInstance, systemId, count, &count, buffer.data());
    if (XR_FAILED(result))
    {
      util::error(Error::GenericOpenXR);
      return false;
    }

    vulkanDeviceExtensions = util::unpackExtensionString(buffer);
  }

  // Add the required swapchain extension for mirror view
  vulkanDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

  // Check that all Vulkan device extensions are supported
  {
    for (const char* extension : vulkanDeviceExtensions)
    {
      bool extensionSupported = false;
      for (const VkExtensionProperties& supportedExtension : supportedVulkanDeviceExtensions)
      {
        if (strcmp(extension, supportedExtension.extensionName) == 0)
        {
          extensionSupported = true;
          break;
        }
      }

      if (!extensionSupported)
      {
        std::stringstream s;
        s << "Vulkan device extension \"" << extension << "\"";
        util::error(Error::FeatureNotSupported, s.str());
        return false;
      }
    }
  }

  // Create a vulkan physical device
  {
    // Verify that the required physical device features are supported
    VkPhysicalDeviceFeatures physicalDeviceFeatures;
    vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);
    if (!physicalDeviceFeatures.shaderStorageImageMultisample)//[t] TODO: Guus, need this for multiview?
    {
      util::error(Error::FeatureNotSupported, "Vulkan physical device feature \"shaderStorageImageMultisample\"");
      return false;
    }

    VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    VkPhysicalDeviceMultiviewFeatures physicalDeviceMultiviewFeatures{//[t] TODO: Guus, we'll need this for multiview
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES
    };
    physicalDeviceFeatures2.pNext = &physicalDeviceMultiviewFeatures;
    vkGetPhysicalDeviceFeatures2(physicalDevice, &physicalDeviceFeatures2);
    if (!physicalDeviceMultiviewFeatures.multiview)
    {
      util::error(Error::FeatureNotSupported, "Vulkan physical device feature \"multiview\"");
      return false;
    }

    physicalDeviceFeatures.shaderStorageImageMultisample = VK_TRUE; // Needed for some OpenXR implementations
    physicalDeviceMultiviewFeatures.multiview = VK_TRUE;            //[t] TODO: Guus, we also need this for stereo rendering

    constexpr float queuePriority = 1.0f;

    std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;

    VkDeviceQueueCreateInfo deviceQueueCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    deviceQueueCreateInfo.queueFamilyIndex = drawQueueFamilyIndex;
    deviceQueueCreateInfo.queueCount = 1u;
    deviceQueueCreateInfo.pQueuePriorities = &queuePriority;
    deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);

    if (drawQueueFamilyIndex != presentQueueFamilyIndex)
    {
      deviceQueueCreateInfo.queueFamilyIndex = presentQueueFamilyIndex;
      deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
    }

    VkDeviceCreateInfo deviceCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    deviceCreateInfo.pNext = &physicalDeviceMultiviewFeatures;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(vulkanDeviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = vulkanDeviceExtensions.data();
    deviceCreateInfo.pEnabledFeatures = &physicalDeviceFeatures;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(deviceQueueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfos.data();
    if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &vkDevice) != VK_SUCCESS)
    { 
      util::error(Error::GenericVulkan);
      return false;
    }
  }

  // Check the graphics requirements for Vulkan
  XrGraphicsRequirementsVulkanKHR graphicsRequirements{ XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR };
  result = xrGetVulkanGraphicsRequirementsKHR(xrInstance, systemId, &graphicsRequirements);
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    return false;
  }

  // Retrieve the queues
  vkGetDeviceQueue(vkDevice, drawQueueFamilyIndex, 0u, &drawQueue); 
  if (!drawQueue)
  {
    util::error(Error::GenericVulkan);
    return false;
  }

  vkGetDeviceQueue(vkDevice, presentQueueFamilyIndex, 0u, &presentQueue);
  if (!presentQueue)
  {
    util::error(Error::GenericVulkan);
    return false;
  }

  return true;
}

void Context_XR::sync() const
{
  vkDeviceWaitIdle(vkDevice);
}

bool Context_XR::isValid() const
{
  return valid;
}

XrViewConfigurationType Context_XR::getXrViewType() const
{
  return viewType;
}

XrInstance Context_XR::getXrInstance() const
{
  return xrInstance;
}

XrSystemId Context_XR::getXrSystemId() const
{
  return systemId;
}

VkInstance Context_XR::getVkInstance() const
{
  return vkInstance;
}

VkPhysicalDevice Context_XR::getVkPhysicalDevice() const
{
  return physicalDevice;
}

uint32_t Context_XR::getVkDrawQueueFamilyIndex() const
{
  return drawQueueFamilyIndex;
}

VkDevice Context_XR::getVkDevice() const
{
  return vkDevice;
}

VkQueue Context_XR::getVkDrawQueue() const
{
  return drawQueue;
}

VkQueue Context_XR::getVkPresentQueue() const
{
  return presentQueue;
}
