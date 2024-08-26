/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include <shards/core/shared.hpp>
#include <shards/core/runtime.hpp>
#include <snappy.h>

namespace shards {
namespace Snappy {
struct Compress {
  std::vector<char> _buffer;

  static SHOptionalString help() { return SHCCSTR("This shard compresses the input bytes sequence using the Snappy algorithm and returns the compressed bytes sequence."); }
  static SHOptionalString inputHelp() { return SHCCSTR("The bytes sequence to compress."); }
  static SHOptionalString outputHelp() { return SHCCSTR("The compressed bytes sequence."); }

  static SHTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    _buffer.resize(snappy::MaxCompressedLength(input.payload.bytesSize));
    size_t outputLen;
    snappy::RawCompress((char *)input.payload.bytesValue, input.payload.bytesSize, &_buffer[0], &outputLen);
    return Var((uint8_t *)&_buffer[0], uint32_t(outputLen));
  }
};

struct Decompress {
  std::vector<char> _buffer;

  static SHOptionalString help() { return SHCCSTR("This shard decompressed the input bytes sequence that has been previously compressed using the Snappy.Compress shard."); }
  static SHOptionalString inputHelp() { return SHCCSTR("The compressed bytes sequence to decompress."); }
  static SHOptionalString outputHelp() { return SHCCSTR("The decompressed bytes sequence."); }

  static SHTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    size_t len;
    if (!snappy::GetUncompressedLength((char *)input.payload.bytesValue, size_t(input.payload.bytesSize), &len)) {
      throw SHException("Snappy failed to find uncompressed length, probably invalid data!");
    }
    _buffer.resize(len + 1);
    snappy::RawUncompress((char *)input.payload.bytesValue, input.payload.bytesSize, &_buffer[0]);
    // easy fix for null term strings
    _buffer[len] = 0;
    return Var((uint8_t *)&_buffer[0], uint32_t(len));
  }
};

} // namespace Snappy
SHARDS_REGISTER_FN(snappy) {
  REGISTER_SHARD("Snappy.Compress", Snappy::Compress);
  REGISTER_SHARD("Snappy.Decompress", Snappy::Decompress);
}
} // namespace shards
