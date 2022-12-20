#include "Context_XR.h"

#include "Util.h"


#include <sstream>

#ifdef DEBUG_XR
  #include <array>
  #include <iostream>
#endif

#include "spdlog/spdlog.h"
                       
Context_XR::Context_XR(std::shared_ptr<gfx::WGPUVulkanShared> _wgpuUVulkanShared)
{
  spdlog::info("[log][t] Context_XR::Context_XR: Creating Context...");
  wgpuUVulkanShared = _wgpuUVulkanShared;

  // Get all supported OpenXR instance extensions
  std::vector<XrExtensionProperties> supportedOpenXRInstanceExtensions;
  {
    uint32_t instanceExtensionCount;
    XrResult result = xrEnumerateInstanceExtensionProperties(nullptr, 0u, &instanceExtensionCount, nullptr);
    if (XR_FAILED(result))
    {
      util::error(Error::GenericOpenXR);
      valid = false;
      spdlog::error("[log][t] Context_XR::Context_XR: error at xrEnumerateInstanceExtensionPropertiesxrEnumerateInstanceExtensionProperties(nullptr, 0u, &instanceExtensionCount, nullptr)");
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
      spdlog::error("[log][t] Context_XR::Context_XR: error at xrEnumerateInstanceExtensionProperties(nullptr, instanceExtensionCount, &instanceExtensionCount, supportedOpenXRInstanceExtensions.data())");
      return;
    }
    spdlog::info("[log][t] Context_XR::Context_XR: Context created. vaid: {}", valid);
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

    #ifdef DEBUG_XR
    // Add the OpenXR DEBUG_XR instance extension
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
        spdlog::error("[log][t] Context_XR::Context_XR: error at supportedExtension : supportedOpenXRInstanceExtensions");
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
      spdlog::error("[log][t] Context_XR::Context_XR: error at xrCreateInstance(&instanceCreateInfo, &xrInstance); ");
      return;
    }
  }

  // Load the required OpenXR extension functions
  if (!util::loadXrExtensionFunction(xrInstance, "xrGetVulkanInstanceExtensionsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&xrGetVulkanInstanceExtensionsKHR)))
  {
    util::error(Error::FeatureNotSupported, "OpenXR extension function \"xrGetVulkanInstanceExtensionsKHR\"");
    valid = false;
    spdlog::error("[log][t] Context_XR::Context_XR: error at loadXrExtensionFunction(xrInstance, 'xrGetVulkanInstanceExtensionsKHR', reinterpret_cast<PFN_xrVoidFunction*>(&xrGetVulkanInstanceExtensionsKHR)))");
    return;
  }

  if (!util::loadXrExtensionFunction(xrInstance, "xrGetVulkanGraphicsDeviceKHR", reinterpret_cast<PFN_xrVoidFunction*>(&xrGetVulkanGraphicsDeviceKHR)))
  {
    util::error(Error::FeatureNotSupported, "OpenXR extension function \"xrGetVulkanGraphicsDeviceKHR\"");
    valid = false;
    spdlog::error("[log][t] Context_XR::Context_XR: error at loadXrExtensionFunction(xrInstance, 'xrGetVulkanGraphicsDeviceKHR', reinterpret_cast<PFN_xrVoidFunction*>(&xrGetVulkanGraphicsDeviceKHR)))");
    return;
  }

  if (!util::loadXrExtensionFunction(xrInstance, "xrGetVulkanDeviceExtensionsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&xrGetVulkanDeviceExtensionsKHR)))
  {
    util::error(Error::FeatureNotSupported, "OpenXR extension function \"xrGetVulkanDeviceExtensionsKHR\"");
    valid = false;
    spdlog::error("[log][t] Context_XR::Context_XR: error at loadXrExtensionFunction(xrInstance, 'xrGetVulkanDeviceExtensionsKHR', reinterpret_cast<PFN_xrVoidFunction*>(&xrGetVulkanDeviceExtensionsKHR)))");
    return;
  }

  if (!util::loadXrExtensionFunction(xrInstance, "xrGetVulkanGraphicsRequirementsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&xrGetVulkanGraphicsRequirementsKHR)))
  {
    util::error(Error::FeatureNotSupported, "OpenXR extension function \"xrGetVulkanGraphicsRequirementsKHR\"");
    valid = false;
    spdlog::error("[log][t] Context_XR::Context_XR: error at loadXrExtensionFunction(xrInstance, 'xrGetVulkanGraphicsRequirementsKHR', reinterpret_cast<PFN_xrVoidFunction*>(&xrGetVulkanGraphicsRequirementsKHR)))");
    return;
  }

  #ifdef DEBUG_XR
  // Create an OpenXR DEBUG_XR utils messenger for validation
  {
    if (!util::loadXrExtensionFunction(xrInstance, "xrCreateDebugUtilsMessengerEXT", reinterpret_cast<PFN_xrVoidFunction*>(&xrCreateDebugUtilsMessengerEXT)))
    {
      util::error(Error::FeatureNotSupported, "OpenXR extension function \"xrCreateDebugUtilsMessengerEXT\"");
      valid = false;
      spdlog::error("[log][t] Context_XR::Context_XR: error at loadXrExtensionFunction(xrInstance, 'xrCreateDebugUtilsMessengerEXT', reinterpret_cast<PFN_xrVoidFunction*>(&xrCreateDebugUtilsMessengerEXT)))");
      return;
    }

    if (!util::loadXrExtensionFunction(xrInstance, "xrDestroyDebugUtilsMessengerEXT", reinterpret_cast<PFN_xrVoidFunction*>(&xrDestroyDebugUtilsMessengerEXT)))
    {
      util::error(Error::FeatureNotSupported, "OpenXR extension function \"xrDestroyDebugUtilsMessengerEXT\"");
      valid = false;
      spdlog::error("[log][t] Context_XR::Context_XR: error at loadXrExtensionFunction(xrInstance, 'xrDestroyDebugUtilsMessengerEXT', reinterpret_cast<PFN_xrVoidFunction*>(&xrDestroyDebugUtilsMessengerEXT)))");
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
      spdlog::error("[log][t] Context_XR::Context_XR: error at messageSeverity >= XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT");
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
      spdlog::error("[log][t] Context_XR::Context_XR: error at loadXrExtensionFunction(xrInstance, 'xrDestroyDebugUtilsMessengerEXT', reinterpret_cast<PFN_xrVoidFunction*>(&xrDestroyDebugUtilsMessengerEXT)))");
      return;
    }
  }
  #endif


  std::vector<const char*> vulkanInstanceExtensions;
  
  //[t] we don't need this
  /*
  // Get the required Vulkan instance extensions from GLFW
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

  /*
  //[t] boilerplate for gfx vk instance, which we should (already) have in context_xr_gfx.hpp
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

    #ifdef DEBUG_XR
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
    wgpuUVulkanShared->vkInstance = vkInstance;
  }
  */

  /*

  #ifdef DEBUG_XR
  // Create a Vulkan DEBUG_XR utils messenger for validation
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

  created = true;
}

//should be called after all the context devices created
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
    spdlog::error("[log][t] Context_XR::checkXRDeviceReady: error at xrGetSystem(xrInstance, &systemGetInfo, &systemId);");
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
      spdlog::error("[log][t] Context_XR::checkXRDeviceReady: error at xrEnumerateEnvironmentBlendModes(xrInstance, systemId, viewType, 0u, &environmentBlendModeCount, nullptr)");
      return false;
    }

    std::vector<XrEnvironmentBlendMode> supportedEnvironmentBlendModes(environmentBlendModeCount);
    result = xrEnumerateEnvironmentBlendModes(xrInstance, systemId, viewType, environmentBlendModeCount, &environmentBlendModeCount, supportedEnvironmentBlendModes.data());
    if (XR_FAILED(result))
    {
      util::error(Error::GenericOpenXR);
      valid = false;
      spdlog::error("[log][t] Context_XR::checkXRDeviceReady: error at xrEnumerateEnvironmentBlendModes(xrInstance, systemId, viewType, environmentBlendModeCount, &environmentBlendModeCount, supportedEnvironmentBlendModes.data());");
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
      spdlog::error("[log][t] Context_XR::checkXRDeviceReady: error at !modeFound in for (const XrEnvironmentBlendMode& mode : supportedEnvironmentBlendModes)");
      return false;
    }
  }

  return true;
}

void Context_XR::getVulkanExtensionsFromOpenXRInstance()
{
  spdlog::info("[log][t] Context_XR::getVulkanExtensionsFromOpenXRInstance()...");

  std::vector<const char*> vulkanInstanceExtensions;
  
  //[t] The folloing are extension checks for vulkan instance from system and from openxr
  // Get all supported Vulkan instance extensions
  //std::vector<VkExtensionProperties> supportedVulkanInstanceExtensions;
  std::vector<vk::ExtensionProperties> supportedVulkanInstanceExtensions;
  {
    uint32_t instanceExtensionCount;
    auto res = vk::enumerateInstanceExtensionProperties(nullptr, 
                                                        &instanceExtensionCount, 
                                                        nullptr, 
                                                        wgpuUVulkanShared->vkLoader);
    if ( res != vk::Result::eSuccess)
    {
      util::error(Error::GenericVulkan);
      valid = false;
      spdlog::error("[log][t] Context_XR::getVulkanExtensionsFromOpenXRInstance() Error at vk::enumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount,nullptr,wgpuUVulkanShared->vkLoader);");
      return;
    }

    supportedVulkanInstanceExtensions.resize(instanceExtensionCount);
    if (vk::enumerateInstanceExtensionProperties(nullptr, 
                                                &instanceExtensionCount,
                                                supportedVulkanInstanceExtensions.data(), 
                                                wgpuUVulkanShared->vkLoader) != vk::Result::eSuccess)
    {
      util::error(Error::GenericVulkan);
      valid = false;
      spdlog::error("[log][t] Context_XR::getVulkanExtensionsFromOpenXRInstance() Error at vk::enumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, supportedVulkanInstanceExtensions.data(), wgpuUVulkanShared->vkLoader)");
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
      spdlog::error("[log][t] Context_XR::getVulkanExtensionsFromOpenXRInstance() Error [{0}] at xrGetVulkanInstanceExtensionsKHR(xrInstance, systemId[{1}], 0u, &count[{2}], nullptr);", result, systemId,count);
      //return;
    }

    std::string buffer;
    buffer.resize(count);
    result = xrGetVulkanInstanceExtensionsKHR(xrInstance, systemId, count, &count, buffer.data());
    if (XR_FAILED(result))
    {
      util::error(Error::GenericOpenXR);
      valid = false;
      spdlog::error("[log][t] Context_XR::getVulkanExtensionsFromOpenXRInstance() Error [{0}] at 2nd xrGetVulkanInstanceExtensionsKHR(xrInstance, systemId[{1}], count[{2}], &count, buffer.data());", result, systemId,count);
      //return;
    }

    const std::vector<const char*> instanceExtensions = util::unpackExtensionString(buffer);
    for (const char* extension : instanceExtensions)
    {
      vulkanInstanceExtensions.push_back(extension);
    }
  }

  #ifdef DEBUG_XR
  // Add the Vulkan DEBUG_XR instance extension
  vulkanInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  #endif

  // Check that all required Vulkan instance extensions are supported
  for (const char* extension : vulkanInstanceExtensions)
  {
    bool extensionSupported = false;

    //for (const VkExtensionProperties& supportedExtension : supportedVulkanInstanceExtensions)
    for (const vk::ExtensionProperties& supportedExtension : supportedVulkanInstanceExtensions)
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
      spdlog::error("[log][t] Context_XR::getVulkanExtensionsFromOpenXRInstance() Error at supportedExtension : supportedVulkanInstanceExtensions");
      return;
    }
  }
  spdlog::info("[log][t] Context_XR::getVulkanExtensionsFromOpenXRInstance() End. valid == {0};",valid);
}

Context_XR::~Context_XR()
{
  spdlog::info("[log][t] Context_XR::~Context_XR();");

  // Clean up OpenXR
#ifdef DEBUG_XR
  xrDestroyDebugUtilsMessengerEXT(xrDebugUtilsMessenger);
#endif

  xrDestroyInstance(xrInstance);


  // Clean up Vulkan
  //vkDestroyDevice(vkDevice, nullptr);

#ifdef DEBUG_XR
  if (vkInstance)
  {
    vkDestroyDebugUtilsMessengerEXT(vkInstance, vkDebugUtilsMessenger, nullptr);
  }
#endif

  //vkDestroyInstance(vkInstance, nullptr);
  
}

bool Context_XR::CreatePhysicalDevice(){
  spdlog::info("[log][t] Context_XR::CreatePhysicalDevice()");
  // Retrieve the physical device from OpenXR
  XrResult result = xrGetVulkanGraphicsDeviceKHR(xrInstance, systemId, vkInstance, &physicalDevice); 
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    spdlog::error("[log][t] Context_XR::CreatePhysicalDevice: error [{0}] at xrGetVulkanGraphicsDeviceKHR(xrInstance, systemId, vkInstance, &physicalDevice);",result);
    return false;
  }

  wgpuUVulkanShared->vkPhysicalDevice = physicalDevice;

  spdlog::info("[log][t] Context_XR::CreatePhysicalDevice: end.");
  return true;
}

std::vector<const char*> Context_XR::GetVulkanXrExtensions(){
  spdlog::info("[log][t] Context_XR::GetVulkanXrExtensions()");
  // Get the required Vulkan device extensions from OpenXR
  std::vector<const char*> vulkanDeviceExtensions;
  {
    uint32_t count; 
    XrResult result = xrGetVulkanDeviceExtensionsKHR(xrInstance, systemId, 0u, &count, nullptr);
    if (XR_FAILED(result))
    {
      util::error(Error::GenericOpenXR);
      spdlog::error("[log][t] Context_XR::GetVulkanXrExtensions: error [{0}] at xrGetVulkanDeviceExtensionsKHR(xrInstance, systemId, 0u, &count, nullptr);", result);
      return vulkanDeviceExtensions;//false;
    }

    std::string buffer;
    buffer.resize(count);
    result = xrGetVulkanDeviceExtensionsKHR(xrInstance, systemId, count, &count, buffer.data());
    if (XR_FAILED(result))
    {
      util::error(Error::GenericOpenXR);
      spdlog::error("[log][t] Context_XR::GetVulkanXrExtensions: error at [{0}] xrGetVulkanDeviceExtensionsKHR(xrInstance, systemId, count, &count, buffer.data());", result);
      return vulkanDeviceExtensions;//false;
    }

    vulkanDeviceExtensions = util::unpackExtensionString(buffer);
  }

  // can't do this check because supportedVulkanDeviceExtensions is not vk:: compatible and prolly needs to be set up in context_xr_gfx.hpp anyway
  /*
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
        spdlog::error("[log][t] Context_XR::createDevice: error at (!extensionSupported)  vulkanDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);");
        return vulkanDeviceExtensions;
      }
    }
  }*/

  spdlog::info("[log][t] Context_XR::GetVulkanXrExtensions: end. Returning vulkanDeviceExtensions.");
  return vulkanDeviceExtensions;
}

bool Context_XR::CheckXrGraphicsRequirementsVulkanKHR(){
  // Check the graphics requirements for Vulkan
  XrGraphicsRequirementsVulkanKHR graphicsRequirements{ XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR };
  XrResult result = xrGetVulkanGraphicsRequirementsKHR(xrInstance, systemId, &graphicsRequirements);
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    spdlog::error("[log][t] Context_XR::CheckXrGraphicsRequirementsVulkanKHR: error [{}] at xrGetVulkanGraphicsRequirementsKHR(xrInstance, systemId, &graphicsRequirements);", result);
    return false;
  }
}

/*
//[t] this should be called after contextxr is created.
//[t] We are creating vkPhysicalDevice, vkDevice, vkDrawQueue, vkPresentQueue here.
//[t] Then sending them to context_xr_gfx in order to interface with our wgpu rendering engine.
//[t] This is adapted for both multiview (singlepass) and multipass
bool Context_XR::createDevice(bool isMultipass)//VkSurfaceKHR mirrorSurface)//mirrorSurface to check compatibility
{ 
  spdlog::info("[log][t] Context_XR::createDevice()");
  // Retrieve the physical device from OpenXR
  XrResult result = xrGetVulkanGraphicsDeviceKHR(xrInstance, systemId, vkInstance, &physicalDevice); 
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    spdlog::error("[log][t] Context_XR::createDevice: error at xrGetVulkanGraphicsDeviceKHR(xrInstance, systemId, vkInstance, &physicalDevice);");
    return false;
  }

  wgpuUVulkanShared->vkPhysicalDevice = physicalDevice;

  // Pick the draw queue family index
  { //TODO: SOMEHOW use void deviceCreated(WGPUDevice inDevice) { wgpuDeviceGetPropertiesEx
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
      spdlog::error("[log][t] Context_XR::createDevice: error at (!drawQueueFamilyIndexFound) Graphics queue family index.");
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

      
      //// Check the queue family for presenting support
      //VkBool32 presentSupport = false;
      //if (vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, static_cast<uint32_t>(queueFamilyIndexCandidate),
      //                                         mirrorSurface, &presentSupport) != VK_SUCCESS)
      //{
      //  continue;
      //}
//
      //if (!presentQueueFamilyIndexFound && presentSupport)
      //{
      //  presentQueueFamilyIndex = static_cast<uint32_t>(queueFamilyIndexCandidate);
      //  presentQueueFamilyIndexFound = true;
      //  break;
      //}
    }

    
    //if (!presentQueueFamilyIndexFound)
    //{
    //  util::error(Error::FeatureNotSupported, "Present queue family index");
    //  spdlog::error("[log][t] Context_XR::createDevice: error at (!presentQueueFamilyIndexFound) Present queue family index.");
    //  return false;
    //}
  }

  // Get all supported Vulkan device extensions
  std::vector<VkExtensionProperties> supportedVulkanDeviceExtensions;
  {
    uint32_t deviceExtensionCount;
    if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, nullptr) != VK_SUCCESS)
    {
      util::error(Error::GenericVulkan);
      spdlog::error("[log][t] Context_XR::createDevice: error at vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, nullptr) != VK_SUCCESS.");
      return false;
    }

    supportedVulkanDeviceExtensions.resize(deviceExtensionCount);
    if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, supportedVulkanDeviceExtensions.data()) != VK_SUCCESS)
    {
      util::error(Error::GenericVulkan);
      spdlog::error("[log][t] Context_XR::createDevice: error at vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, supportedVulkanDeviceExtensions.data()) != VK_SUCCESS");
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
      spdlog::error("[log][t] Context_XR::createDevice: error at xrGetVulkanDeviceExtensionsKHR(xrInstance, systemId, 0u, &count, nullptr);");
      return false;
    }

    std::string buffer;
    buffer.resize(count);
    result = xrGetVulkanDeviceExtensionsKHR(xrInstance, systemId, count, &count, buffer.data());
    if (XR_FAILED(result))
    {
      util::error(Error::GenericOpenXR);
      spdlog::error("[log][t] Context_XR::createDevice: error at xrGetVulkanDeviceExtensionsKHR(xrInstance, systemId, count, &count, buffer.data());");
      return false;
    }

    vulkanDeviceExtensions = util::unpackExtensionString(buffer);
  }

  // Add the required swapchain extension for mirror view
  //vulkanDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

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
        spdlog::error("[log][t] Context_XR::createDevice: error at (!extensionSupported)  vulkanDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);");
        return false;
      }
    }
  }

  // Create a vulkan physical device
  {

    //TODO: Guus: so do we not need any physical device features at all for multipass?
    VkPhysicalDeviceMultiviewFeatures physicalDeviceMultiviewFeatures{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES
    };
    // Verify that the required physical device features are supported
    VkPhysicalDeviceFeatures physicalDeviceFeatures;
    if(!isMultipass){
      //vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);
      vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);
      if (!physicalDeviceFeatures.shaderStorageImageMultisample)
      {
        util::error(Error::FeatureNotSupported, "Vulkan physical device feature \"shaderStorageImageMultisample\"");
        spdlog::error("[log][t] Context_XR::createDevice: error at Vulkan physical device feature \"shaderStorageImageMultisample\"");
        return false;
      }

      VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
      
      //VkPhysicalDeviceMultiviewFeatures physicalDeviceMultiviewFeatures{
      //  VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES
      //};
      physicalDeviceFeatures2.pNext = &physicalDeviceMultiviewFeatures;
      vkGetPhysicalDeviceFeatures2(physicalDevice, &physicalDeviceFeatures2);
      if (!physicalDeviceMultiviewFeatures.multiview)
      {
        util::error(Error::FeatureNotSupported, "Vulkan physical device feature \"multiview\"");
        spdlog::error("[log][t] Context_XR::createDevice: error at Vulkan physical device feature \"multiview\"");
        return false;
      }

      physicalDeviceFeatures.shaderStorageImageMultisample = VK_TRUE; // Needed for some OpenXR implementations
      physicalDeviceMultiviewFeatures.multiview = VK_TRUE;            
    }
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
    if(!isMultipass){
      deviceCreateInfo.pNext = &physicalDeviceMultiviewFeatures;
    }
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(vulkanDeviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = vulkanDeviceExtensions.data();
    deviceCreateInfo.pEnabledFeatures = &physicalDeviceFeatures;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(deviceQueueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfos.data();
    if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &vkDevice) != VK_SUCCESS)
    { 
      util::error(Error::GenericVulkan);
      spdlog::error("[log][t] Context_XR::createDevice: error at vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &vkDevice) != VK_SUCCESS");
      return false;
    }
  }

  // Check the graphics requirements for Vulkan
  XrGraphicsRequirementsVulkanKHR graphicsRequirements{ XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR };
  result = xrGetVulkanGraphicsRequirementsKHR(xrInstance, systemId, &graphicsRequirements);
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    spdlog::error("[log][t] Context_XR::createDevice: error at xrGetVulkanGraphicsRequirementsKHR(xrInstance, systemId, &graphicsRequirements);");
    return false;
  }

  // Retrieve the queues
  vkGetDeviceQueue(vkDevice, drawQueueFamilyIndex, 0u, &drawQueue); 
  if (!drawQueue)
  {
    util::error(Error::GenericVulkan);
    spdlog::error("[log][t] Context_XR::createDevice: error at vkGetDeviceQueue(vkDevice, drawQueueFamilyIndex, 0u, &drawQueue); ");
    return false;
  }

  vkGetDeviceQueue(vkDevice, presentQueueFamilyIndex, 0u, &presentQueue);
  if (!presentQueue)
  {
    util::error(Error::GenericVulkan);
    spdlog::error("[log][t] Context_XR::createDevice: error at vkGetDeviceQueue(vkDevice, presentQueueFamilyIndex, 0u, &presentQueue); ");
    return false;
  }

  spdlog::error("[log][t] Context_XR::createDevice: end; ");
  return true;
}*/

void Context_XR::sync() const
{
  spdlog::info("[log][t] Context_XR::sync()");
  //vkDeviceWaitIdle(vkDevice);
  //wgpuUVulkanShared->vkDevice.waitIdle(wgpuUVulkanShared->vkDevice, wgpuUVulkanShared->vkLoader);
  wgpuUVulkanShared->vkDevice.waitIdle(wgpuUVulkanShared->vkLoader);
}

bool Context_XR::isValid() const
{
  spdlog::info("[log][t] Context_XR::isValid() valid[{0}] created[{1}]", valid, created);
  return valid && created;
}

XrViewConfigurationType Context_XR::getXrViewType() const
{
  spdlog::info("[log][t] Context_XR::getXrViewType()");
  return viewType;
}

XrInstance Context_XR::getXrInstance() const
{
  spdlog::info("[log][t] Context_XR::getXrInstance()");
  return xrInstance;
}

XrSystemId Context_XR::getXrSystemId() const
{
  spdlog::info("[log][t] Context_XR::getXrSystemId()");
  return systemId;
}

VkInstance Context_XR::getVkInstance() const
{
  spdlog::info("[log][t] Context_XR::getVkInstance()");
  return vkInstance;
}

VkPhysicalDevice Context_XR::getVkPhysicalDevice() const
{
  spdlog::info("[log][t] Context_XR::getVkPhysicalDevice()");
  return physicalDevice;
}

uint32_t Context_XR::getVkDrawQueueFamilyIndex() const
{
  spdlog::info("[t] Context_XR::getVkDrawQueueFamilyIndex()");
  return drawQueueFamilyIndex;
}

VkDevice Context_XR::getVkDevice() const
{
  spdlog::info("[log][t] Context_XR::getVkDevice()");
  return vkDevice;
}

VkQueue Context_XR::getVkDrawQueue() const
{
  spdlog::info("[log][t] Context_XR::getVkDrawQueue()");
  return drawQueue;
}

VkQueue Context_XR::getVkPresentQueue() const
{
  spdlog::info("[log][t] Context_XR::getVkPresentQueue()");
  return presentQueue;
}
