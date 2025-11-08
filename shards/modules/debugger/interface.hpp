#ifndef EB39FB65_2B9E_4609_AAE5_17D02DF485A1
#define EB39FB65_2B9E_4609_AAE5_17D02DF485A1

#include <shards/shards.h>
#include <shards/core/assert.hpp>
#include <vector>
#include <string>
#include <tuple>

namespace shards::dbg {
void onShard(SHContext *context, Shard *where);
void onError(SHContext *context, Shard *blk, const std::string &err);
void onWireRunStart(SHContext *ctx);
void onWireRunEnd(SHContext *ctx);
void onEnterActivation(SHContext *context, const SHVar **input, const SHVar **output, Shard **start, size_t offset, size_t len);
void onExitActivation(SHContext *context, Shard **start, size_t offset, size_t len);
void unload();

template <typename T> std::tuple<Shard **, size_t, size_t> extractShardsParam(T &shards) {
  Shard **start{};
  size_t stride{};
  size_t len{};
  if constexpr (std::is_same<T, Shards>::value) {
    start = &shards.elements[0];
    stride = sizeof(ShardPtr);
    len = shards.len;
  } else if constexpr (std::is_same<T, SHSeq>::value) {
    start = &shards.elements[0].payload.shardValue;
    stride = (uint8_t *)&shards.elements[1] - (uint8_t *)&shards.elements[0];
    len = shards.len;
  } else if constexpr (std::is_same<T, std::vector<ShardPtr>>::value) {
    start = shards.data();
    stride = sizeof(ShardPtr);
    len = shards.size() - 1; // excluding null terminator
  } else {
    shassert(false && "Unreachable shardsActivation case");
  }
  return {start, stride, len};
}

template <typename T> void onEnterActivation(T &shards, SHContext *context, const SHVar **input, const SHVar **output) {
  auto [start, stride, len] = extractShardsParam(shards);
  onEnterActivation(context, input, output, start, stride, len);
}

template <typename T> void onExitActivation(T &shards, SHContext *context) {
  auto [start, stride, len] = extractShardsParam(shards);
  onExitActivation(context, start, stride, len);
}
} // namespace shards::dbg

#endif /* EB39FB65_2B9E_4609_AAE5_17D02DF485A1 */
