#ifndef IMAGING_H
#define IMAGING_H

#include "shared.hpp"

namespace shards {
namespace Imaging {

// Premultiplies the alpha channel in input image and writes the resulting image to a given SHVar output
// Assumes that both SHVars are images. Output's data must be pre-allocated before calling this function
// Adds SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA flag to output image
template <typename T> void premultiplyAlpha(const SHVar &input, SHVar &output, int32_t w, int32_t h) {
  const auto from = reinterpret_cast<T *>(input.payload.imageValue.data);
  auto to = reinterpret_cast<T *>(output.payload.imageValue.data);

  premultiplyAlpha<T>(from, to, w, h);

  // mark as premultiplied
  output.payload.imageValue.flags |= SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA;
}

// Premultiplies the alpha channel in input image and writes the resulting image to a given vector of bytes
// bytes's data must be pre-allocated before calling htis function
// Does not add SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA flag to output image
template <typename T> void premultiplyAlpha(const SHVar &input, std::vector<uint8_t> &bytes, int32_t w, int32_t h) {
  const auto from = reinterpret_cast<T *>(input.payload.imageValue.data);
  auto to = reinterpret_cast<T *>(&bytes[0]);

  premultiplyAlpha<T>(from, to, w, h);
}

template <typename T> inline void premultiplyAlpha(T *from, T *to, int32_t w, int32_t h) {
  const auto max = std::numeric_limits<T>::max(); // set to respective max value for uint8_t uint16_t and float
  for (auto y = 0; y < h; y++) {
    for (auto x = 0; x < w; x++) {
      const auto addr = ((w * y) + x) * 4;
      // premultiply RGB values
      for (auto z = 0; z < 3; z++) {
        // do calculation in float for better accuracy
        to[addr + z] = static_cast<T>(static_cast<float>(from[addr + z]) / max * from[addr + 3]);
      }
      // copy A value
      to[addr + 3] = from[addr + 3];
    }
  }
}

// Premultiplies the alpha channel in input image and writes the resulting image to a given SHVar output
// Assumes that both SHVars are images. Output's data must be pre-allocated before calling this function
// Adds SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA flag to output image
template <typename T> void demultiplyAlpha(const SHVar &input, SHVar &output, int32_t w, int32_t h) {
  const auto from = reinterpret_cast<T *>(input.payload.imageValue.data);
  auto to = reinterpret_cast<T *>(output.payload.imageValue.data);

  demultiplyAlpha<T>(from, to, w, h);

  // remove premultiplied flag
  output.payload.imageValue.flags &= ~SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA;
}

// Premultiplies the alpha channel in input image and writes the resulting image to a given vector of bytes
// bytes's data must be pre-allocated before calling htis function
// Does not add SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA flag to output image
template <typename T> void demultiplyAlpha(const SHVar &input, std::vector<uint8_t> &bytes, int32_t w, int32_t h) {
  const auto from = reinterpret_cast<T *>(input.payload.imageValue.data);
  auto to = reinterpret_cast<T *>(&bytes[0]);

  demultiplyAlpha<T>(from, to, w, h);
}

template <typename T> inline void demultiplyAlpha(T *from, T *to, int32_t w, int32_t h) {
  const auto max = std::numeric_limits<T>::max(); // set to respective max value for uint8_t uint16_t and float

  for (auto y = 0; y < h; y++) {
    for (auto x = 0; x < w; x++) {
      const auto addr = ((w * y) + x) * 4;
      // un-premultiply RGB values
      for (auto z = 0; z < 3; z++) {
        to[addr + z] = static_cast<T>(static_cast<float>(from[addr + z]) / from[addr + 3] * max);
      }
      // copy A value
      to[addr + 3] = from[addr + 3];
    }
  }
}
} // namespace Imaging
} // namespace shards

#endif // IMAGING_H