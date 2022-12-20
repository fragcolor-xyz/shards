#include "OpenXRSystem.h"
#include "spdlog/spdlog.h"

const OpenXRSystem::HeadsetType OpenXRSystem::defaultHeadset = {};





//[t] checkXRDeviceReady can be called at any time to check if a certain headset is connected.
bool OpenXRSystem::checkXRDeviceReady(HeadsetType heasetType = defaultHeadset){
  return context_xr->checkXRDeviceReady(heasetType.viewType, 
                                    heasetType.environmentBlendMode, 
                                    heasetType.xrFormFactor);
}


//[t] this should be created before we call anything from context_xr_gfx. 
//[t] But we're using this weird wgpuVulkanShared loader thingy that requires to be set up before calling any vulkan functions from the xr code...
int OpenXRSystem::InitOpenXR(std::shared_ptr<gfx::WGPUVulkanShared> wgpuUVulkanShared, bool isMultipass, HeadsetType headsetType)
{
  spdlog::info("[log][t] OpenXRSystem::InitOpenXR...");

  //context_xr = new Context_XR(wgpuUVulkanShared); 
  context_xr = std::make_shared<Context_XR>(wgpuUVulkanShared); 
  if (!context_xr->isValid())
  {
    spdlog::error("[log][t] OpenXRSystem::InitOpenXR: error at !headset->isValid().");
    return EXIT_FAILURE;
  }

  /*
  //[t] TODO: maybe have a loop here to wait / ask user to plug it in? 
  if(!checkXRDeviceReady(headsetType)){
    return EXIT_FAILURE;
  }*/
  
  
  context_xr->getVulkanExtensionsFromOpenXRInstance();
  //[t] Our WGPUVulkanShared for the purposes of using XR, doesn't need to be created unless the ^above XR specific setup succeeds.
  if (!context_xr->isValid())
  {
    spdlog::error("[[[[[[[[[[[[log][t] OpenXRSystem::InitOpenXR: error at context_xr.");
  }

  //[t] Create Mirror View in / with the gfx context
  //[t] from context.cpp?

  context_xr->createDevice(isMultipass);
  
  //[t] TODO: We also need to verify that OpenXRMirrorView was successful at CreateMirrorSurface() and createMirrorSwapchain() 
  //          for queue, surface, to know if we can render & present
  //    So if context.cpp Context::deviceObtained() passed.

  spdlog::info("[log][t] OpenXRSystem::InitOpenXR... End.");
  return EXIT_SUCCESS;
}

// Requires OpenXRSystem::InitOpenXR and ContextXrGfxBackend->createInstance() to be called first.
//std::shared_ptr<gfx::Headset> OpenXRSystem::createHeadset(bool isMultipass, std::shared_ptr<gfx::WGPUVulkanShared> gfxContext)
std::shared_ptr<gfx::IContextMainOutput> OpenXRSystem::createHeadset(std::shared_ptr<gfx::WGPUVulkanShared> wgpuUVulkanShared, bool isMultipass)
{
  spdlog::info("[log][t] OpenXRSystem::createHeadset(wgpuUVulkanShared, bool isMultipass[{}])...",isMultipass);
  this->isMultipass = isMultipass;

  if(context_xr == nullptr)
    spdlog::error("[[[[[[[[[[[[log][t] OpenXRSystem::createHeadset: context_xr == nullptr.");
  if (!context_xr->isValid())
  {
    spdlog::error("[[[[[[[[[[[[log][t] OpenXRSystem::createHeadset: error at context_xr.");
  }
  
  headset = std::make_shared<Headset>(context_xr, wgpuUVulkanShared, isMultipass); 
  //headset = new Headset(context_xr, gfxContext, isMultipass);
  if (!headset->isValid())
  {
    spdlog::error("[log][t] OpenXRSystem::createHeadset: error at !headset->isValid().");
    return nullptr;
  }

  spdlog::info("[log][t] OpenXRSystem::createHeadset(). End.");
  return headset;
}

bool OpenXRSystem::getIsMultipass(){
  return isMultipass;
}

