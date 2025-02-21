#ifndef DA31BE00_8A3F_4717_B613_6619BC51EADD
#define DA31BE00_8A3F_4717_B613_6619BC51EADD

#include "linalg.hpp"
#include "view.hpp"

namespace gfx {

struct ScreenSizeHelper {
  gfx::View &view;
  float2 viewportSize;
  float scalingFactor = 1.0f;
  mutable std::optional<float4x4> viewProjMatrix;

  float getConstantScreenSize(float3 position, float size) const {
    float4 projected = linalg::mul(view.view, float4(position, 1.0f));
    projected /= projected.w;

    float4x4 projMatrix = view.getProjectionMatrix(viewportSize);
    float minPerspective = projMatrix[1][1];

    // Scaling factor to make object 100% vertical size on screen
    float distanceFromCamera = std::abs(projected.z);
    float scalingFactor1 = distanceFromCamera / minPerspective;

    // Adjust for desired size
    float yRatio = (size * this->scalingFactor) / viewportSize.y;
    return scalingFactor1 * yRatio * 2.0f;
  }

  // New method to calculate pixel span of a 3D line segment
  float getLineSegmentPixelSpan(float3 startPosition, float3 direction) const {
    if (!viewProjMatrix) {
      viewProjMatrix.emplace(linalg::mul(view.getProjectionMatrix(viewportSize), view.view));
    }
    // Project the start position into screen space
    float4 startProjected = linalg::mul(*viewProjMatrix, float4(startPosition, 1.0f));
    startProjected /= startProjected.w;

    // Calculate the end position of the line segment
    float3 endPosition = startPosition + direction;

    // Project the end position into screen space
    float4 endProjected = linalg::mul(*viewProjMatrix, float4(endPosition, 1.0f));
    endProjected /= endProjected.w;

    // Calculate the difference in screen space
    float2 screenSpaceDiff = float2(endProjected.x - startProjected.x, endProjected.y - startProjected.y);

    // Convert the screen space difference to pixel space
    float2 pixelSpaceDiff = screenSpaceDiff * viewportSize * 0.5f;

    // Return the length of the vector in pixel space
    return linalg::length(pixelSpaceDiff);
  }
};
} // namespace gfx

#endif /* DA31BE00_8A3F_4717_B613_6619BC51EADD */
