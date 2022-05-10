#include "view.hpp"
#include "linalg/linalg.h"
#include <type_traits>

namespace gfx {
View::View() : view(linalg::identity), proj(linalg::identity) {}

float4x4 View::getProjectionMatrix(const int2 &viewSize) const {
  return std::visit(
      [&](auto &&arg) -> float4x4 {
        float aspectRatio = float(viewSize.x) / float(viewSize.y);

        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, float4x4>) {
          return arg;
        } else if constexpr (std::is_same_v<T, ViewPerspectiveProjection>) {
          float fovY = arg.fov;
          if (arg.fovType == FovDirection::Horizontal) {
            fovY = arg.fov / aspectRatio;
          }
          return linalg::perspective_matrix(fovY, aspectRatio, arg.near, arg.far, linalg::neg_z, linalg::zero_to_one);
        } else if constexpr (std::is_same_v<T, ViewOrthographicProjection>) {
          float2 orthoSize = float2(viewSize);
          if (arg.sizeType == OrthographicSizeType::Vertical) {
            orthoSize.x = arg.size * aspectRatio;
            orthoSize.y = arg.size;
          } else if (arg.sizeType == OrthographicSizeType::Horizontal) {
            orthoSize.x = arg.size;
            orthoSize.y = arg.size / aspectRatio;
          } else if (arg.sizeType == OrthographicSizeType::PixelScale) {
            orthoSize.x = viewSize.x * arg.size;
            orthoSize.y = viewSize.y * arg.size;
          }

          float4x4 mat = linalg::identity;
          float depthRange = arg.far - arg.near;
          mat = mul(mat, linalg::scaling_matrix(float3(2.0f / orthoSize.x, 2.0f / orthoSize.y, -1.0f / depthRange)));
          mat = mul(mat, linalg::translation_matrix(float3(0.0f, 0.0f, arg.near)));
          return mat;
        } else {
          return linalg::identity;
        }
      },
      proj);
}
} // namespace gfx