/*
int OpenXRSystem::somethingSomethingMakeFrames(){

  //[t] Main loop. TODO: Guus: Where do we integrate this sort of thing?
  while (!headset->isExitRequested() )//&& !mirrorView.isExitRequested())
  {

    //[t] Guus: process main window events? -- I assume we already do this, where?
    
    //  struct MainWindow : public Base {
    //  window->pollEvents(events);
    //  // openxr events here
    


    //[t] If we run in isMultipass (render twice), then we have 2 swapchains.
    //[t] If we run in single pass (multiview), then we have 1 swapchain with 2 layers
    //[t] The headset has an array of swapchain images, and each one has a vukan swapchainImageIndex
    //[t] So each time you acquireSwapchainForFrame, you get back the swapchainImageIndex for the swapchain you're working with.
    uint32_t swapchainImageIndex;
    
    Headset::BeginFrameResult frameResult = headset->beginFrame();
    //mirrorView WGPUTextureView requestFrame() 
    
    if (frameResult == Headset::BeginFrameResult::Error)
    {
      return EXIT_FAILURE;
    }

    // for multipass, have 2 swapchains
    uint32_t swapchainNumber = 1;
    if(isMultipass){
      swapchainNumber = 2u;
    }

    for(size_t i=0; i< swapchainNumber; i++)
    {
      frameResult = headset->acquireSwapchainForFrame(i, swapchainImageIndex);

      if (frameResult == Headset::BeginFrameResult::Error)
      {
        return EXIT_FAILURE;
      }
      else
      if (frameResult == Headset::BeginFrameResult::RenderFully)
      {
        //[t] The swapchainImageIndex points to a rendertarget, so we can get the frame buffer from it.
        //[t] i = what pass we are on (one per eye) in the case we are multi-pass
        
        //  So in renderer we should have a wgpu equivalent of:
        //    VkRenderPassBeginInfo renderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        //    renderPassBeginInfo.renderPass = headset->getRenderPass();
        //    renderPassBeginInfo.framebuffer = headset->getRenderTarget(swapchainImageIndex)->getFramebuffer();
        //    renderPassBeginInfo.renderArea.offset = { 0, 0 };
        //    renderPassBeginInfo.renderArea.extent = headset->getEyeResolution(0u);
        //    renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        //    renderPassBeginInfo.pClearValues = clearValues.data();
        
        //renderer.render(i, swapchainImageIndex);
          // eye data
          // swapchains
          // also the mirror view

            
        //void render(Headset headset, uint32_t eyeIndex, uint32_t swapchainImageIndex){
        //  const VkCommandBuffer commandBuffer = renderer->getCurrentCommandBuffer();//[t] TODO: but we want wgpu command buffer, right?
        //  
        //  const VkImage sourceImage = headset->getRenderTarget(swapchainImageIndex)->getImage();
        //  const VkImage destinationImage = mirrorSwapchainImages.at(currentImageIndex); // [t] we have these from createMirrorSwapchain()
        //  const VkExtent2D eyeResolution = headset->getEyeResolution(eyeIndex);

          // [t] For vulkan, the steps should be smth like this:
          
          //  Convert source image from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
          //    vkCmdPipelineBarrier with command buffer
//
          //  Convert destination image from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
          //    vkCmdPipelineBarrier with command buffer
//
          //  match source image eye aspect ratio to window / crop
//
          //  VkImageBlit vkCmdBlitImage the source image to destination image
//
          //  Convert source image from VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
//
          //  Convert destination image from VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
//
          //  present()
//
          //
        //}


        // Render Mirror View
        // if we want to render this eye,
        //mirrorView render: OpenXRMirrorView::render(headset, eyeIndex, swapchainImageIndex)
        //[t] TODO: Also, render() should not be in that struct.
        // if window is visible and render success:
        // mirrorView present: OpenXRMirrorView::present()

        headset->releaseSwapchain(i);
      }
    }
    // after you rendered and released all swapchains
    if (frameResult == Headset::BeginFrameResult::RenderFully || frameResult == Headset::BeginFrameResult::SkipRender)
    {
      headset->endFrame();
    }
  }

  context_xr->sync(); // Sync before destroying so that resources are free
  return EXIT_SUCCESS;
}*/

OpenXRSystem::~OpenXRSystem(){
  spdlog::info("[log][t] OpenXRSystem::~OpenXRSystem()");
}






