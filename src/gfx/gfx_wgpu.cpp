#include "gfx_wgpu.hpp"
#include "stdio.h"

WGPULimits wgpuGetUndefinedLimits();
WGPULimits wgpuGetDefaultLimits() {
#if WEBGPU_NATIVE
  return WGPULimits{}; // 0 = unset/use default
#else
  return wgpuGetUndefinedLimits(); // WGPU_LIMIT_UXX_UNDEFINED = unset/use default
#endif
}

WGPULimits wgpuGetUndefinedLimits() {
  return WGPULimits{
      .maxTextureDimension1D = WGPU_LIMIT_U32_UNDEFINED,
      .maxTextureDimension2D = WGPU_LIMIT_U32_UNDEFINED,
      .maxTextureDimension3D = WGPU_LIMIT_U32_UNDEFINED,
      .maxTextureArrayLayers = WGPU_LIMIT_U32_UNDEFINED,
      .maxBindGroups = WGPU_LIMIT_U32_UNDEFINED,
      .maxDynamicUniformBuffersPerPipelineLayout = WGPU_LIMIT_U32_UNDEFINED,
      .maxDynamicStorageBuffersPerPipelineLayout = WGPU_LIMIT_U32_UNDEFINED,
      .maxSampledTexturesPerShaderStage = WGPU_LIMIT_U32_UNDEFINED,
      .maxSamplersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED,
      .maxStorageBuffersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED,
      .maxStorageTexturesPerShaderStage = WGPU_LIMIT_U32_UNDEFINED,
      .maxUniformBuffersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED,
      .maxUniformBufferBindingSize = WGPU_LIMIT_U64_UNDEFINED,
      .maxStorageBufferBindingSize = WGPU_LIMIT_U64_UNDEFINED,
      .minUniformBufferOffsetAlignment = WGPU_LIMIT_U32_UNDEFINED,
      .minStorageBufferOffsetAlignment = WGPU_LIMIT_U32_UNDEFINED,
      .maxVertexBuffers = WGPU_LIMIT_U32_UNDEFINED,
      .maxVertexAttributes = WGPU_LIMIT_U32_UNDEFINED,
      .maxVertexBufferArrayStride = WGPU_LIMIT_U32_UNDEFINED,
      .maxInterStageShaderComponents = WGPU_LIMIT_U32_UNDEFINED,
      .maxComputeWorkgroupStorageSize = WGPU_LIMIT_U32_UNDEFINED,
      .maxComputeInvocationsPerWorkgroup = WGPU_LIMIT_U32_UNDEFINED,
      .maxComputeWorkgroupSizeX = WGPU_LIMIT_U32_UNDEFINED,
      .maxComputeWorkgroupSizeY = WGPU_LIMIT_U32_UNDEFINED,
      .maxComputeWorkgroupSizeZ = WGPU_LIMIT_U32_UNDEFINED,
      .maxComputeWorkgroupsPerDimension = WGPU_LIMIT_U32_UNDEFINED,
  };
}

void gfxWgpuDeviceGetLimits(WGPUDevice device, WGPUSupportedLimits *outLimits) {
#ifdef WEBGPU_NATIVE
  wgpuDeviceGetLimits(device, outLimits);
#else
  // NOTE: EMSCRIPTEN Doesn't implement limits at this moment, assume default everywhere
  outLimits->limits = wgpuGetDefaultLimits();
#endif
}
