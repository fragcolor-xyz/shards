/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

// Deprecating this in favor of brotli
// will be removed in the future!

#include "blocks/shared.hpp"
#include "runtime.hpp"
#include <brotli/decode.h>
#include <brotli/encode.h>

namespace chainblocks {
namespace Brotli {
struct Compress {
  std::vector<uint8_t> _buffer;
  int _quality{BROTLI_DEFAULT_QUALITY};

  static CBTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BytesType; }

  static inline Parameters params{
      {"Quality",
       "Compression quality, higher is better but slower, "
       "valid values from 1 to 11.",
       {CoreInfo::IntType}}};

  CBParametersInfo parameters() { return params; }

  void setParam(int index, CBVar value) {
    _quality = int(value.payload.intValue);
    _quality = std::clamp(_quality, 1, 11);
  }

  CBVar getParam(int index) { return Var(_quality); }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto maxLen = BrotliEncoderMaxCompressedSize(input.payload.bytesSize);
    _buffer.resize(maxLen + sizeof(uint32_t));
    size_t outputLen = maxLen;
    auto res = BrotliEncoderCompress(
        _quality, BROTLI_DEFAULT_WINDOW, BROTLI_DEFAULT_MODE,
        input.payload.bytesSize, input.payload.bytesValue, &outputLen,
        &_buffer[sizeof(uint32_t)]);
    if (res != BROTLI_TRUE) {
      throw ActivationError("Failed to compress");
    }
    auto len = reinterpret_cast<uint32_t *>(&_buffer.front());
    *len = input.payload.bytesSize;
    return Var((uint8_t *)&_buffer[0], uint32_t(outputLen + sizeof(uint32_t)));
  }
};

struct Decompress {
  std::vector<uint8_t> _buffer;

  static CBTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BytesType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto len = reinterpret_cast<uint32_t *>(input.payload.bytesValue);
    auto buffer = &input.payload.bytesValue[sizeof(uint32_t)];
    auto bufferSize = input.payload.bytesSize - sizeof(uint32_t);
    _buffer.resize((*len) + 1);
    size_t inLen = size_t(*len);
    auto res = BrotliDecoderDecompress(bufferSize, buffer, &inLen, &_buffer[0]);
    if (res != BROTLI_DECODER_RESULT_SUCCESS) {
      throw ActivationError("Failed to decompress");
    }
    _buffer[*len] = 0;
    return Var((uint8_t *)&_buffer[0], uint32_t(*len));
  }
};

void registerBlocks() {
  REGISTER_CBLOCK("Brotli.Compress", Compress);
  REGISTER_CBLOCK("Brotli.Decompress", Decompress);
}
} // namespace Brotli
} // namespace chainblocks