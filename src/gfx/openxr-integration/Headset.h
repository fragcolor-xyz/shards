#pragma once

#include <Vulkan-Headers/include/vulkan/vulkan.h>

#define XR_USE_GRAPHICS_API_VULKAN
#include <openxrsdk/include/openxr/openxr.h>
#include <openxrsdk/include/openxr/openxr_platform.h>

//#include <glm/include/glm/mat4x4.hpp>
#include "linalg.hpp"
//#include "linalg.h"
#include <vector>


//#include "context.hpp"
#include "context_xr.h"
#include "context_xr_gfx_data.hpp"


//#include "wgpu_handle.hpp"
//#include <gfx_rust.h>

#include "RenderTarget.h"



class RenderTarget;

class Headset:public gfx::IContextMainOutput
{
public:
  Headset(const std::shared_ptr<Context_XR> xrContext, std::shared_ptr<gfx::WGPUVulkanShared> _gfxWgpuVulkanShared, bool isMultipass);
  ~Headset();

  enum class BeginFrameResult
  {
    Error,       // An error occurred
    RenderFully, // Render this frame normally
    SkipRender,  // Skip rendering the frame but end it
    SkipFully    // Skip processing this frame entirely without ending it
  };
  BeginFrameResult beginFrame();
  BeginFrameResult acquireSwapchainForFrame(uint32_t eyeIndex, uint32_t& swapchainImageIndex);
  void releaseSwapchain(uint32_t eyeIndex) const;
  void endFrame() const;

  const linalg::aliases::int2 &getSize() const override;
  WGPUTextureFormat getFormat() const override;
  std::vector<WGPUTextureView> requestFrame() override;
  std::vector<gfx::IContextCurrentFramePayload> getCurrentFrame() const override;
  void present() override;

  bool isValid() const;
  bool isExitRequested() const;
  VkRenderPass getRenderPass() const;
  size_t getEyeCount() const;
  VkExtent2D getEyeResolution(size_t eyeIndex);// const;
  //glm::mat4 
  linalg::aliases::float4x4 getEyeViewMatrix(size_t eyeIndex) const;
  //glm::mat4 
  linalg::aliases::float4x4 getEyeProjectionMatrix(size_t eyeIndex) const;
  RenderTarget* getRenderTarget(size_t swapchainImageIndex) const;

private:
  uint32_t frame = 0; 
  uint32_t debugs = 3;    

  bool valid = true;
  bool exitRequested = false;
  linalg::aliases::int2 linalgint2size;

  std::shared_ptr<Context_XR> xrContext = nullptr;
  //const gfx::Context* gfxWgpuVulkanContext = nullptr;
  std::shared_ptr<gfx::WGPUVulkanShared> gfxWgpuVulkanShared = nullptr;

  size_t eyeCount = 0u;
  //std::vector<glm::mat4> 
  std::vector<linalg::aliases::float4x4> eyeViewMatrices;
  //std::vector<glm::mat4> 
  std::vector<linalg::aliases::float4x4> eyeProjectionMatrices;

  XrSession session = nullptr;
  XrSessionState sessionState = XR_SESSION_STATE_UNKNOWN;
  XrSpace space = nullptr;
  XrFrameState frameState = {};
  XrViewState viewState = {};

  std::vector<XrViewConfigurationView> eyeImageInfos;
  std::vector<XrView> eyePoses;
  std::vector<XrCompositionLayerProjectionView> eyeRenderInfos;

  std::vector<XrSwapchain*> swapchainArr;// = nullptr;
  std::vector<RenderTarget*> swapchainRenderTargets;
  //std::vector<gfx::WgpuHandle<WGPUTextureView>> swapchainRTTextureViews;
  std::vector<WGPUTextureView> swapchainRTTextureViews;
  
  WGPUTextureView currentView{}; 

  VkRenderPass renderPass = nullptr;

  // Depth buffer
  VkImage depthImage = nullptr;
  VkDeviceMemory depthMemory = nullptr;
  VkImageView depthImageView = nullptr;

  bool beginSession() const;
  bool endSession() const;
};