#ifndef IMAGING_H
#define IMAGING_H

#include "shared.hpp"

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
        int idxx = _xindex + x;

        // pad shifting to nearest
        while (idxx < 0)
          idxx++;
        while (idxx >= w)
          idxx--;

        int idxy = _yindex + y;

        // pad shifting to nearest
        while (idxy < 0)
          idxy++;
        while (idxx >= h)
          idxy--;

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

    return Var(&_bytes.front(), w, h, 3, input.payload.imageValue.flags);
  }

private:
  std::vector<uint8_t> _bytes;
  int32_t _radius{1};
  uint32_t _kernel{1};
  int32_t _xindex{0};
  int32_t _yindex{0};
};

void registerBlocks() {
  REGISTER_CBLOCK("Convolve", Convolve);
  REGISTER_CBLOCK("StripAlpha", StripAlpha);
}
} // namespace Imaging
} // namespace chainblocks

#endif /* IMAGING_H */
