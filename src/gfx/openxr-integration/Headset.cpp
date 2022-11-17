#include "Headset.h"

#include "Context_XR.h"
#include "context.hpp"
#include "context_xr_gfx.hpp"
#include "RenderTarget.h"
#include "Util.h"

#include <array>
#include <sstream>

/*
Based on https://github.com/janhsimon/openxr-vulkan-example

MIT License

Copyright (c) 2022 Jan Simon

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

namespace
{
constexpr XrReferenceSpaceType spaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
constexpr VkFormat colorFormat = VK_FORMAT_R8G8B8A8_SRGB;
constexpr VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
} // namespace

Headset::Headset(const Context_XR* xrContext, gfx::Context* gfxContext) : context(context)
{
  gfxWgpuVulkanContext = gfxContext->getContextXrGfxBackend();
  gfxWgpuVulkanShared = gfxWgpuVulkanContext->getWGPUVulkanShared();
  const VkDevice device = gfxWgpuVulkanShared->device;

  // Create a (one) render pass
  {
    constexpr uint32_t viewMask = 0b00000011;
    constexpr uint32_t correlationMask = 0b00000011;

    VkRenderPassMultiviewCreateInfo renderPassMultiviewCreateInfo{
      VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO
    };
    renderPassMultiviewCreateInfo.subpassCount = 1u;
    renderPassMultiviewCreateInfo.pViewMasks = &viewMask;
    renderPassMultiviewCreateInfo.correlationMaskCount = 1u;
    renderPassMultiviewCreateInfo.pCorrelationMasks = &correlationMask;

    VkAttachmentDescription colorAttachmentDescription{};
    colorAttachmentDescription.format = colorFormat;
    colorAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentReference;
    colorAttachmentReference.attachment = 0u;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachmentDescription{};
    depthAttachmentDescription.format = depthFormat;
    depthAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentReference;
    depthAttachmentReference.attachment = 1u;
    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription{};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1u;
    subpassDescription.pColorAttachments = &colorAttachmentReference;
    subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;

    const std::array attachments = { colorAttachmentDescription, depthAttachmentDescription };

    VkRenderPassCreateInfo renderPassCreateInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    renderPassCreateInfo.pNext = &renderPassMultiviewCreateInfo;
    renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassCreateInfo.pAttachments = attachments.data();
    renderPassCreateInfo.subpassCount = 1u;
    renderPassCreateInfo.pSubpasses = &subpassDescription;
    if (vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass) != VK_SUCCESS) 
    {
      util::error(Error::GenericVulkan);
      valid = false;
      return;
    }
  }

  // vukan context for openxr
  const XrInstance xrInstance = xrContext->getXrInstance(); 
  const XrSystemId xrSystemId = xrContext->getXrSystemId();
  const VkPhysicalDevice vkPhysicalDevice = gfxWgpuVulkanShared->physicalDevice;
  const uint32_t vkDrawQueueFamilyIndex = gfxWgpuVulkanShared->queue;

  // Create a session with Vulkan graphics binding
  XrGraphicsBindingVulkanKHR graphicsBinding{ XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR };
  graphicsBinding.device = device;
  graphicsBinding.instance = gfxWgpuVulkanShared->instance;
  graphicsBinding.physicalDevice = vkPhysicalDevice;
  graphicsBinding.queueFamilyIndex = vkDrawQueueFamilyIndex;
  graphicsBinding.queueIndex = 0u;

  XrSessionCreateInfo sessionCreateInfo{ XR_TYPE_SESSION_CREATE_INFO };
  sessionCreateInfo.next = &graphicsBinding;
  sessionCreateInfo.systemId = xrSystemId;
  XrResult result = xrCreateSession(xrInstance, &sessionCreateInfo, &session);
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    valid = false;
    return;
  }

  // Create a play space
  XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
  referenceSpaceCreateInfo.referenceSpaceType = spaceType;
  referenceSpaceCreateInfo.poseInReferenceSpace = util::makeIdentity();
  result = xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &space);
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    valid = false;
    return;
  }

  const XrViewConfigurationType viewType = xrContext->getXrViewType();

  // Get the number of eyes
  result = xrEnumerateViewConfigurationViews(xrInstance, xrSystemId, viewType, 0u,
                                             reinterpret_cast<uint32_t*>(&eyeCount), nullptr);
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    valid = false;
    return;
  }

  // Get the eye image info per eye
  eyeImageInfos.resize(eyeCount);
  for (XrViewConfigurationView& eyeInfo : eyeImageInfos)
  {
    eyeInfo.type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
    eyeInfo.next = nullptr;
  }

  result =
    xrEnumerateViewConfigurationViews(xrInstance, xrSystemId, viewType, static_cast<uint32_t>(eyeImageInfos.size()),
                                      reinterpret_cast<uint32_t*>(&eyeCount), eyeImageInfos.data());
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    valid = false;
    return;
  }

  // Allocate the eye poses
  eyePoses.resize(eyeCount);
  for (XrView& eyePose : eyePoses)
  {
    eyePose.type = XR_TYPE_VIEW;
    eyePose.next = nullptr;
  }

  // Verify that the desired color format is supported
  {
    uint32_t formatCount = 0u;
    result = xrEnumerateSwapchainFormats(session, 0u, &formatCount, nullptr);
    if (XR_FAILED(result))
    {
      util::error(Error::GenericOpenXR);
      valid = false;
      return;
    }

    std::vector<int64_t> formats(formatCount);
    result = xrEnumerateSwapchainFormats(session, formatCount, &formatCount, formats.data());
    if (XR_FAILED(result))
    {
      util::error(Error::GenericOpenXR);
      valid = false;
      return;
    }

    bool formatFound = false;
    for (const int64_t& format : formats)
    {
      if (format == static_cast<int64_t>(colorFormat)) 
      {
        formatFound = true;
        break;
      }
    }

    if (!formatFound)
    {
      util::error(Error::FeatureNotSupported, "OpenXR swapchain color format");
      valid = false;
      return;
    }
  }

  // [t] vk from xr instance eye size
  const VkExtent2D eyeResolution = getEyeResolution(0u);

  // [t] let's try without a depth buffer
  /*
  // Create a depth buffer
  {
    // Create an image
    VkImageCreateInfo imageCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.extent.width = eyeResolution.width;
    imageCreateInfo.extent.height = eyeResolution.height;
    imageCreateInfo.extent.depth = 1u;
    imageCreateInfo.mipLevels = 1u;
    imageCreateInfo.arrayLayers = 2u;
    imageCreateInfo.format = depthFormat;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(device, &imageCreateInfo, nullptr, &depthImage) != VK_SUCCESS)
    {
      util::error(Error::GenericVulkan);
      valid = false;
      return;
    }

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(device, depthImage, &memoryRequirements);

    VkPhysicalDeviceMemoryProperties supportedMemoryProperties;
    vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice, &supportedMemoryProperties);

    const VkMemoryPropertyFlags memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    const VkMemoryPropertyFlags typeFilter = memoryRequirements.memoryTypeBits;
    uint32_t memoryTypeIndex = 0u;
    bool memoryTypeFound = false;
    for (uint32_t i = 0u; i < supportedMemoryProperties.memoryTypeCount; ++i)
    {
      const VkMemoryPropertyFlags propertyFlags = supportedMemoryProperties.memoryTypes[i].propertyFlags;
      if (typeFilter & (1 << i) && (propertyFlags & memoryProperties) == memoryProperties)
      {
        memoryTypeIndex = i;
        memoryTypeFound = true;
        break;
      }
    }

    if (!memoryTypeFound)
    {
      util::error(Error::FeatureNotSupported, "Suitable depth buffer memory type");
      valid = false;
      return;
    }

    VkMemoryAllocateInfo memoryAllocateInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = memoryTypeIndex;
    if (vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &depthMemory) != VK_SUCCESS)
    {
      std::stringstream s;
      s << memoryRequirements.size << " bytes for depth buffer";
      util::error(Error::OutOfMemory, s.str());
      valid = false;
      return;
    }

    if (vkBindImageMemory(device, depthImage, depthMemory, 0) != VK_SUCCESS)
    {
      util::error(Error::GenericVulkan);
      valid = false;
      return;
    }

    // Create an image view
    VkImageViewCreateInfo imageViewCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    imageViewCreateInfo.image = depthImage;
    imageViewCreateInfo.format = depthFormat;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    imageViewCreateInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                       VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
    imageViewCreateInfo.subresourceRange.layerCount = 2u;
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0u;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0u;
    imageViewCreateInfo.subresourceRange.levelCount = 1u;
    if (vkCreateImageView(device, &imageViewCreateInfo, nullptr, &depthImageView) != VK_SUCCESS)
    {
      util::error(Error::GenericVulkan);
      valid = false;
      return;
    }
  }*/


  // Create an xr swapchain and render targets
  { 
    const XrViewConfigurationView& eyeImageInfo = eyeImageInfos.at(0u);

    // Create a swapchain
    XrSwapchainCreateInfo swapchainCreateInfo{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
    swapchainCreateInfo.format = colorFormat;
    swapchainCreateInfo.sampleCount = eyeImageInfo.recommendedSwapchainSampleCount;
    swapchainCreateInfo.width = eyeImageInfo.recommendedImageRectWidth;
    swapchainCreateInfo.height = eyeImageInfo.recommendedImageRectHeight;
    swapchainCreateInfo.arraySize = static_cast<uint32_t>(eyeCount);
    swapchainCreateInfo.faceCount = 1u;
    swapchainCreateInfo.mipCount = 1u;

    result = xrCreateSwapchain(session, &swapchainCreateInfo, &swapchain); // [t] xr swapchain
    if (XR_FAILED(result))
    { 
      util::error(Error::GenericOpenXR);
      valid = false;
      return;
    }

    // Get the number of swapchain images -- there is one xr swapchain, one image per eye (and should have multiview)
    uint32_t swapchainImageCount; 
    result = xrEnumerateSwapchainImages(swapchain, 0u, &swapchainImageCount, nullptr);
    if (XR_FAILED(result))
    {
      util::error(Error::GenericOpenXR);
      valid = false;
      return;
    }

    //[t] xr vk

    // Retrieve the swapchain image
    std::vector<XrSwapchainImageVulkanKHR> swapchainImages;
    swapchainImages.resize(swapchainImageCount);
    for (XrSwapchainImageVulkanKHR& swapchainImage : swapchainImages) // [t] vk swapchain image
    {
      swapchainImage.type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
    }

    //[t] TODO: to verify: So at this point, I believe the vk swapchain image should be the one set up for the WGPUVukanShared vkInstance

    //[t] xr swapchain

    XrSwapchainImageBaseHeader* data = reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImages.data());
    result =
      xrEnumerateSwapchainImages(swapchain, static_cast<uint32_t>(swapchainImages.size()), &swapchainImageCount, data);
    if (XR_FAILED(result))
    {
      util::error(Error::GenericOpenXR);
      valid = false;
      return;
    }

    //[t] create the xr vk render target(s)
    swapchainRenderTargets.resize(swapchainImages.size());
    for (size_t renderTargetIndex = 0u; renderTargetIndex < swapchainRenderTargets.size(); ++renderTargetIndex)
    {
      RenderTarget*& renderTarget = swapchainRenderTargets.at(renderTargetIndex);
      //[t] image should be coming from data of WGPUVukanShared if all is done right
      const VkImage image = swapchainImages.at(renderTargetIndex).image;
      //[t] 2 rendertargets. One for each swapchainImage. Using the same renderpass.
      //[t] depthImageView can be null 
      renderTarget = new RenderTarget(device, image, depthImageView, eyeResolution, colorFormat, renderPass, 2u);
      if (!renderTarget->isValid()) 
      {
        valid = false;
        return;
      }
      //[t] TODO: Guus: Ok so far so good🤞, so how/where do we use the RenderTarget in your existing system? 🙂
    }
  }

  // Create the eye render infos
  eyeRenderInfos.resize(eyeCount);
  for (size_t eyeIndex = 0u; eyeIndex < eyeRenderInfos.size(); ++eyeIndex)
  {
    XrCompositionLayerProjectionView& eyeRenderInfo = eyeRenderInfos.at(eyeIndex);
    eyeRenderInfo.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
    eyeRenderInfo.next = nullptr;

    // Associate this eye with the swapchain
    const XrViewConfigurationView& eyeImageInfo = eyeImageInfos.at(eyeIndex);
    eyeRenderInfo.subImage.swapchain = swapchain;
    eyeRenderInfo.subImage.imageArrayIndex = static_cast<uint32_t>(eyeIndex);
    eyeRenderInfo.subImage.imageRect.offset = { 0, 0 };
    eyeRenderInfo.subImage.imageRect.extent = { static_cast<int32_t>(eyeImageInfo.recommendedImageRectWidth),
                                                static_cast<int32_t>(eyeImageInfo.recommendedImageRectHeight) };
  }

  // Allocate view and projection matrices
  eyeViewMatrices.resize(eyeCount);
  eyeProjectionMatrices.resize(eyeCount);
}

