#include "context.hpp"
#include "Context_XR.h"
#include "Headset.h"



//[t] TODO: figure out where to call this from. Not sure how the general flow is outside of context.cpp
int openXRMain(gfx::Context gfxContext)
{
  Context_XR context;
  if (!context.isValid())
  {
    return EXIT_FAILURE;
  }

  //[t] checkXRDeviceReady can be called at any time to check if a certain headset is connected.
                            //[t] requires openxr.h
  context.checkXRDeviceReady(XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 
                            XR_ENVIRONMENT_BLEND_MODE_OPAQUE, 
                            XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY);
  
  // maybe move some of this to gfx
  context.getVulkanExtensionsFromOpenXRInstance();
  //[t] TODO: Our WGPUVulkanShared for the purposes of using XR, doesn't need to be created unless the ^above XR specific setup succeeds.
  
  //[t] Create Mirror View in / with the gfx context
  //[t] For us it's done by context.cpp
  
  //[t] TODO: We also need to verify that VulkanOpenXRSwapchain was successful at CreateMirrorSurface() and createMirrorSwapchain() 
  //          for queue, surface, to know if we can render & present
  //    So if context.cpp Context::deviceObtained() passed.

  bool isMultipass = true;

  Headset headset(&context, &gfxContext, isMultipass);
  if (!headset.isValid())
  {
    return EXIT_FAILURE;
  }


  //[t] Main loop. TODO: Guus: Where do we integrate this sort of thing?
  while (!headset.isExitRequested() )//&& !mirrorView.isExitRequested())
  {

    //[t] Guus: process main window events? -- I assume we already do this, where?
    /*
      struct MainWindow : public Base {
      window->pollEvents(events);
      // openxr events here
    */


    //[t] If we run in isMultipass (render twice), then we have 2 swapchains.
    //[t] If we run in single pass (multiview), then we have 1 swapchain with 2 layers
    //[t] The headset has an array of swapchain images, and each one has a vukan swapchainImageIndex
    //[t] So each time you acquireSwapchainForFrame, you get back the swapchainImageIndex for the swapchain you're working with.
    uint32_t swapchainImageIndex;
    
    Headset::BeginFrameResult frameResult = headset.beginFrame();
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
      frameResult = headset.acquireSwapchainForFrame(i, swapchainImageIndex);

      if (frameResult == Headset::BeginFrameResult::Error)
      {
        return EXIT_FAILURE;
      }
      else
      if (frameResult == Headset::BeginFrameResult::RenderFully)
      {
        //[t] TODO: guus: how to tell renderer to handle RenderTarget's differently if
        //[t] is multipass vs if is multiview singlepass?
        //[t] The swapchainImageIndex points to a rendertarget, so we can get the frame buffer from it.
        //[t] i = what pass we are on (one per eye) in the case we are multi-pass
        /*
          So in renderer we should have a wgpu equivalent of:
            VkRenderPassBeginInfo renderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
            renderPassBeginInfo.renderPass = headset->getRenderPass();
            renderPassBeginInfo.framebuffer = headset->getRenderTarget(swapchainImageIndex)->getFramebuffer();
            renderPassBeginInfo.renderArea.offset = { 0, 0 };
            renderPassBeginInfo.renderArea.extent = headset->getEyeResolution(0u);
            renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassBeginInfo.pClearValues = clearValues.data();
        */
        //renderer.render(i, swapchainImageIndex);
          // eye data
          // swapchains
          // also the mirror view


        // Render Mirror View
        // if we want to render this eye,
        //mirrorView render: VulkanOpenXRSwapchain::render(headset, eyeIndex, swapchainImageIndex)
        //[t] TODO: Also, render() should not be in that struct.
        // if window is visible and render success:
        // mirrorView present: VulkanOpenXRSwapchain::present()

        headset.releaseSwapchain(i);
      }
    }
    // after you rendered and released all swapchains
    if (frameResult == Headset::BeginFrameResult::RenderFully || frameResult == Headset::BeginFrameResult::SkipRender)
    {
      headset.endFrame();
    }
  }

  context.sync(); // Sync before destroying so that resources are free
  return EXIT_SUCCESS;
}

