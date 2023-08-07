#ifndef SHARDS_GFX_RUST_BINDINGS_HPP
#define SHARDS_GFX_RUST_BINDINGS_HPP

#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>
#include <wgpu.h>

namespace gfx {

struct GFXShaderModuleDescriptor {
  const char *label;
  const char *wgsl;
};

struct GFXShaderModuleResult {
  WGPUShaderModule module;
  const char *error;
};

struct GFXRenderPipelineResult {
  WGPURenderPipeline pipeline;
  const char *error;
};

struct GFXComputePipelineResult {
  WGPUComputePipeline pipeline;
  const char *error;
};

extern "C" {

void gfxDeviceCreateShaderModule(WGPUDevice device,
                                 const GFXShaderModuleDescriptor *descriptor,
                                 void (*callback)(const GFXShaderModuleResult*, void*),
                                 void *user_data);

void gfxDeviceCreateRenderPipeline(WGPUDevice device,
                                   const WGPURenderPipelineDescriptor *descriptor,
                                   void (*callback)(const GFXRenderPipelineResult*, void*),
                                   void *user_data);

void gfxDeviceCreateComputePipeline(WGPUDevice device,
                                    const WGPUComputePipelineDescriptor *descriptor,
                                    void (*callback)(const GFXComputePipelineResult*, void*),
                                    void *user_data);

void gfxTracyInit();

} // extern "C"

} // namespace gfx

#endif // SHARDS_GFX_RUST_BINDINGS_HPP
