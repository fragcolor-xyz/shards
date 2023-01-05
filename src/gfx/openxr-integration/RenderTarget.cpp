#include "RenderTarget.h"

#include "Util.h"


RenderTarget::RenderTarget(WGPUDevice wgpuDevice,
                           VkImage image,
                           VkImageView depthImageView,//[t] TODO: to implement, if we need depth
                           VkExtent2D size,
                           WGPUTextureFormat format,
                           //VkRenderPass renderPass,
                           uint32_t layerCount)
: wgpuDevice(wgpuDevice), image(image)
{
  valid = false;
  WGPUTextureDescriptor textureDesc{
      .usage = WGPUTextureUsage::WGPUTextureUsage_CopyDst | WGPUTextureUsage::WGPUTextureUsage_RenderAttachment,
      .dimension = WGPUTextureDimension_2D,
      .size = WGPUExtent3D{.width = uint32_t(size.width), .height = uint32_t(size.height), .depthOrArrayLayers = 1},
      .format = format,
      .mipLevelCount = 1,
      .sampleCount = 1,
      .viewFormatCount = 0,
  };

  WGPUTextureViewDescriptor viewDesc{
      .format = textureDesc.format,
      .dimension = WGPUTextureViewDimension_2D,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .arrayLayerCount = layerCount,//[t] TODO: is this equivalent to vukan: imageViewCreateInfo.subresourceRange.layerCount = layerCount;
      .aspect = WGPUTextureAspect_All,
  };

  WGPUExternalTextureDescriptorVK extDescVk{
      .chain = {.sType = WGPUSType(WGPUNativeSTypeEx_ExternalTextureDescriptorVK)},
      .image = image,
  };
  WGPUExternalTextureDescriptor extDesc{
      .nextInChain = &extDescVk.chain,
  };

  textureView = wgpuCreateExternalTextureView(wgpuDevice, &textureDesc, &viewDesc, &extDesc);
  assert(textureView);
  valid = true;



  // VK version
  /*
  // Create an image view
  VkImageViewCreateInfo imageViewCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
  imageViewCreateInfo.image = image;
  imageViewCreateInfo.format = format;
  imageViewCreateInfo.viewType = (layerCount == 1u ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY);
  imageViewCreateInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                     VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
  imageViewCreateInfo.subresourceRange.layerCount = layerCount;
  imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageViewCreateInfo.subresourceRange.baseArrayLayer = 0u;
  imageViewCreateInfo.subresourceRange.baseMipLevel = 0u;
  imageViewCreateInfo.subresourceRange.levelCount = 1u;
  if (vkCreateImageView(device, &imageViewCreateInfo, nullptr, &imageView) != VK_SUCCESS)
  {
    util::error(Error::GenericVulkan);
    valid = false;
    return;
  }

  std::vector<VkImageView> attachments = { imageView };
  if (depthImageView)
  {
    attachments.push_back(depthImageView);
  }

  // Create a framebuffer
  VkFramebufferCreateInfo framebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
  framebufferCreateInfo.renderPass = renderPass;
  framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  framebufferCreateInfo.pAttachments = attachments.data();
  framebufferCreateInfo.width = size.width;
  framebufferCreateInfo.height = size.height;
  framebufferCreateInfo.layers = 1u;
  if (vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &framebuffer) != VK_SUCCESS)
  {
    util::error(Error::GenericVulkan);
    valid = false;
    return;
  }
  */
}

RenderTarget::~RenderTarget()
{
  /*
  vkDestroyFramebuffer(device, framebuffer, nullptr);
  vkDestroyImageView(device, imageView, nullptr);
  */
}

bool RenderTarget::isValid() const
{
  return valid;
}

VkImage RenderTarget::getImage() const
{
  return image;
}

/*
VkFramebuffer RenderTarget::getFramebuffer() const
{
  return framebuffer;
}*/

WGPUTextureView RenderTarget::getRTTextureView() const
{
  return textureView;
}
