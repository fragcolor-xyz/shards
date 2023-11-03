#ifndef F4220AE4_FBD9_4190_AE31_57B6F3D431BE
#define F4220AE4_FBD9_4190_AE31_57B6F3D431BE

#include <shards/shards.h>
#include <linalg.h>
#include <vector>
#include <stdexcept>
#include <cassert>
#include <string_view>

namespace shards {
struct BufferRefWriter {
  std::vector<uint8_t> &_buffer;
  BufferRefWriter(std::vector<uint8_t> &stream, bool clear = true) : _buffer(stream) {
    if (clear) {
      _buffer.clear();
    }
  }
  void operator()(const uint8_t *buf, size_t size) { _buffer.insert(_buffer.end(), buf, buf + size); }
};

struct BufferWriter {
  std::vector<uint8_t> _buffer;
  BufferRefWriter _inner;
  BufferWriter() : _inner(_buffer) {}
  void operator()(const uint8_t *buf, size_t size) { _inner(buf, size); }
};

struct BytesReader {
  const uint8_t *buffer;
  size_t offset;
  size_t max;

  BytesReader(std::string_view sv) : buffer((uint8_t *)sv.data()), offset(0), max(sv.size()) {}
  BytesReader(const uint8_t *buf, size_t size) : buffer(buf), offset(0), max(size) {}
  void operator()(uint8_t *buf, size_t size) {
    auto newOffset = offset + size;
    if (newOffset > max) {
      throw std::runtime_error("Overflow requested");
    }
    memcpy(buf, buffer + offset, size);
    offset = newOffset;
  }
};

struct BufferRefReader {
  BytesReader _inner;
  BufferRefReader(const std::vector<uint8_t> &stream) : _inner(stream.data(), stream.size()) {}
  void operator()(uint8_t *buf, size_t size) { _inner(buf, size); }
};

struct VarReader {
  BytesReader _inner;
  VarReader(const SHVar &var) : _inner(var.payload.bytesValue, var.payload.bytesSize) { assert(var.valueType == SHType::Bytes); }
  void operator()(uint8_t *buf, size_t size) { _inner(buf, size); }
};

template <typename T, typename V>
std::enable_if_t<std::is_integral_v<V> || std::is_floating_point_v<V> || std::is_same_v<V, bool>, void> //
serde(T &stream, V &v) {
  stream((uint8_t *)&v, sizeof(V));
}
template <typename T, typename V, int M> void serde(T &stream, linalg::vec<V, M> &v) {
  for (int i = 0; i < M; i++)
    serde(stream, v[i]);
}
template <typename T, typename V, int N, int M> void serde(T &stream, linalg::mat<V, M, N> &v) {
  for (int m = 0; m < N; m++) {
    serde(stream, v[m]);
  }
}
template <typename T> void serde(T &stream, std::monostate &v) {}
template <typename T, typename V> void serdeConst(T &stream, const V &v) { serde(stream, const_cast<V &>(v)); }
template <typename T, typename As, typename E> std::enable_if_t<std::is_enum_v<E>> serdeAs(T &stream, E &v) {
  As a = As(v);
  serde(stream, a);
  v = E(a);
}
} // namespace shards

#endif /* F4220AE4_FBD9_4190_AE31_57B6F3D431BE */
