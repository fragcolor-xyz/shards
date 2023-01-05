#include "Headset.h"



#include "Util.h"

#include <array>
#include <sstream>

#include <glm/include/glm/mat4x4.hpp>
#include "spdlog/spdlog.h"


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
constexpr WGPUTextureFormat colorFormat_wgpu = WGPUTextureFormat_RGBA8UnormSrgb;//[t] TODO: is this actually equiv to the above vkformat??
constexpr VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
} // namespace

Headset::Headset(std::shared_ptr<Context_XR> _xrContext, std::shared_ptr<gfx::WGPUVulkanShared> _gfxWgpuVulkanShared, bool isMultipass)
{
  spdlog::info("[log][t] Headset::Headset(xrContext, gfxWgpuVulkanShared)...");
  xrContext = _xrContext;
  if (!xrContext->isValid())
  {
    spdlog::error("[log][t] OpenXRSystem::InitOpenXR: error at xrContext.");
  }
  if (!_xrContext->isValid())
  {
    spdlog::error("[log][t] OpenXRSystem::InitOpenXR: error at _xrContext.");
  }
  this->gfxWgpuVulkanShared = _gfxWgpuVulkanShared;
  const VkDevice device = gfxWgpuVulkanShared->vkDevice;


  //[t] TODO: we really should use this code for multiview (single pass). But this is vulkan-core, OpenXR uses vlkan-core. But we use something called vulkan-hpp which is different; I hate khronos so much.
  //[t] commented out for now
  /*
  // Create a render pass
  {
    constexpr uint32_t viewMask = 0b00000011;
    constexpr uint32_t correlationMask = 0b00000011;


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

    //VkRenderPassCreateInfo renderPassCreateInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    vk::RenderPassCreateInfo renderPassCreateInfo;//{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    if(isMultipass)
    {
      // [t] For multiview
      // [t] When you connect VkRenderPassMultiviewCreateInfo to VkRenderPassCreateInfo
      // [t] you are telling Vulkan to execute your pipeline TWICE
      // [t] (or more, depending on the number of view masks set in VkRenderPassMultiviewCreateInfo)
      // [t] The only difference between single and multiview executions is:
      //  with:
      //    #extension GL_EXT_multiview : enable
      //  you get:
      //    gl_ViewIndex 0 or 1: gl_Position = ubo.viewProjection[gl_ViewIndex] * pos;
      // [t] each result goes to layer 0 or layer 1
      VkRenderPassMultiviewCreateInfo renderPassMultiviewCreateInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO
      };
      renderPassMultiviewCreateInfo.subpassCount = 1u;
      renderPassMultiviewCreateInfo.pViewMasks = &viewMask;
      renderPassMultiviewCreateInfo.correlationMaskCount = 1u;
      renderPassMultiviewCreateInfo.pCorrelationMasks = &correlationMask;

      renderPassCreateInfo.pNext = &renderPassMultiviewCreateInfo;
    }
    renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassCreateInfo.pAttachments = attachments.data();
    renderPassCreateInfo.subpassCount = 1u;
    renderPassCreateInfo.pSubpasses = &subpassDescription;
    //if (vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass) != VK_SUCCESS)

    if (gfxWgpuVulkanShared->vkDevice.createRenderPass(&renderPassCreateInfo, nullptr, &renderPass, gfxWgpuVulkanShared->vkLoader) != vk::Result::eSuccess)
    {
      util::error(Error::GenericVulkan);
      valid = false;
      return;
    }
  }*/

  spdlog::info("[log][t] Headset::Headset: vukan context for openxr");
  // vukan context for openxr

  const XrInstance xrInstance = xrContext->getXrInstance();
  const XrSystemId xrSystemId = xrContext->getXrSystemId();
  const VkPhysicalDevice vkPhysicalDevice = gfxWgpuVulkanShared->vkPhysicalDevice;
  const uint32_t vkDrawQueueFamilyIndex = gfxWgpuVulkanShared->queueFamilyIndex;

  spdlog::info("[log][t] Headset::Headset: Create an xr session with Vulkan graphics binding.");
  // Create an xr session with Vulkan graphics binding
  XrGraphicsBindingVulkanKHR graphicsBinding{ XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR };
  graphicsBinding.device = device;
  graphicsBinding.instance = gfxWgpuVulkanShared->vkInstance;
  graphicsBinding.physicalDevice = vkPhysicalDevice;
  graphicsBinding.queueFamilyIndex = vkDrawQueueFamilyIndex;
  graphicsBinding.queueIndex = gfxWgpuVulkanShared->queueIndex;//0u;

  XrSessionCreateInfo sessionCreateInfo{ XR_TYPE_SESSION_CREATE_INFO };
  sessionCreateInfo.next = &graphicsBinding;
  sessionCreateInfo.systemId = xrSystemId;

  XrResult result = xrCreateSession(xrInstance, &sessionCreateInfo, &session);

  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    valid = false;
    spdlog::error("[log][t] Headset::Headset: error at xrCreateSession(xrInstance, &sessionCreateInfo, &session);");
    return;
  }

  spdlog::info("[log][t] Headset::Headset: Create an xr play space.");
  // Create an xr play space
  XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
  referenceSpaceCreateInfo.referenceSpaceType = spaceType;
  referenceSpaceCreateInfo.poseInReferenceSpace = util::makeIdentity();
  result = xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &space);
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    valid = false;
    spdlog::error("[log][t] Headset::Headset: error at  xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &space);");
    return;
  }

  const XrViewConfigurationType viewType = xrContext->getXrViewType();

  spdlog::info("[log][t] Headset::Headset: Get xr eye information.");
  // Get the number of eyes
  result = xrEnumerateViewConfigurationViews(xrInstance, xrSystemId, viewType, 0u,
                                             reinterpret_cast<uint32_t*>(&eyeCount), nullptr);
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    valid = false;
    spdlog::error("[log][t] Headset::Headset: error at xrEnumerateViewConfigurationViews(xrInstance, xrSystemId, viewType, 0u, reinterpret_cast<uint32_t*>(&eyeCount), nullptr); result: {0}, eyeCount: {1}, viewType: {2}",result, eyeCount, viewType);
    return;
  }

  spdlog::info("[log][t] Headset::Headset: Get xr eye image info.");
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
    spdlog::error("[log][t] Headset::Headset: error at xrEnumerateViewConfigurationViews(xrInstance, xrSystemId, viewType, static_cast<uint32_t>(eyeImageInfos.size()), reinterpret_cast<uint32_t*>(&eyeCount), eyeImageInfos.data());");
    return;
  }

  spdlog::info("[log][t] Headset::Headset: Allocate the eye poses.");
  // Allocate the eye poses
  eyePoses.resize(eyeCount);
  for (XrView& eyePose : eyePoses)
  {
    eyePose.type = XR_TYPE_VIEW;
    eyePose.next = nullptr;
  }

  spdlog::info("[log][t] Headset::Headset: Verify that the desired color format is supported.");
  // Verify that the desired color format is supported
  {
    uint32_t formatCount = 0u;
    result = xrEnumerateSwapchainFormats(session, 0u, &formatCount, nullptr);
    if (XR_FAILED(result))
    {
      util::error(Error::GenericOpenXR);
      valid = false;
      spdlog::error("[log][t] Headset::Headset: error at xrEnumerateSwapchainFormats(session, 0u, &formatCount, nullptr);");
      return;
    }

    std::vector<int64_t> formats(formatCount);
    result = xrEnumerateSwapchainFormats(session, formatCount, &formatCount, formats.data());
    if (XR_FAILED(result))
    {
      util::error(Error::GenericOpenXR);
      valid = false;
      spdlog::error("[log][t] Headset::Headset: error at xrEnumerateSwapchainFormats(session, formatCount, &formatCount, formats.data());");
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
      spdlog::error("[log][t] Headset::Headset: error at color format, no formatFound / feature not suppurted;");
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

  spdlog::info("[log][t] Headset::Headset: Create xr swapchains and render targets.");
  // Create xr swapchains and render targets
  // [t] Either creates one swapchain with 2 layers / swapchain images, or two swapchains with one image each.
  // [t] Either way it's 2 render targets.
  {
    uint32_t swapchainNumber = 1u;
    uint32_t swapchainImageCount = static_cast<uint32_t>(eyeCount);//2u
    if(isMultipass){
      //swapchainNumber = static_cast<uint32_t>(eyeCount);//2u;
      // [guus] Force use only 1 swapchain for now
      swapchainNumber = 1u;
      swapchainImageCount = 1u;
    }
    spdlog::info("[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[log][t] Headset::Headset: swapchainNumber: {}, swapchainImageCount: {}", swapchainNumber, swapchainImageCount);

    swapchainArr.resize(swapchainNumber);

    for(size_t i=0; i< swapchainNumber; i++)
    {

      const XrViewConfigurationView& eyeImageInfo = eyeImageInfos.at(0u);

      // Create a swapchain
      //[t] TODO: In case this isn't enough, look up XrSwapchainCreateInfo swapchainCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO}; in the khronos hello_xr
      XrSwapchainCreateInfo swapchainCreateInfo{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
      swapchainCreateInfo.format = colorFormat;
      swapchainCreateInfo.sampleCount = eyeImageInfo.recommendedSwapchainSampleCount;
      swapchainCreateInfo.width = eyeImageInfo.recommendedImageRectWidth;
      swapchainCreateInfo.height = eyeImageInfo.recommendedImageRectHeight;
      // [t] arraySize is the number of array layers in the image or 1 for a simple 2D image,
      // [t] 2 for 2 layers for multiview
      // [guus] Avoid using array images, doesn't seem to work well with OpenXR => wgpu
      // [t] So swapchainImageCount should remain in multipass mode ( = 1u), until we figure out multiview (singlepass)
      swapchainCreateInfo.arraySize = swapchainImageCount;
      spdlog::info("[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[log][t] Headset::Headset: swapchainCreateInfo.arraySize: {}", swapchainCreateInfo.arraySize);
      swapchainCreateInfo.faceCount = 1u;
      swapchainCreateInfo.mipCount = 1u;
      //[t] TODO: is this needed:
      swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

      spdlog::info("[log][t] Headset::Headset: swapchainCreateInfo: swapchainCreateInfo.format: {}\r\n, swapchainCreateInfo.sampleCount: {}\r\n, swapchainCreateInfo.width: {}\r\n, swapchainCreateInfo.height: {}\r\n, swapchainCreateInfo.arraySize (this is in case you have subimages inside each swapchain): {}\r\n, swapchainCreateInfo.faceCount: {}\r\n, swapchainCreateInfo.mipCount: {}\r\n, swapchainCreateInfo.usageFlags: XR_SWAPCHAIN_USAGE_SAMPLED_BIT[{}] | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT[{}]: {}\r\n",
      swapchainCreateInfo.format,
      swapchainCreateInfo.sampleCount,
      swapchainCreateInfo.width,
      swapchainCreateInfo.height,
      swapchainCreateInfo.arraySize,
      swapchainCreateInfo.faceCount,
      swapchainCreateInfo.mipCount,
      XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
      XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
      swapchainCreateInfo.usageFlags
      );

      XrSwapchain xrSwapchain;
      result = xrCreateSwapchain(session, &swapchainCreateInfo, &xrSwapchain);
      swapchainArr.at(i) = xrSwapchain;// [t] &swapchain.handle? ^
      if(swapchainArr.at(i) == nullptr)
        spdlog::error("[log][t] Headset::Headset: error at swapchainarr");
      if (XR_FAILED(result))
      {
        util::error(Error::GenericOpenXR);
        valid = false;

        spdlog::error("[log][t] Headset::Headset: error at xrCreateSwapchain(session, &swapchainCreateInfo, swapchainArr.at(i)); result: {}, i: {}, swapchainNumber: {}, XrSessionState: {} -> transitioning to XR_SESSION_STATE_UNKNOWN", result, i, swapchainNumber, sessionState);
        XrSessionState sessionState = XR_SESSION_STATE_UNKNOWN;
        return;
      }

      // [t] Get the number of swapchain images for this swapchain --
      // [t] If multipass, there should be 2 swapchains, each with one swapchain image.
      // [t] If singlepass, there should be one xr swapchain, with one (multiview) image (which has 2 layers).
      // [t] Multiview has 2 layers, one per eye, set in swapchainCreateInfo.arraySize above.
      // [t] But we need to use this xrEnumerateSwapchainImages magic to create 2 swapchainImages,
      // [t] one swapchain image for each multiview image layer
      uint32_t swapchainImageCount;
      result = xrEnumerateSwapchainImages(swapchainArr.at(i), 0u, &swapchainImageCount, nullptr);
      if (XR_FAILED(result))
      {
        util::error(Error::GenericOpenXR);
        valid = false;
        spdlog::error("[log][t] Headset::Headset: error at xrEnumerateSwapchainImages(*(swapchainArr.at(i)), 0u, &swapchainImageCount, nullptr);");
        return;
      }
      spdlog::info("[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[log][t] Headset::Headset: swapchainImageCount: {}", swapchainImageCount);

      //[t] xr - vk

      //[t] Retrieve the swapchain images, which can be one per layer of multiview
      std::vector<XrSwapchainImageVulkanKHR> swapchainImages;
      swapchainImages.resize(swapchainImageCount);
      for (XrSwapchainImageVulkanKHR& swapchainImage : swapchainImages)
      {
        swapchainImage.type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
      }

      //[t] TODO: So at this point, I believe the vk swapchain image should be the one set up for the WGPUVulkanShared vkInstance
      //[t] because the graphicsBinding vk instance is set to our already set up gfxWgpuVulkanShared->instance

      XrSwapchainImageBaseHeader* data = reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImages.data());
      result = xrEnumerateSwapchainImages(swapchainArr.at(i), static_cast<uint32_t>(swapchainImages.size()), &swapchainImageCount, data);
      if (XR_FAILED(result))
      {
        util::error(Error::GenericOpenXR);
        valid = false;
        spdlog::error("[log][t] Headset::Headset: error at xrEnumerateSwapchainImages(*(swapchainArr.at(i)), static_cast<uint32_t>(swapchainImages.size()), &swapchainImageCount, data);");
        return;
      }

      //[t] create the xr vk render target(s). Each RenderTarget and VKImage here,
      //[t] represents one of the 2 array layers in the swapchaincreateinfo (multiview).
      //[t] alternatively we set up 2 swapchains with one layer each.
      swapchainRenderTargets.resize(swapchainImages.size());
      //swapchainRTTextureViews.clear();
      for (size_t renderTargetIndex = 0u; renderTargetIndex < swapchainRenderTargets.size(); ++renderTargetIndex)
      {
        RenderTarget*& renderTarget = swapchainRenderTargets.at(renderTargetIndex);
        //[t] image should be based on device/instance data of WGPUVukanShared if all is done right
        const VkImage image = swapchainImages.at(renderTargetIndex).image;
        //[t] One rendertarget for each swapchainImage (layer). All using the same renderpass.
        //[t] depthImageView can be null
        //[t] 2u is the layer count for multiview. The results of rendering with gl_ViewIndex (see other comment on gl_ViewIndex).
        // [guus] Don't use layers for now, see previous comment
        // [t] layerCount should remain 1u unless we use singlepass (multiview)
        uint32_t layerCount = 2u;
        if(isMultipass)
           layerCount = 1u;
        spdlog::info("[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[log][t] Headset::Headset: layerCount: {}, swapchainRenderTargets.size(): {}", layerCount, swapchainRenderTargets.size());
        //renderTarget = new RenderTarget(device, image, depthImageView, eyeResolution, colorFormat_wgpu, renderPass, layerCount);
        renderTarget = new RenderTarget(gfxWgpuVulkanShared->wgpuDevice, image, depthImageView, eyeResolution, colorFormat_wgpu, layerCount);
        //[t] TODO: Also, the vulkan code I've been using VKFormat instead of WGPUTextureFormat colorFormat_wgpu

        if (!renderTarget->isValid())
        {
          valid = false;
          spdlog::error("[log][t] Headset::Headset: error at renderTarget->isValid();");
          return;
        }

        //swapchainRTTextureViews.emplace_back(gfx::toWgpuHandle(renderTarget->getRTTextureView()));
      }

    }
  }

  spdlog::info("[log][t] Headset::Headset: Create the eye render infos / projectionLayerViews.");
  //[t] Create the eye render infos / projectionLayerViews. These should be the same regardless if single pass or multipass
  eyeRenderInfos.resize(eyeCount);
  for (size_t eyeIndex = 0u; eyeIndex < eyeRenderInfos.size(); ++eyeIndex)
  {
    XrCompositionLayerProjectionView& eyeRenderInfo = eyeRenderInfos.at(eyeIndex);
    eyeRenderInfo.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
    eyeRenderInfo.next = nullptr;

    // Associate this eye with the swapchain
    const XrViewConfigurationView &eyeImageInfo = eyeImageInfos.at(eyeIndex);
    if(swapchainArr.size() == 1){ 
      eyeRenderInfo.subImage.swapchain = swapchainArr.at(0);//swapchainArr.at(eyeIndex);//multiview, 1 swapchain, 2 swapchainImages
    }
    else{
      eyeRenderInfo.subImage.swapchain = swapchainArr.at(eyeIndex);//multipass, 2 swapchains
    }
    
    if(isMultipass){
      eyeRenderInfo.subImage.imageArrayIndex = 0; 
    }
    else{// singlepass multiview
      eyeRenderInfo.subImage.imageArrayIndex = static_cast<uint32_t>(eyeIndex);
    }

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
  spdlog::info("[log][t] Headset::~Headset().");
  // Clean up OpenXR
  xrEndSession(session);
  for(size_t i=0; i< swapchainArr.size(); i++)
    xrDestroySwapchain(swapchainArr.at(i));

  for (const RenderTarget* swapchainRenderTarget: swapchainRenderTargets)
  {
    delete swapchainRenderTarget;
  }

  xrDestroySpace(space);
  xrDestroySession(session);

  // Clean up Vulkan
  const VkDevice vkDevice = gfxWgpuVulkanShared->vkDevice;
  //vkDestroyImageView(vkDevice, depthImageView, nullptr);
  gfxWgpuVulkanShared->vkDevice.destroyImageView(depthImageView, nullptr, gfxWgpuVulkanShared->vkLoader);
  //vkFreeMemory(vkDevice, depthMemory, nullptr);
  gfxWgpuVulkanShared->vkDevice.freeMemory(depthMemory, nullptr, gfxWgpuVulkanShared->vkLoader);
  //vkDestroyImage(vkDevice, depthImage, nullptr);
  gfxWgpuVulkanShared->vkDevice.destroyImage(depthImage, nullptr, gfxWgpuVulkanShared->vkLoader);
  //vkDestroyRenderPass(vkDevice, renderPass, nullptr);
  gfxWgpuVulkanShared->vkDevice.destroyRenderPass(renderPass, nullptr, gfxWgpuVulkanShared->vkLoader);
}

//[t] Whether we render single pass (single swapchain with multiview) or multipass (multiple swapchains),
//[t] this is common setup for each frame. Followed by acquireSwapchainForFrame, render, releaseSwapchain, and endFrame
Headset::BeginFrameResult Headset::beginFrame()
{
  if(frame<debugs){
    spdlog::info("[log][t] Headset::beginFrame().");
    spdlog::info("[log][t] Headset::beginFrame: xrContext->getXrInstance().");
  }
  const XrInstance instance = xrContext->getXrInstance();

  if(frame<debugs){
    spdlog::info("[log][t] Headset::beginFrame: Poll OpenXR events while().");
  }
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
        if(frame<debugs){
          spdlog::info("[log][t] Headset::beginFrame: XR_SESSION_STATE_READY.");
        }
        if (!beginSession())
        {
          return BeginFrameResult::Error;
        }
      }
      else if (event->state == XR_SESSION_STATE_STOPPING)
      {
        if(frame<debugs){
          spdlog::info("[log][t] Headset::beginFrame: XR_SESSION_STATE_STOPPING.");
        }
        if (!endSession())
        {
          return BeginFrameResult::Error;
        }
      }
      else if (event->state == XR_SESSION_STATE_LOSS_PENDING || event->state == XR_SESSION_STATE_EXITING)
      {
        if(frame<debugs){
          spdlog::info("[log][t] Headset::beginFrame: XR_SESSION_STATE_LOSS_PENDING || XR_SESSION_STATE_EXITING.");
        }
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
    if(frame<debugs){
      spdlog::info("[log][t] Headset::beginFrame: Skipping frame because not ready, or not synchronized or not visible or not focused.");
    }
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
    spdlog::error("[log][t] Headset::beginFrame: error at xrWaitFrame(session, &frameWaitInfo, &frameState);");
    return BeginFrameResult::Error;
  }

  if(frame<debugs){
    spdlog::info("[log][t] Headset::beginFrame: Begin the new frame xrBeginFrame().");
  }
  // Begin the new frame
  XrFrameBeginInfo frameBeginInfo{ XR_TYPE_FRAME_BEGIN_INFO };
  result = xrBeginFrame(session, &frameBeginInfo);
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    spdlog::error("[log][t] Headset::beginFrame: error at xrBeginFrame(session, &frameBeginInfo);");
    return BeginFrameResult::Error;
  }

  if (!frameState.shouldRender)
  {
    if(frame<debugs){
      spdlog::info("[log][t] Headset::beginFrame: Let the host know that we don't want to render this frame. We do still need to end the frame however.");
    }
    // Let the host know that we don't want to render this frame
    // We do still need to end the frame however
    return BeginFrameResult::SkipRender;
  }

  spdlog::info("[log][t] Headset::beginFrame: Update the eye poses.");
  // Update the eye poses
  viewState.type = XR_TYPE_VIEW_STATE;
  uint32_t viewCount;
  XrViewLocateInfo viewLocateInfo{ XR_TYPE_VIEW_LOCATE_INFO };
  viewLocateInfo.viewConfigurationType = xrContext->getXrViewType();
  viewLocateInfo.displayTime = frameState.predictedDisplayTime;
  viewLocateInfo.space = space;
  result = xrLocateViews(session, &viewLocateInfo, &viewState, static_cast<uint32_t>(eyePoses.size()), &viewCount, eyePoses.data());
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    spdlog::error("[log][t] Headset::beginFrame: error at xrLocateViews(session, &viewLocateInfo, &viewState, static_cast<uint32_t>(eyePoses.size()), &viewCount, eyePoses.data());");
    return BeginFrameResult::Error;
  }

  if (viewCount != eyeCount)
  {
    util::error(Error::GenericOpenXR);
    spdlog::error("[log][t] Headset::beginFrame: error at viewCount != eyeCount");
    return BeginFrameResult::Error;
  }

  if(frame<debugs){
    spdlog::info("[log][t] Headset::beginFrame: Update the eye render infos, view and projection matrices.");
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
    eyeViewMatrices.at(eyeIndex) = util::glmToLinalgFloat4x4(util::poseToMatrix(pose));
    eyeProjectionMatrices.at(eyeIndex) = util::glmToLinalgFloat4x4(util::createProjectionMatrix(eyeRenderInfo.fov, 0.1f, 250.0f));
  }

  if(frame<debugs){
    spdlog::info("[log][t] Headset::beginFrame: End. Request acquiring of current swapchain image, then after request full rendering of the frame on this swapchain, then afterwards releaseSwapchain() and endFrame().");
  }
  // Request acquiring of current swapchain image, then after request full rendering of the frame on this swapchain, then afterwards releaseSwapchain() and endFrame()
  return BeginFrameResult::RenderFully;
}