Headset::~Headset()
{
  // Clean up OpenXR
  xrEndSession(session);
  xrDestroySwapchain(swapchain);

  for (const RenderTarget* renderTarget : swapchainRenderTargets)
  {
    delete renderTarget;
  }

  xrDestroySpace(space);
  xrDestroySession(session);

  // Clean up Vulkan
  const VkDevice vkDevice = gfxWgpuVulkanShared->device);
  vkDestroyImageView(vkDevice, depthImageView, nullptr);
  vkFreeMemory(vkDevice, depthMemory, nullptr);
  vkDestroyImage(vkDevice, depthImage, nullptr);
  vkDestroyRenderPass(vkDevice, renderPass, nullptr);
}

Headset::BeginFrameResult Headset::beginFrame(uint32_t& swapchainImageIndex)
                                              //[t] it's rendered per swapchain (but we should have one swapchain), 
                                              //the swapchain should have one image: a layered multiview image. 
                                              //OpenXR is set up with this swapchain image and internally assigns an index to it.
                                              //And provides data for 2 eyes (matrixes etc).
{ 
  const XrInstance instance = xrContext->instance;

  // Poll OpenXR events
  XrEventDataBuffer buffer;
  buffer.type = XR_TYPE_EVENT_DATA_BUFFER;
  while (xrPollEvent(instance, &buffer) == XR_SUCCESS)
  {
    switch (buffer.type)
    {
    case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
      exitRequested = true;
      return BeginFrameResult::SkipFully;
    case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
    {
      XrEventDataSessionStateChanged* event = reinterpret_cast<XrEventDataSessionStateChanged*>(&buffer);
      sessionState = event->state;

      if (event->state == XR_SESSION_STATE_READY)
      {
        if (!beginSession())
        {
          return BeginFrameResult::Error;
        }
      }
      else if (event->state == XR_SESSION_STATE_STOPPING)
      {
        if (!endSession())
        {
          return BeginFrameResult::Error;
        }
      }
      else if (event->state == XR_SESSION_STATE_LOSS_PENDING || event->state == XR_SESSION_STATE_EXITING)
      {
        exitRequested = true;
        return BeginFrameResult::SkipFully;
      }

      break;
    }
    }
  }

  if (sessionState != XR_SESSION_STATE_READY && sessionState != XR_SESSION_STATE_SYNCHRONIZED &&
      sessionState != XR_SESSION_STATE_VISIBLE && sessionState != XR_SESSION_STATE_FOCUSED)
  {
    // If we are not ready, synchronized, visible or focused, we skip all processing of this frame
    // This means no waiting, no beginning or ending of the frame at all
    return BeginFrameResult::SkipFully;
  }

  // Wait for the new frame
  frameState.type = XR_TYPE_FRAME_STATE;
  XrFrameWaitInfo frameWaitInfo{ XR_TYPE_FRAME_WAIT_INFO };
  XrResult result = xrWaitFrame(session, &frameWaitInfo, &frameState);
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    return BeginFrameResult::Error;
  }

  // Begin the new frame
  XrFrameBeginInfo frameBeginInfo{ XR_TYPE_FRAME_BEGIN_INFO };
  result = xrBeginFrame(session, &frameBeginInfo);
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    return BeginFrameResult::Error;
  }

  if (!frameState.shouldRender)
  {
    // Let the host know that we don't want to render this frame
    // We do still need to end the frame however
    return BeginFrameResult::SkipRender;
  }

  // Update the eye poses
  viewState.type = XR_TYPE_VIEW_STATE;
  uint32_t viewCount;
  XrViewLocateInfo viewLocateInfo{ XR_TYPE_VIEW_LOCATE_INFO };
  viewLocateInfo.viewConfigurationType = xrContext->getXrViewType();
  viewLocateInfo.displayTime = frameState.predictedDisplayTime;
  viewLocateInfo.space = space;
  result = xrLocateViews(session, &viewLocateInfo, &viewState, static_cast<uint32_t>(eyePoses.size()), &viewCount,
                         eyePoses.data());
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    return BeginFrameResult::Error;
  }

  if (viewCount != eyeCount)
  {
    util::error(Error::GenericOpenXR);
    return BeginFrameResult::Error;
  }

  // Update the eye render infos, view and projection matrices
  for (size_t eyeIndex = 0u; eyeIndex < eyeCount; ++eyeIndex)
  {
    // Copy the eye poses into the eye render infos
    XrCompositionLayerProjectionView& eyeRenderInfo = eyeRenderInfos.at(eyeIndex);
    const XrView& eyePose = eyePoses.at(eyeIndex);
    eyeRenderInfo.pose = eyePose.pose;
    eyeRenderInfo.fov = eyePose.fov;

    // Update the view and projection matrices
    const XrPosef& pose = eyeRenderInfo.pose;
    eyeViewMatrices.at(eyeIndex) = util::poseToMatrix(pose);
    eyeProjectionMatrices.at(eyeIndex) = util::createProjectionMatrix(eyeRenderInfo.fov, 0.1f, 250.0f);
  }

  // Acquire the swapchain image
  XrSwapchainImageAcquireInfo swapchainImageAcquireInfo{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
  result = xrAcquireSwapchainImage(swapchain, &swapchainImageAcquireInfo, &swapchainImageIndex); 
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    return BeginFrameResult::Error;
  }

  // Wait for the swapchain image
  XrSwapchainImageWaitInfo swapchainImageWaitInfo{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
  swapchainImageWaitInfo.timeout = XR_INFINITE_DURATION;
  result = xrWaitSwapchainImage(swapchain, &swapchainImageWaitInfo);
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    return BeginFrameResult::Error;
  }

  return BeginFrameResult::RenderFully; // Request full rendering of the frame
}

