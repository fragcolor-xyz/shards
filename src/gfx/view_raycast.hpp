#ifndef B90FDE21_8B17_4891_A2CA_72048DE03308
#define B90FDE21_8B17_4891_A2CA_72048DE03308

#include "linalg.hpp"
#include "view.hpp"
#include "fwd.hpp"

namespace gfx {

struct ViewRay {
  float3 origin;
  float3 direction;
};

struct SizedView {
  float2 size;
  float4x4 viewProj;
  float4x4 viewProjInv;
  float4x4 view;
  float4x4 viewInv;

  SizedView() = default;
  SizedView(ViewPtr view, float2 size) : size(size) {
    viewProj = linalg::mul(view->getProjectionMatrix(size), view->view);
    viewProjInv = linalg::inverse(viewProj);
    this->view = view->view;
    viewInv = linalg::inverse(view->view);
  }

  ViewRay getRay(float2 viewPosition) const {
    ViewRay result;

    float2 ndc2 = float2(viewPosition) / float2(size);
    ndc2 = ndc2 * 2.0f - 1.0f;
    ndc2.y = -ndc2.y;
    float4 ndc = float4(ndc2, 0.1f, 1.0f);

    float4 unprojected = linalg::mul(viewProjInv, ndc);
    unprojected /= unprojected.w;

    result.origin = viewInv.w.xyz();
    result.direction = linalg::normalize(unprojected.xyz() - result.origin);

    return result;
  }
};

} // namespace gfx

#endif /* B90FDE21_8B17_4891_A2CA_72048DE03308 */