std::vector<WGPUTextureView> Headset::requestFrame() {
  if(frame<debugs){
    frame++;
    spdlog::info("[log][t] IContextMainOutput::Headset::requestFrame().");
  }
  Headset::BeginFrameResult frameResult = beginFrame();
  if (frameResult == Headset::BeginFrameResult::Error)
  {
    std::vector<WGPUTextureView> err;
    spdlog::error("[log][t] Headset::requestFrame: error at beginFrame()");
    return err;
  }

  swapchainRTTextureViews.clear();

  //TODO: try multipass, but, read dscription of acquireSwapchainForFrame, not sure you can call it twice per "frame"
  /*
    // for multipass, have 2 swapchains
    uint32_t swapchainNumber = 1;
    if(isMultipass){
      swapchainNumber = 2u;
    }

    for(size_t eyeIndex=0; eyeIndex< swapchainNumber; eyeIndex++)
  */

  uint32_t eyeCount = 1;
  swapchainRTTextureViews.resize(eyeCount);
  uint32_t eyeIndex= 0;
  uint32_t swapchainImageIndex;
  frameResult = acquireSwapchainForFrame(eyeIndex, swapchainImageIndex);

  if (frameResult == Headset::BeginFrameResult::Error)
  {
    std::vector<WGPUTextureView> err;
    spdlog::error("[log][t] Headset::requestFrame: error at acquireSwapchainForFrame(eyeIndex, swapchainImageIndex);");
    return err;
  }
  else{
    frameResult = Headset::BeginFrameResult::RenderFully;
  }

  swapchainRTTextureViews.at(eyeIndex) = getRenderTarget(swapchainImageIndex)->getRTTextureView();// of swapchainRTTextureViews matching internal swapchain index


  VkExtent2D vke2d = getEyeResolution(0u);
  linalgint2size = linalg::aliases::int2(vke2d.width, vke2d.height);
  if(frame<debugs){
    spdlog::info("[log][t] IContextMainOutput::Headset::requestFrame: End.");
  }
  return swapchainRTTextureViews;

  /*
  assert(!currentView);

  vk::ResultValue<uint32_t> result =
      gfxWgpuVulkanShared->vkDevice.acquireNextImageKHR(mirrorSwapchain, UINT64_MAX, {}, fence, gfxWgpuVulkanShared->vkLoader);
  if (result.result == vk::Result::eSuccess) {
    vk::Result waitResult = gfxWgpuVulkanShared->vkDevice.waitForFences(fence, true, UINT64_MAX, gfxWgpuVulkanShared->vkLoader);
    assert(waitResult == vk::Result::eSuccess);

    gfxWgpuVulkanShared->vkDevice.resetFences(fence, gfxWgpuVulkanShared->vkLoader);

    currentImageIndex = result.value; // but 2 xr swapchains?
    currentView = swapchainRTTextureViews[currentImageIndex];
    return currentView;
  } else
  {
    return nullptr;
  }*/
}

