#pragma once

#include <Vulkan-Headers/include/vulkan/vulkan.h>


#include "platform_surface.hpp"
#include "wgpu_handle.hpp"

#include <gfx_rust.h>

class RenderTarget final
{
public:
  RenderTarget(WGPUDevice wgpuDevice,
               VkImage image,
               VkImageView depthImageView,
               VkExtent2D size,
               WGPUTextureFormat format,
               //VkRenderPass renderPass,
               uint32_t layerCount);
  ~RenderTarget();

  bool isValid() const;
  VkImage getImage() const;
  //VkFramebuffer getFramebuffer() const;
  WGPUTextureView getRTTextureView() const;

private:
  bool valid = true;

  WGPUDevice wgpuDevice = nullptr;
  VkImage image = nullptr;
  VkImageView imageView = nullptr;
  //VkFramebuffer framebuffer = nullptr;

  WGPUTextureView textureView = nullptr;
};