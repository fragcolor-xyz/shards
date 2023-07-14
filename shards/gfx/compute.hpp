#ifndef B60F1349_17C2_4CCD_8686_3AFAE011029E
#define B60F1349_17C2_4CCD_8686_3AFAE011029E

#include <params.hpp>

namespace gfx {
struct BufferDescriptor {
  // The fiels in the buffer
  std::vector<ShaderParameter> params;
  // Number of elements in the buffer
  // If not set, the buffer will be dynamically sized
  std::optional<size_t> length = 1;
};
} // namespace gfx

#endif /* B60F1349_17C2_4CCD_8686_3AFAE011029E */
