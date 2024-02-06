#include "gfx_wgpu.hpp"
#include "stdio.h"

WGPULimits wgpuGetUndefinedLimits();
WGPULimits wgpuGetDefaultLimits() {
  return wgpuGetUndefinedLimits(); // WGPU_LIMIT_UXX_UNDEFINED = unset/use default
}

WGPULimits wgpuGetUndefinedLimits() {
  return WGPULimits{
      .maxTextureDimension1D = WGPU_LIMIT_U32_UNDEFINED,
      .maxTextureDimension2D = WGPU_LIMIT_U32_UNDEFINED,
      .maxTextureDimension3D = WGPU_LIMIT_U32_UNDEFINED,
      .maxTextureArrayLayers = WGPU_LIMIT_U32_UNDEFINED,
      .maxBindGroups = WGPU_LIMIT_U32_UNDEFINED,
      .maxBindGroupsPlusVertexBuffers = WGPU_LIMIT_U32_UNDEFINED,
#ifdef WEBGPU_NATIVE
      .maxBindingsPerBindGroup = WGPU_LIMIT_U32_UNDEFINED,
#endif
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
#ifdef WEBGPU_NATIVE
      .maxBufferSize = WGPU_LIMIT_U32_UNDEFINED,
#endif
      .maxVertexAttributes = WGPU_LIMIT_U32_UNDEFINED,
      .maxVertexBufferArrayStride = WGPU_LIMIT_U32_UNDEFINED,
      .maxInterStageShaderComponents = WGPU_LIMIT_U32_UNDEFINED,
      .maxInterStageShaderVariables = WGPU_LIMIT_U32_UNDEFINED,
      .maxColorAttachments = WGPU_LIMIT_U32_UNDEFINED,
#ifdef WEBGPU_NATIVE
      .maxColorAttachmentBytesPerSample = WGPU_LIMIT_U32_UNDEFINED,
#endif
      .maxComputeWorkgroupStorageSize = WGPU_LIMIT_U32_UNDEFINED,
      .maxComputeInvocationsPerWorkgroup = WGPU_LIMIT_U32_UNDEFINED,
      .maxComputeWorkgroupSizeX = WGPU_LIMIT_U32_UNDEFINED,
      .maxComputeWorkgroupSizeY = WGPU_LIMIT_U32_UNDEFINED,
      .maxComputeWorkgroupSizeZ = WGPU_LIMIT_U32_UNDEFINED,
      .maxComputeWorkgroupsPerDimension = WGPU_LIMIT_U32_UNDEFINED,
  };
}

void gfxWgpuDeviceGetLimits(WGPUDevice device, WGPUSupportedLimits *outLimits) { wgpuDeviceGetLimits(device, outLimits); }
