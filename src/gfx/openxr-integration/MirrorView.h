#pragma once

#include <vulkan/vulkan.h>

#include <vector>

class Context_XR;
struct GLFWwindow;
class Headset;
class Renderer;

class MirrorView final
{
public:
  MirrorView(const Context_XR* context);
  ~MirrorView();

  void onWindowResize();

  bool connect(const Headset* headset, const Renderer* renderer);
  void processWindowEvents() const;

  enum class RenderResult
  {
    Error,    // An error occurred
    Visible,  // Visible mirror view for normal rendering
    Invisible // Minimized window for example without rendering
  };
  RenderResult render(uint32_t swapchainImageIndex);
  void present();

  bool isValid() const;
  bool isExitRequested() const;
  VkSurfaceKHR getSurface() const;

private:
  bool valid = true;

  const Context_XR* context = nullptr;
  const Headset* headset = nullptr;
  const Renderer* renderer = nullptr;

  GLFWwindow* window = nullptr;

  VkSurfaceKHR surface = nullptr;
  VkSwapchainKHR swapchain = nullptr;
  std::vector<VkImage> swapchainImages;
  VkExtent2D swapchainResolution = { 0u, 0u };

  uint32_t destinationImageIndex = 0u;
  bool resizeDetected = false;

  bool recreateSwapchain();
};