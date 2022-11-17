#pragma once

#include <glm/fwd.hpp>

#include <vulkan/vulkan.h>

#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <string>
#include <vector>

// All the things that can go wrong
enum class Error
{
  FeatureNotSupported,
  FileMissing,
  GenericGLFW,
  GenericOpenXR,
  GenericVulkan,
  HeadsetNotConnected,
  OutOfMemory,
  VulkanNotSupported,
  WindowFailure
};

namespace util
{
// Reports an error with optional details through a system-native message box
void error(Error error, const std::string& details = "");

// Loads an OpenXR extension function by 'name' into 'function', returns false on error
bool loadXrExtensionFunction(XrInstance instance, const std::string& name, PFN_xrVoidFunction* function);

// Loads a Vulkan extension function by 'name', returns nullptr on error
PFN_vkVoidFunction loadVkExtensionFunction(VkInstance instance, const std::string& name);

// Unpacks an extension list in a single string into a vector of c-style strings
std::vector<const char*> unpackExtensionString(const std::string& string);

// Loads a shader from 'file' into 'shaderModule', returns false on error
bool loadShaderFromFile(VkDevice device, const std::string& filename, VkShaderModule& shaderModule);

// Creates an identity pose
XrPosef makeIdentity();

// Converts a pose to a transformation matrix
glm::mat4 poseToMatrix(const XrPosef& pose);

// Creates a projection matrix
glm::mat4 createProjectionMatrix(XrFovf fov, float nearClip, float farClip);
} // namespace util