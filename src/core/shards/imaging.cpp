/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

#ifndef IMAGING_H
#define IMAGING_H

#include "shared.hpp"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize.h>

namespace shards {
namespace Imaging {
struct Convolve {
  static SHTypesInfo inputTypes() { return CoreInfo::ImageType; }
  static SHTypesInfo outputTypes() { return CoreInfo::ImageType; }

  static inline Parameters _params{{"Radius",
                                    SHCCSTR("The radius of the kernel, e.g. 1 = 1x1; 2 = 3x3; 3 = 5x5 and "
                                            "so on."),
                                    {CoreInfo::IntType}},
                                   {"Step", SHCCSTR("How many pixels to advance each activation."), {CoreInfo::IntType}}};

  static SHParametersInfo parameters() { return _params; }

  SHVar getParam(int index) {
    if (index == 0)
      return Var(int64_t(_radius));
    else
      return Var(int64_t(_step));
  }

  void setParam(int index, const SHVar &value) {
    if (index == 0) {
      _radius = int32_t(value.payload.intValue);
      if (_radius <= 0)
        _radius = 1;
      _kernel = (_radius - 1) * 2 + 1;
    } else {
      _step = int32_t(value.payload.intValue);
      if (_step <= 0)
        _step = 1;
    }
  }

  void warmup(SHContext *context) {
    _bytes.reserve(_kernel * _kernel * 4); // assume max 4 channels
  }

  void cleanup() {
    _xindex = 0;
    _yindex = 0;
  }

  template <typename T> void process(const SHVar &pixels, int32_t w, int32_t h, int32_t c) {
    const int high = _radius - 1;
    const int low = high * -1;
    int index = 0;
    const auto from = reinterpret_cast<T *>(pixels.payload.imageValue.data);
    auto to = reinterpret_cast<T *>(&_bytes[0]);
    for (int y = low; y <= high; y++) {
      for (int x = low; x <= high; x++) {
        const int cidxx = _xindex + x;
        const int cidxy = _yindex + y;
        const auto idxx = std::clamp<int>(cidxx, 0, w - 1);
        const auto idxy = std::clamp<int>(cidxy, 0, h - 1);
        const int addr = ((w * idxy) + idxx) * c;
        for (int i = 0; i < c; i++) {
          to[index++] = from[addr + i];
        }
      }
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    int32_t w = int32_t(input.payload.imageValue.width);
    int32_t h = int32_t(input.payload.imageValue.height);
    int32_t c = int32_t(input.payload.imageValue.channels);

    auto pixsize = 1;
    if ((input.payload.imageValue.flags & SHIMAGE_FLAGS_16BITS_INT) == SHIMAGE_FLAGS_16BITS_INT)
      pixsize = 2;
    else if ((input.payload.imageValue.flags & SHIMAGE_FLAGS_32BITS_FLOAT) == SHIMAGE_FLAGS_32BITS_FLOAT)
      pixsize = 4;

    _bytes.resize(_kernel * _kernel * c * pixsize);

    if (_xindex >= w) {
      _xindex = 0;
      _yindex++;
      if (_yindex >= h) {
        _yindex = 0;
      }
    }

    if (pixsize == 1) {
      process<uint8_t>(input, w, h, c);
    } else if (pixsize == 2) {
      process<uint16_t>(input, w, h, c);
    } else if (pixsize == 4) {
      process<float>(input, w, h, c);
    }

    // advance the scan
    _xindex += _step;

    return Var(&_bytes.front(), uint16_t(_kernel), uint16_t(_kernel), input.payload.imageValue.channels,
               input.payload.imageValue.flags);
  }

private:
  std::vector<uint8_t> _bytes;
  int32_t _radius{1};
  int32_t _step{1};
  uint32_t _kernel{1};
  int32_t _xindex{0};
  int32_t _yindex{0};
};

struct StripAlpha {
  static SHTypesInfo inputTypes() { return CoreInfo::ImageType; }
  static SHTypesInfo outputTypes() { return CoreInfo::ImageType; }

