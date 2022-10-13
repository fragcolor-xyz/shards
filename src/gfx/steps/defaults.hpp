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

// Color output WITHOUT clear
inline RenderStepIO::NamedOutput getDefaultColorOutput(std::optional<float4> clearColor = std::nullopt) {
  return RenderStepIO::NamedOutput{
      .name = "color",
      .format = WGPUTextureFormat_BGRA8UnormSrgb,
      .clearValues = clearColor ? std::make_optional(ClearValues::getColorValue(clearColor.value())) : std::nullopt,
  };
}

// Depth output with clear
inline RenderStepIO::NamedOutput getDefaultDepthOutput(bool clearDepth = true) {
  return RenderStepIO::NamedOutput{
      .name = "depth",
      .format = WGPUTextureFormat_Depth32Float,
      .clearValues = clearDepth ? std::make_optional(ClearValues::getDefaultDepthStencil()) : std::nullopt,
  };
}

// Default output color+depth with clear
inline RenderStepIO getDefaultDrawPassIO(bool clearDepth = true, std::optional<float4> clearColor = float4(0, 0, 0, 0)) {
  return RenderStepIO{
      .outputs =
          {
              getDefaultColorOutput(clearColor),
              getDefaultDepthOutput(clearDepth),
          },
  };
}
} // namespace gfx::steps

#endif /* C33C5C44_2FC2_44EE_B4D3_B25A714E24C6 */