WGPUTextureFormat Headset::getFormat() const {
  return colorFormat_wgpu;
}

std::vector<gfx::IContextCurrentFramePayload> Headset::getCurrentFrame() const {
  if(frame<debugs){
    spdlog::info("[log][t] IContextMainOutput::Headset::getCurrentFrame().");
  }
  //assert(currentView);
  //return currentView;
  std::vector<gfx::IContextCurrentFramePayload> payload;
  payload.resize(swapchainRTTextureViews.size());
  for(size_t eyeIndex = 0; eyeIndex<swapchainRTTextureViews.size(); eyeIndex++){
    payload.at(eyeIndex).wgpuTextureView = swapchainRTTextureViews.at(eyeIndex);
    payload.at(eyeIndex).useMatrix = true;
    payload.at(eyeIndex).eyeViewMatrix = getEyeViewMatrix(eyeIndex);
    payload.at(eyeIndex).eyeProjectionMatrix = getEyeProjectionMatrix(eyeIndex);
  }
  if(frame<debugs){
    spdlog::info("[log][t] IContextMainOutput::Headset::getCurrentFrame: End.");
  }
  return payload;
}

//[t] called from context; see: backend = std::make_shared<ContextXrGfxBackend>()
void Headset::present() {
  if(frame<debugs){
    spdlog::info("[log][t] IContextMainOutput::Headset::present().");
  }
  /*
  WGPUExternalPresentVK wgpuPresent{};
  wgpuPrepareExternalPresentVK(gfxWgpuVulkanShared->wgpuQueue, &wgpuPresent);

  vk::Semaphore waitSemaphore = wgpuPresent.waitSemaphore ? VkSemaphore(wgpuPresent.waitSemaphore) : nullptr;

  vk::PresentInfoKHR presentInfo;
  presentInfo.setSwapchainCount(1);
  presentInfo.setPSwapchains(&mirrorSwapchain);
  presentInfo.setPImageIndices(&currentImageIndex);
  presentInfo.setPWaitSemaphores(&waitSemaphore);
  presentInfo.setWaitSemaphoreCount(1);

  // presentInfo.setWaitSemaphores(const vk::ArrayProxyNoTemporaries<const vk::Semaphore> &waitSemaphores_)
  vk::Result result = gfxWgpuVulkanShared->vkQueue.presentKHR(&presentInfo, gfxWgpuVulkanShared->vkLoader);
  assert(result == vk::Result::eSuccess);
  */
  // Increment image
  currentView = nullptr;

  //[t] for each headset swapchain, release swapchain
  for(size_t i=0; i< swapchainRTTextureViews.size(); i++){
    releaseSwapchain(i);
  }
  //[t] end xr frame
  endFrame();
  if(frame<debugs){
    spdlog::info("[log][t] IContextMainOutput::Headset::present: End.");
  }
}

