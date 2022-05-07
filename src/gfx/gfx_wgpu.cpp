#include "gfx_wgpu.hpp"
#include "stdio.h"

WGPULimits wgpuGetDefaultLimits() {
  return WGPULimits{
      .maxTextureDimension1D = 0,
      .maxTextureDimension2D = 0,
      .maxTextureDimension3D = 0,
      .maxTextureArrayLayers = 0,
      .maxBindGroups = 0,
      .maxDynamicUniformBuffersPerPipelineLayout = 0,
      .maxDynamicStorageBuffersPerPipelineLayout = 0,
      .maxSampledTexturesPerShaderStage = 0,
      .maxSamplersPerShaderStage = 0,
      .maxStorageBuffersPerShaderStage = 0,
      .maxStorageTexturesPerShaderStage = 0,
      .maxUniformBuffersPerShaderStage = 0,
      .maxUniformBufferBindingSize = 0,
      .maxStorageBufferBindingSize = 0,
      .minUniformBufferOffsetAlignment = 0,
      .minStorageBufferOffsetAlignment = 0,
      .maxVertexBuffers = 0,
      .maxVertexAttributes = 0,
      .maxVertexBufferArrayStride = 0,
      .maxInterStageShaderComponents = 0,
      .maxComputeWorkgroupStorageSize = 0,
      .maxComputeInvocationsPerWorkgroup = 0,
      .maxComputeWorkgroupSizeX = 0,
      .maxComputeWorkgroupSizeY = 0,
      .maxComputeWorkgroupSizeZ = 0,
      .maxComputeWorkgroupsPerDimension = 0,
  };
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