  template <typename T> void process(const SHVar &input, int32_t w, int32_t h) {
    const auto from = reinterpret_cast<T *>(input.payload.imageValue.data);
    auto to = reinterpret_cast<T *>(&_bytes[0]);
    for (auto y = 0; y < h; y++) {
      for (auto x = 0; x < w; x++) {
        const auto faddr = ((w * y) + x) * 4;
        const auto taddr = ((w * y) + x) * 3;
        for (auto z = 0; z < 3; z++) {
          to[taddr + z] = from[faddr + z];
        }
      }
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (input.payload.imageValue.channels < 4)
      return input; // nothing to do

    int32_t w = int32_t(input.payload.imageValue.width);
    int32_t h = int32_t(input.payload.imageValue.height);

    auto pixsize = 1;
    if ((input.payload.imageValue.flags & SHIMAGE_FLAGS_16BITS_INT) == SHIMAGE_FLAGS_16BITS_INT)
      pixsize = 2;
    else if ((input.payload.imageValue.flags & SHIMAGE_FLAGS_32BITS_FLOAT) == SHIMAGE_FLAGS_32BITS_FLOAT)
      pixsize = 4;

    _bytes.resize(w * h * 3 * pixsize);

    if (pixsize == 1) {
      process<uint8_t>(input, w, h);
    } else if (pixsize == 2) {
      process<uint16_t>(input, w, h);
    } else if (pixsize == 4) {
      process<float>(input, w, h);
    }

    return Var(&_bytes.front(), uint16_t(w), uint16_t(h), 3, input.payload.imageValue.flags);
  }

private:
  std::vector<uint8_t> _bytes;
};

struct FillAlpha {
  static SHTypesInfo inputTypes() { return CoreInfo::ImageType; }
  static SHTypesInfo outputTypes() { return CoreInfo::ImageType; }

  template <typename T, typename TA> void process(const SHVar &input, int32_t w, int32_t h, TA alpha_value) {
    const auto from = reinterpret_cast<T *>(input.payload.imageValue.data);
    auto to = reinterpret_cast<T *>(&_bytes[0]);
    for (auto y = 0; y < h; y++) {
      for (auto x = 0; x < w; x++) {
        const auto faddr = ((w * y) + x) * 3;
        const auto taddr = ((w * y) + x) * 4;
        for (auto z = 0; z < 3; z++) {
          to[taddr + z] = from[faddr + z];
        }
        to[taddr + 3] = alpha_value;
      }
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (input.payload.imageValue.channels == 4)
      return input; // nothing to do

    // TODO remove this limit maybe!
    if (input.payload.imageValue.channels != 3)
      throw ActivationError("A 3 or 4 channels image was expected.");

    int32_t w = int32_t(input.payload.imageValue.width);
    int32_t h = int32_t(input.payload.imageValue.height);

    auto pixsize = 1;
    if ((input.payload.imageValue.flags & SHIMAGE_FLAGS_16BITS_INT) == SHIMAGE_FLAGS_16BITS_INT)
      pixsize = 2;
    else if ((input.payload.imageValue.flags & SHIMAGE_FLAGS_32BITS_FLOAT) == SHIMAGE_FLAGS_32BITS_FLOAT)
      pixsize = 4;

    _bytes.resize(w * h * 4 * pixsize);

    if (pixsize == 1) {
      process<uint8_t>(input, w, h, 255);
    } else if (pixsize == 2) {
      process<uint16_t>(input, w, h, 65535);
    } else if (pixsize == 4) {
      process<float>(input, w, h, 1.0);
    }

    return Var(&_bytes.front(), uint16_t(w), uint16_t(h), 4, input.payload.imageValue.flags);
  }

private:
  std::vector<uint8_t> _bytes;
};

struct Resize {
  static SHTypesInfo inputTypes() { return CoreInfo::ImageType; }
  static SHTypesInfo outputTypes() { return CoreInfo::ImageType; }

