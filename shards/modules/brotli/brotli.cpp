/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

#include <shards/core/module.hpp>
#include <shards/core/shared.hpp>
#include <shards/core/runtime.hpp>
#include <brotli/decode.h>
#include <brotli/encode.h>

namespace shards {
namespace Brotli {
struct Compress {
  std::vector<uint8_t> _buffer;
  int _quality{BROTLI_DEFAULT_QUALITY};

  static SHTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }

  static inline Parameters params{{"Quality",
                                   SHCCSTR("Compression quality, higher is better but slower, valid values "
                                           "from 1 to 11."),
                                   {CoreInfo::IntType}}};

  SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) {
    _quality = int(value.payload.intValue);
    _quality = std::clamp(_quality, 1, 11);
  }

  SHVar getParam(int index) { return Var(_quality); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto maxLen = BrotliEncoderMaxCompressedSize(input.payload.bytesSize);
    _buffer.resize(maxLen + sizeof(uint32_t));
    size_t outputLen = maxLen;
    auto res = BrotliEncoderCompress(_quality, BROTLI_DEFAULT_WINDOW, BROTLI_DEFAULT_MODE, input.payload.bytesSize,
                                     input.payload.bytesValue, &outputLen, &_buffer[sizeof(uint32_t)]);
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

  static SHTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }

  static void decompressLengthSanityCheck(uint32_t decompressedLength, uint32_t compressedLength) {
    if (compressedLength == 0 && decompressedLength != 0) {
      throw ActivationError("Decompressed length is non-zero but compressed length is zero");
    }
    uint32_t largeAllocThreshold = (1 << 26); // 64MiB threshold
    if (decompressedLength > largeAllocThreshold) {
      double ratio = double(decompressedLength) / double(compressedLength);
      if (ratio > 16.0) {
        throw ActivationError(fmt::format("Decompression ratio ({}) too large, possibly corrupted data", ratio));
      }
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (input.payload.bytesSize < sizeof(uint32_t)) {
      throw ActivationError("Failed to decompress, not enough input bytes");
    }
    auto len = reinterpret_cast<uint32_t *>(input.payload.bytesValue);
    decompressLengthSanityCheck(*len, input.payload.bytesSize - sizeof(uint32_t));

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

} // namespace Brotli
SHARDS_REGISTER_FN(brotli) {
  REGISTER_SHARD("Brotli.Compress", Brotli::Compress);
  REGISTER_SHARD("Brotli.Decompress", Brotli::Decompress);
}
} // namespace shards