void Headset::endFrame() const
{
  // Release the swapchain image
  XrSwapchainImageReleaseInfo swapchainImageReleaseInfo{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
  XrResult result = xrReleaseSwapchainImage(swapchain, &swapchainImageReleaseInfo);
  if (XR_FAILED(result))
  {
    return;
  }

  // End the frame
  XrCompositionLayerProjection compositionLayerProjection{ XR_TYPE_COMPOSITION_LAYER_PROJECTION };
  compositionLayerProjection.space = space;
  compositionLayerProjection.viewCount = static_cast<uint32_t>(eyeRenderInfos.size());
  compositionLayerProjection.views = eyeRenderInfos.data();

  std::vector<XrCompositionLayerBaseHeader*> layers;

  const bool positionValid = viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT;
  const bool orientationValid = viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT;
  if (frameState.shouldRender && positionValid && orientationValid)
  {
    layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&compositionLayerProjection));
  }

  XrFrameEndInfo frameEndInfo{ XR_TYPE_FRAME_END_INFO };
  frameEndInfo.displayTime = frameState.predictedDisplayTime;
  frameEndInfo.layerCount = static_cast<uint32_t>(layers.size());
  frameEndInfo.layers = layers.data();
  frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  result = xrEndFrame(session, &frameEndInfo);
  if (XR_FAILED(result))
  {
    return;
  }
}

