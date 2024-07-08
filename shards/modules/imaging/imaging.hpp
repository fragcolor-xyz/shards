#ifndef B426C31C_17B4_49E6_99FD_7387DBE57320
#define B426C31C_17B4_49E6_99FD_7387DBE57320

#include <shards/core/shared.hpp>

namespace shards {
namespace Imaging {

template <typename T> void premultiplyAlpha(T *from, T *to, int32_t w, int32_t h, int32_t rowStride) {
  const auto max = std::numeric_limits<T>::max(); // set to respective max value for uint8_t uint16_t and float
  for (auto y = 0; y < h; y++) {
    T *rowFrom = reinterpret_cast<T *>(reinterpret_cast<uint8_t *>(from) + rowStride * y);
    for (auto x = 0; x < w; x++) {
      const auto srcAddr = x * 4;
      const auto dstAddr = ((w * y) + x) * 4;
      // premultiply RGB values
      for (auto z = 0; z < 3; z++) {
        // do calculation in float for better accuracy
        to[dstAddr + z] = static_cast<T>(static_cast<float>(rowFrom[srcAddr + z]) / max * rowFrom[srcAddr + 3]);
      }
      // copy A value
      to[dstAddr + 3] = rowFrom[srcAddr + 3];
    }
  }
}

// Premultiplies the alpha channel in input image and writes to a pre-allocated output.
// Assumes input & output are CoreInfo::ImageType. Output flags is set to input's + SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA flag
template <typename T> void premultiplyAlpha(const SHVar &input, SHVar &output, int32_t w, int32_t h, int32_t rowStride) {
  const auto from = reinterpret_cast<T *>(input.payload.imageValue->data);
  auto to = reinterpret_cast<T *>(output.payload.imageValue->data);

  premultiplyAlpha<T>(from, to, w, h, rowStride);

  // mark as premultiplied
  output.payload.imageValue->flags = input.payload.imageValue->flags;
  output.payload.imageValue->flags |= SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA;
}

// Premultiplies the alpha channel in input image and writes to a pre-allocated bytes buffer.
// Assumes input & output are CoreInfo::ImageType. Does NOT add SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA flag
template <typename T>
void premultiplyAlpha(const SHVar &input, std::vector<uint8_t> &bytes, int32_t w, int32_t h, int32_t rowStride) {
  const auto from = reinterpret_cast<T *>(input.payload.imageValue->data);
  auto to = reinterpret_cast<T *>(&bytes[0]);

  premultiplyAlpha<T>(from, to, w, h, rowStride);
}

template <typename T> void demultiplyAlpha(T *from, T *to, int32_t w, int32_t h, int32_t rowStride) {
  const auto max = std::numeric_limits<T>::max(); // set to respective max value for uint8_t uint16_t and float

  for (auto y = 0; y < h; y++) {
    T *rowFrom = reinterpret_cast<T *>(reinterpret_cast<uint8_t *>(from) + rowStride * y);
    for (auto x = 0; x < w; x++) {
      const auto srcAddr = x * 4;
      const auto dstAddr = ((w * y) + x) * 4;
      // un-premultiply RGB values
      if (rowFrom[srcAddr + 3] == 0.0f) {
        // If alpha is zero, set RGB to zero
        for (auto z = 0; z < 3; z++) {
          to[dstAddr + z] = static_cast<T>(0);
        }
      } else {
        for (auto z = 0; z < 3; z++) {
          to[dstAddr + z] = static_cast<T>(static_cast<float>(rowFrom[srcAddr + z]) / rowFrom[srcAddr + 3] * max);
        }
      }
      // copy A value
      to[dstAddr + 3] = rowFrom[srcAddr + 3];
    }
  }
}

// Demultiplies the alpha channel in input image and writes to a pre-allocated output.
// Assumes input & output are CoreInfo::ImageType. Output flags is set to input's without SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA flag
template <typename T> void demultiplyAlpha(const SHVar &input, SHVar &output, int32_t w, int32_t h, int32_t rowStride) {
  const auto from = reinterpret_cast<T *>(input.payload.imageValue->data);
  auto to = reinterpret_cast<T *>(output.payload.imageValue->data);

  demultiplyAlpha<T>(from, to, w, h, rowStride);

  // remove premultiplied flag
  output.payload.imageValue->flags = input.payload.imageValue->flags;
  output.payload.imageValue->flags &= ~SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA;
}

// Demultiplies the alpha channel in input image and writes the resulting image to a given vector of bytes
// bytes's data must be pre-allocated before calling htis function
// Does not add SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA flag to output image
template <typename T>
void demultiplyAlpha(const SHVar &input, std::vector<uint8_t> &bytes, int32_t w, int32_t h, int32_t rowStride) {
  const auto from = reinterpret_cast<T *>(input.payload.imageValue->data);
  auto to = reinterpret_cast<T *>(&bytes[0]);

  demultiplyAlpha<T>(from, to, w, h, rowStride);
}
} // namespace Imaging
} // namespace shards

#endif /* B426C31C_17B4_49E6_99FD_7387DBE57320 */