//[t] it's rendered per swapchain (and we should have one swapchain for single pass),
//if the swapchain has 2 images (single pass with multiview), both should reference a layered multiview image.
//OpenXR is set up with this swapchain image and internally assigns an index to it.
//But still provides data for 2 eyes (matrixes etc).
// TODO: guus: I don't know if this can be called twice (e.g. for both eyes at the same time, instead of rendering one and then rendering the other)
// So I don't know if we can send  textures to the renderer at the same time
Headset::BeginFrameResult Headset::acquireSwapchainForFrame(uint32_t eyeIndex, uint32_t& swapchainImageIndex)
{
  if(frame<debugs){
    spdlog::info("[log][t] Headset::acquireSwapchainForFrame(eyeIndex{0}, swapchainImageIndex{1}).", eyeIndex, swapchainImageIndex);
  }
  //[t] MULTIPASS from here

  if(frame<debugs){
    spdlog::info("[log][t] Headset::acquireSwapchainForFrame: Acquire the multiview swapchain image, or one of the 2 multipass swapchains.");
  }
  //[t] Acquire the multiview swapchain image, or one of the 2 multipass swapchains
  XrSwapchainImageAcquireInfo swapchainImageAcquireInfo{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
  XrSwapchain swapchain = swapchainArr.at(eyeIndex);
  //XrResult result = xrAcquireSwapchainImage(*(swapchainArr.at(eyeIndex)), &swapchainImageAcquireInfo, &swapchainImageIndex);
  XrResult result = xrAcquireSwapchainImage(swapchain, &swapchainImageAcquireInfo, &swapchainImageIndex);
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    spdlog::error("[log][t] Headset::acquireSwapchainForFrame: error at xrAcquireSwapchainImage(*(swapchainArr.at(eyeIndex)), &swapchainImageAcquireInfo, &swapchainImageIndex); Result: {}, eyeIndex: {}, swapchainImageIndex: {}", result, eyeIndex, swapchainImageIndex);
    return BeginFrameResult::Error;
  }

  if(frame<debugs){
    spdlog::info("[log][t] Headset::acquireSwapchainForFrame: Wait for the swapchain image.");
  }
  // Wait for the swapchain image
  XrSwapchainImageWaitInfo swapchainImageWaitInfo{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
  swapchainImageWaitInfo.timeout = XR_INFINITE_DURATION;
  //result = xrWaitSwapchainImage(*(swapchainArr.at(eyeIndex)), &swapchainImageWaitInfo);
  result = xrWaitSwapchainImage(swapchain, &swapchainImageWaitInfo);

  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    spdlog::error("[log][t] Headset::acquireSwapchainForFrame: error at xrWaitSwapchainImage(*(swapchainArr.at(eyeIndex)), &swapchainImageWaitInfo);");
    return BeginFrameResult::Error;
  }

  if(frame<debugs){
    spdlog::info("[log][t] Headset::acquireSwapchainForFrame: End. Request full rendering of the frame on this swapchain, then afterwards releaseSwapchain() and endFrame().");
  }
  // Request full rendering of the frame on this swapchain, then afterwards releaseSwapchain() and endFrame()
  return BeginFrameResult::RenderFully;
}