  static inline Parameters _params{{"Width", SHCCSTR("The target width."), {CoreInfo::IntType}},
                                   {"Height", SHCCSTR("How target height."), {CoreInfo::IntType}}};

  static SHParametersInfo parameters() { return _params; }

  SHVar getParam(int index) {
    if (index == 0)
      return Var(int64_t(_width));
    else
      return Var(int64_t(_height));
  }

  void setParam(int index, const SHVar &value) {
    if (index == 0) {
      _width = int32_t(value.payload.intValue);
    } else {
      _height = int32_t(value.payload.intValue);
    }
  }

  void warmup(SHContext *context) {
    _bytes.resize(_width * _height * 4); // assume max 4 channels
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    int w = uint32_t(input.payload.imageValue.width);
    int h = uint32_t(input.payload.imageValue.height);
    int c = uint32_t(input.payload.imageValue.channels);

    auto pixsize = 1;
    if ((input.payload.imageValue.flags & SHIMAGE_FLAGS_16BITS_INT) == SHIMAGE_FLAGS_16BITS_INT)
      pixsize = 2;
    else if ((input.payload.imageValue.flags & SHIMAGE_FLAGS_32BITS_FLOAT) == SHIMAGE_FLAGS_32BITS_FLOAT)
      pixsize = 4;

    _bytes.resize(_width * _height * c * pixsize);

    int flags = 0;
    if ((input.payload.imageValue.flags & SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA) == SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA)
      flags = STBIR_FLAG_ALPHA_PREMULTIPLIED;

    if (pixsize == 1) {
      auto res = stbir_resize_uint8_generic(input.payload.imageValue.data, w, h, w * c, &_bytes.front(), _width, _height,
                                            _width * c, c, c == 4 ? 3 : STBIR_ALPHA_CHANNEL_NONE, flags, STBIR_EDGE_ZERO,
                                            STBIR_FILTER_DEFAULT, STBIR_COLORSPACE_SRGB, nullptr);
      if (res == 0) {
        throw ActivationError("Failed to resize image!");
      }
    } else if (pixsize == 2) {
      auto res =
          stbir_resize_uint16_generic((uint16_t *)input.payload.imageValue.data, w, h, w * c * 2, (uint16_t *)&_bytes.front(),
                                      _width, _height, _width * c * 2, c, c == 4 ? 3 : STBIR_ALPHA_CHANNEL_NONE, flags,
                                      STBIR_EDGE_ZERO, STBIR_FILTER_DEFAULT, STBIR_COLORSPACE_SRGB, nullptr);
      if (res == 0) {
        throw ActivationError("Failed to resize image!");
      }
    } else if (pixsize == 4) {
      auto res = stbir_resize_float_generic((float *)input.payload.imageValue.data, w, h, w * c * 4, (float *)&_bytes.front(),
                                            _width, _height, _width * c * 4, c, c == 4 ? 3 : STBIR_ALPHA_CHANNEL_NONE, flags,
                                            STBIR_EDGE_ZERO, STBIR_FILTER_DEFAULT, STBIR_COLORSPACE_LINEAR, nullptr);
      if (res == 0) {
        throw ActivationError("Failed to resize image!");
      }
    }

    return Var(&_bytes.front(), uint16_t(_width), uint16_t(_height), input.payload.imageValue.channels,
               input.payload.imageValue.flags);
  }

private:
  std::vector<uint8_t> _bytes;
  int _width{32};
  int _height{32};
};

void registerShards() {
  REGISTER_SHARD("Convolve", Convolve);
  REGISTER_SHARD("StripAlpha", StripAlpha);
  REGISTER_SHARD("FillAlpha", FillAlpha);
  REGISTER_SHARD("ResizeImage", Resize);
}
} // namespace Imaging
} // namespace shards

#endif /* IMAGING_H */
