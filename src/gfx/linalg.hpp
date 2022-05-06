#ifndef GFX_LINALG
#define GFX_LINALG

#include <linalg/linalg.h>

namespace gfx {
using namespace linalg::aliases;

// tighlty packs the matrix into dst with sizeof(float)*16
inline void packFloat4x4(const float4x4 &mat, float *dst) {
  for (int i = 0; i < 4; i++) {
    memcpy(&dst[i * 4], &mat[i].x, sizeof(float) * 4);
  }
}

} // namespace gfx

#endif // GFX_LINALG