bool Headset::isValid() const
{
  return valid;
}

bool Headset::isExitRequested() const
{
  return exitRequested;
}

VkRenderPass Headset::getRenderPass() const
{
  return renderPass;
}

size_t Headset::getEyeCount() const
{
  return eyeCount;
}

VkExtent2D Headset::getEyeResolution(size_t eyeIndex) const
{
  const XrViewConfigurationView& eyeInfo = eyeImageInfos.at(eyeIndex);
  return { eyeInfo.recommendedImageRectWidth, eyeInfo.recommendedImageRectHeight };
}

glm::mat4 Headset::getEyeViewMatrix(size_t eyeIndex) const
{
  return eyeViewMatrices.at(eyeIndex);
}

glm::mat4 Headset::getEyeProjectionMatrix(size_t eyeIndex) const
{
  return eyeProjectionMatrices.at(eyeIndex);
}

RenderTarget* Headset::getRenderTarget(size_t swapchainImageIndex) const
{
  return swapchainRenderTargets.at(swapchainImageIndex);
}

bool Headset::beginSession() const
{
  // Start the session
  XrSessionBeginInfo sessionBeginInfo{ XR_TYPE_SESSION_BEGIN_INFO };
  sessionBeginInfo.primaryViewConfigurationType = xrContext->getXrViewType();
  const XrResult result = xrBeginSession(session, &sessionBeginInfo);
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    return false;
  }

  return true;
}

bool Headset::endSession() const
{
  // End the session
  const XrResult result = xrEndSession(session);
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    return false;
  }

  return true;
}