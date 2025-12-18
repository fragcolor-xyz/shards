/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace shards {
namespace base64 {

inline constexpr char encode_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

inline constexpr int8_t decode_table[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0-15
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 16-31
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, // 32-47 (+, /)
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, // 48-63 (0-9)
    -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, // 64-79 (A-O)
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, // 80-95 (P-Z)
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, // 96-111 (a-o)
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, // 112-127 (p-z)
};

inline size_t encoded_size(size_t n) { return (n + 2) / 3 * 4; }

inline size_t decoded_size(size_t n) { return (n + 3) / 4 * 3; }

inline size_t encode(char *dest, const void *src, size_t len) {
  const auto *in = static_cast<const uint8_t *>(src);
  char *out = dest;

  size_t i = 0;
  for (; i + 2 < len; i += 3) {
    *out++ = encode_table[(in[i] >> 2) & 0x3F];
    *out++ = encode_table[((in[i] & 0x03) << 4) | ((in[i + 1] >> 4) & 0x0F)];
    *out++ = encode_table[((in[i + 1] & 0x0F) << 2) | ((in[i + 2] >> 6) & 0x03)];
    *out++ = encode_table[in[i + 2] & 0x3F];
  }

  if (i < len) {
    *out++ = encode_table[(in[i] >> 2) & 0x3F];
    if (i + 1 < len) {
      *out++ = encode_table[((in[i] & 0x03) << 4) | ((in[i + 1] >> 4) & 0x0F)];
      *out++ = encode_table[(in[i + 1] & 0x0F) << 2];
    } else {
      *out++ = encode_table[(in[i] & 0x03) << 4];
      *out++ = '=';
    }
    *out++ = '=';
  }

  return out - dest;
}

inline std::pair<size_t, size_t> decode(void *dest, const char *src, size_t len) {
  auto *out = static_cast<uint8_t *>(dest);
  size_t written = 0;

  uint32_t accum = 0;
  int bits = 0;

  for (size_t i = 0; i < len; ++i) {
    unsigned char c = static_cast<unsigned char>(src[i]);
    if (c == '=')
      break;
    if (c >= 128)
      continue;
    int8_t val = decode_table[c];
    if (val < 0)
      continue;

    accum = (accum << 6) | val;
    bits += 6;

    if (bits >= 8) {
      bits -= 8;
      *out++ = static_cast<uint8_t>((accum >> bits) & 0xFF);
      ++written;
    }
  }

  return {written, len};
}

} // namespace base64
} // namespace shards
