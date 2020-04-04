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
       {CoreInfo::IntType}}};

  static CBParametersInfo parameters() { return _params; }

  CBVar getParam(int index) { return Var(int64_t(_radius)); }

  void setParam(int index, CBVar value) {
    _radius = int32_t(value.payload.intValue);
    if (_radius <= 0)
      _radius = 1;
    _kernel = (_radius - 1) * 2 + 1;
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
    for (int x = high; x >= low; x--) {
      for (int y = high; y >= low; y--) {
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
    _xindex++;

    return Var(&_bytes.front(), uint16_t(_kernel), uint16_t(_kernel),
               input.payload.imageValue.channels,
               input.payload.imageValue.flags);
  }

private:
  std::vector<uint8_t> _bytes;
  int32_t _radius{1};
  uint32_t _kernel{1};
  int32_t _xindex{0};
  int32_t _yindex{0};
};

void registerBlocks() { REGISTER_CBLOCK("Convolve", Convolve); }
} // namespace Imaging
} // namespace chainblocks

#endif /* IMAGING_H */
