#ifndef BAF56650_63E5_4A55_854E_87DAABD00E99
#define BAF56650_63E5_4A55_854E_87DAABD00E99

#include "gfx_wgpu.hpp"
#include "wgpu_handle.hpp"
#include <optional>
#include <stdexcept>

namespace gfx {
inline WgpuHandle<WGPUShaderModule> compileShaderFromWgsl(WGPUDevice device, const char *wgsl) {
#if WEBGPU_NATIVE
  GFXShaderModuleDescriptor desc{
      .wgsl = wgsl,
  };
  struct Result {
    WgpuHandle<WGPUShaderModule> handle;
    std::optional<std::runtime_error> ex;
  } result;
  gfxDeviceCreateShaderModule(
      device, &desc, //
      [](const GFXShaderModuleResult *result, void *ud) {
        ((Result *)ud)->handle.reset(result->module);
        if (result->error) {
          ((Result *)ud)->ex.emplace(result->error);
        }
        //
      },
      &result);

  if (result.ex) {
    throw *result.ex;
  }

  return std::move(result.handle);
#else
  WGPUShaderModuleDescriptor moduleDesc = {};
  WGPUShaderModuleWGSLDescriptor wgslModuleDesc = {};
  moduleDesc.label = "pipeline";
  moduleDesc.nextInChain = &wgslModuleDesc.chain;

  wgslModuleDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
  wgslModuleDesc.code = wgsl;

  WGPUShaderModule module = wgpuDeviceCreateShaderModule(device, &moduleDesc);
  if (!module) {
    throw std::runtime_error("Failed to create shader module");
  }
  return WgpuHandle<WGPUShaderModule>(module);
#endif
}
} // namespace gfx

#endif /* BAF56650_63E5_4A55_854E_87DAABD00E99 */
