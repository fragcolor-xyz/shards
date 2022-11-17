#include "context.hpp"
#include "Context_XR.h"
#include "Headset.h"
#include "MirrorView.h"
#include "Renderer.h"


//[t] TODO: figure out where to call this from. Not sure how the general flow is outside of context.cpp
int openXRMain(gfx::Context gfxContext)
{
  Context_XR context;
  if (!context.isValid())
  {
    return EXIT_FAILURE;
  }
  
  /*
  MirrorView mirrorView(&context);
  if (!mirrorView.isValid())
  {
    return EXIT_FAILURE;
  }

  if (!context.createDevice(mirrorView.getSurface()))//mirrorSurface only used to check compatibility
  {
    return EXIT_FAILURE;
  }
  */

  Headset headset(&context, &gfxContext);
  if (!headset.isValid())
  {
    return EXIT_FAILURE;
  }

  //[t] TODO: connect contexts & headset to our Renderer...
  //[t] TODO: connect headset and renderer to Mirror View

  //[t] Main loop. TODO: Guus: Where do we integrate this sort of thing?
  while (!headset.isExitRequested() )//&& !mirrorView.isExitRequested())
  {
    //mirrorView.processWindowEvents();

    uint32_t swapchainImageIndex;// this should be for one multiview swapchain image
    const Headset::BeginFrameResult frameResult = headset.beginFrame(swapchainImageIndex);
    if (frameResult == Headset::BeginFrameResult::Error)
    {
      return EXIT_FAILURE;
    }
    else if (frameResult == Headset::BeginFrameResult::RenderFully)
    {
      renderer.render(swapchainImageIndex);
      
      /*
      const MirrorView::RenderResult mirrorResult = mirrorView.render(swapchainImageIndex);
      if (mirrorResult == MirrorView::RenderResult::Error)
      {
        return EXIT_FAILURE;
      }

      const bool mirrorViewVisible = (mirrorResult == MirrorView::RenderResult::Visible);
      renderer.submit(mirrorViewVisible);

      if (mirrorViewVisible)
      {
        mirrorView.present();
      }
      */
    }

    if (frameResult == Headset::BeginFrameResult::RenderFully || frameResult == Headset::BeginFrameResult::SkipRender)
    {
      headset.endFrame();
    }
  }

  context.sync(); // Sync before destroying so that resources are free
  return EXIT_SUCCESS;
}
