#ifndef C33C5C44_2FC2_44EE_B4D3_B25A714E24C6
#define C33C5C44_2FC2_44EE_B4D3_B25A714E24C6

#include "../pipeline_step.hpp"
#include "../features/transform.hpp"

namespace gfx::steps {
template <typename... TFeatures> inline std::vector<FeaturePtr> withDefaultFullscreenFeatures(TFeatures &&...features) {
  return std::vector<FeaturePtr>{
      features::Transform::create(false, false),
      features...,
  };
}

// Color output with optional clear color
inline RenderStepOutput::Named getDefaultColorOutput(std::optional<float4> clearColor = std::nullopt) {
  return RenderStepOutput::Named("color", WGPUTextureFormat_BGRA8UnormSrgb,
                                 clearColor ? std::make_optional(ClearValues::getColorValue(clearColor.value())) : std::nullopt);
}

// Depth output with optional clear
inline RenderStepOutput::Named getDefaultDepthOutput(bool clearDepth = false) {
  return RenderStepOutput::Named("depth", WGPUTextureFormat_Depth32Float,

                                 clearDepth ? std::make_optional(ClearValues::getDefaultDepthStencil()) : std::nullopt);
}

// Default output color+depth with optional clear
inline RenderStepOutput getDefaultRenderStepOutput(bool clearDepth = false, std::optional<float4> clearColor = std::nullopt) {
  RenderStepOutput out;
  out.attachments.push_back(getDefaultColorOutput(clearColor));
  out.attachments.push_back(getDefaultDepthOutput(clearDepth));
  return out;
}
} // namespace gfx::steps

#endif /* C33C5C44_2FC2_44EE_B4D3_B25A714E24C6 */
