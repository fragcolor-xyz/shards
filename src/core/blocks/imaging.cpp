#ifndef IMAGING_H
#define IMAGING_H

#include "shared.hpp"
#include "smolscale.h"

namespace chainblocks {
namespace Imaging {
struct Convolve {
  static CBTypesInfo inputTypes() { return CoreInfo::ImageType; }
  static CBTypesInfo outputTypes() { return CoreInfo::ImageType; }

  static inline Parameters _params{
      {"Radius",
       "The radius of the kernel, e.g. 1 = 1x1; 2 = 3x3; 3 = 5x5 and so on.",
       {CoreInfo::IntType}},
      {"Step",
       "How many pixels to advance each activation.",
       {CoreInfo::IntType}}};

  static CBParametersInfo parameters() { return _params; }

  CBVar getParam(int index) {
    if (index == 0)
      return Var(int64_t(_radius));
    else
      return Var(int64_t(_step));
  }

  void setParam(int index, CBVar value) {
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

  void warmup(CBContext *context) {
    _bytes.reserve(_kernel * _kernel * 4); // assume max 4 channels
  }

  void cleanup() {
    _xindex = 0;
    _yindex = 0;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    int32_t w = int32_t(input.payload.imageValue.width);
    int32_t h = int32_t(input.payload.imageValue.height);
    int32_t c = int32_t(input.payload.imageValue.channels);

    _bytes.resize(_kernel * _kernel * c);

    if (_xindex >= w) {
      _xindex = 0;
      _yindex++;
      if (_yindex >= h) {
        _yindex = 0;
      }
    }

    // read proper pixels at indices
    const int high = _radius - 1;
    const int low = high * -1;
    int index = 0;
    for (int y = low; y <= high; y++) {
      for (int x = low; x <= high; x++) {
        const int cidxx = _xindex + x;
        const int cidxy = _yindex + y;
        const auto idxx = std::clamp<int>(cidxx, 0, w - 1);
        const auto idxy = std::clamp<int>(cidxy, 0, h - 1);
        const int addr = ((w * idxy) + idxx) * c;
        for (int i = 0; i < c; i++) {
          _bytes[index++] = input.payload.imageValue.data[addr + i];
        }
      }
    }

    // advance the scan
    _xindex += _step;

    return Var(&_bytes.front(), uint16_t(_kernel), uint16_t(_kernel),
               input.payload.imageValue.channels,
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
  static CBTypesInfo inputTypes() { return CoreInfo::ImageType; }
  static CBTypesInfo outputTypes() { return CoreInfo::ImageType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (input.payload.imageValue.channels < 4)
      return input; // nothing to do

    int32_t w = int32_t(input.payload.imageValue.width);
    int32_t h = int32_t(input.payload.imageValue.height);

    _bytes.resize(w * h * 3);

    for (auto y = 0; y < h; y++) {
      for (auto x = 0; x < w; x++) {
        const auto faddr = ((w * y) + x) * 4;
        const auto taddr = ((w * y) + x) * 3;
        for (auto z = 0; z < 3; z++) {
          _bytes[taddr + z] = input.payload.imageValue.data[faddr + z];
        }
      }
    }

    return Var(&_bytes.front(), uint16_t(w), uint16_t(h), 3,
               input.payload.imageValue.flags);
  }

private:
  std::vector<uint8_t> _bytes;
};

struct FillAlpha {
  static CBTypesInfo inputTypes() { return CoreInfo::ImageType; }
  static CBTypesInfo outputTypes() { return CoreInfo::ImageType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (input.payload.imageValue.channels == 4)
      return input; // nothing to do

    // TODO remove this limit maybe!
    if (input.payload.imageValue.channels != 3)
      throw ActivationError("A 3 or 4 channels image was expected.");

    int32_t w = int32_t(input.payload.imageValue.width);
    int32_t h = int32_t(input.payload.imageValue.height);

    _bytes.resize(w * h * 4);

    for (auto y = 0; y < h; y++) {
      for (auto x = 0; x < w; x++) {
        const auto faddr = ((w * y) + x) * 3;
        const auto taddr = ((w * y) + x) * 4;
        for (auto z = 0; z < 3; z++) {
          _bytes[taddr + z] = input.payload.imageValue.data[faddr + z];
        }
        _bytes[taddr + 3] = 255;
      }
    }

    return Var(&_bytes.front(), uint16_t(w), uint16_t(h), 4,
               input.payload.imageValue.flags);
  }

private:
  std::vector<uint8_t> _bytes;
};

struct Resize {
  static CBTypesInfo inputTypes() { return CoreInfo::ImageType; }
  static CBTypesInfo outputTypes() { return CoreInfo::ImageType; }

  static inline Parameters _params{
      {"Width", "The target width.", {CoreInfo::IntType}},
      {"Height", "How target height.", {CoreInfo::IntType}}};

  static CBParametersInfo parameters() { return _params; }

  CBVar getParam(int index) {
    if (index == 0)
      return Var(int64_t(_width));
    else
      return Var(int64_t(_height));
  }

  void setParam(int index, CBVar value) {
    if (index == 0) {
      _width = int32_t(value.payload.intValue);
    } else {
      _height = int32_t(value.payload.intValue);
    }
  }

  void warmup(CBContext *context) {
    _bytes.resize(_width * _height * 4); // assume max 4 channels
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    uint32_t w = uint32_t(input.payload.imageValue.width);
    uint32_t h = uint32_t(input.payload.imageValue.height);
    uint32_t c = uint32_t(input.payload.imageValue.channels);

    SmolPixelType pixType = SMOL_PIXEL_MAX;

    if (c != 4) {
      throw ActivationError("number of channels not supported, must be 4.");
    }

    if ((input.payload.imageValue.flags & CBIMAGE_FLAGS_BGRA) == 0) {
      if ((input.payload.imageValue.flags &
           CBIMAGE_FLAGS_PREMULTIPLIED_ALPHA) == 0) {
        pixType = SMOL_PIXEL_BGRA8_PREMULTIPLIED;
      } else {
        pixType = SMOL_PIXEL_BGRA8_UNASSOCIATED;
      }
    } else {
      if ((input.payload.imageValue.flags &
           CBIMAGE_FLAGS_PREMULTIPLIED_ALPHA) == 0) {
        pixType = SMOL_PIXEL_RGBA8_PREMULTIPLIED;
      } else {
        pixType = SMOL_PIXEL_RGBA8_UNASSOCIATED;
      }
    }

    if (pixType == SMOL_PIXEL_MAX) {
      throw ActivationError("Invalid pixel format.");
    }

    smol_scale_simple(pixType, (const uint32_t *)input.payload.imageValue.data,
                      w, h, w * c, pixType, (uint32_t *)&_bytes.front(),
                      uint32_t(_width), uint32_t(_height),
                      uint32_t(_width) * c);

    return Var(&_bytes.front(), uint16_t(_width), uint16_t(_height),
               input.payload.imageValue.channels,
               input.payload.imageValue.flags);
  }

private:
  std::vector<uint8_t> _bytes;
  int32_t _width{32};
  int32_t _height{32};
};

void registerBlocks() {
  REGISTER_CBLOCK("Convolve", Convolve);
  REGISTER_CBLOCK("StripAlpha", StripAlpha);
  REGISTER_CBLOCK("FillAlpha", FillAlpha);
  REGISTER_CBLOCK("ResizeImage", Resize);
}
} // namespace Imaging
} // namespace chainblocks

#endif /* IMAGING_H */