void Headset::releaseSwapchain(uint32_t eyeIndex) const
{
  if(frame<debugs){
    spdlog::info("[log][t] Headset::releaseSwapchain(eyeIndex{}). Release XR Swapchain.",eyeIndex);
  }
  // Release the swapchain image
  XrSwapchainImageReleaseInfo swapchainImageReleaseInfo{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
  XrResult result = xrReleaseSwapchainImage(swapchainArr.at(eyeIndex), &swapchainImageReleaseInfo);
  if (XR_FAILED(result))
  {
    return;
  }
  if(frame<debugs){
    spdlog::info("[log][t] Headset::releaseSwapchain: End. Released XR Swapchain.");
  }
}

void Headset::endFrame() const
{
  if(frame<debugs){
    spdlog::info("[log][t] Headset::endFrame(). End XR Frame.");
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
  XrResult result = xrEndFrame(session, &frameEndInfo);
  if (XR_FAILED(result))
  {
    return;
  }
  if(frame<debugs){
    spdlog::info("[log][t] Headset::endFrame(). End. Ended XR Frame.");
  }
}


const linalg::aliases::int2 &Headset::getSize() const {
  //VkExtent2D vkextent2d = getEyeResolution(0u);

  return linalgint2size;
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

VkExtent2D Headset::getEyeResolution(size_t eyeIndex)// const
{
  const XrViewConfigurationView& eyeInfo = eyeImageInfos.at(eyeIndex);
  return { eyeInfo.recommendedImageRectWidth, eyeInfo.recommendedImageRectHeight };
}

//glm::mat4
linalg::aliases::float4x4 Headset::getEyeViewMatrix(size_t eyeIndex) const
{
  return eyeViewMatrices.at(eyeIndex);
}

//glm::mat4
linalg::aliases::float4x4 Headset::getEyeProjectionMatrix(size_t eyeIndex) const
{
  return eyeProjectionMatrices.at(eyeIndex);
}

RenderTarget* Headset::getRenderTarget(size_t swapchainImageIndex) const
{
  return swapchainRenderTargets.at(swapchainImageIndex);
}

bool Headset::beginSession() const
{
  spdlog::info("[log][t] Headset::beginSession().");
  // Start the session
  XrSessionBeginInfo sessionBeginInfo{ XR_TYPE_SESSION_BEGIN_INFO };
  sessionBeginInfo.primaryViewConfigurationType = xrContext->getXrViewType();
  const XrResult result = xrBeginSession(session, &sessionBeginInfo);
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    spdlog::error("[log][t] Headset::beginSession: error at xrBeginSession(session, &sessionBeginInfo).");
    return false;
  }

  spdlog::info("[log][t] Headset::beginSession(). End.");
  return true;
}

bool Headset::endSession() const
{
  spdlog::info("[log][t] Headset::endSession().");
  // End the session
  const XrResult result = xrEndSession(session);
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    spdlog::error("[log][t] Headset::endSession: error at xrEndSession(session).");
    return false;
  }

  spdlog::info("[log][t] Headset::endSession(). End.");
  return true;
}