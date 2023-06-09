#ifndef F0D8A2DF_0D50_48F6_8E6E_C50BD8188B71
#define F0D8A2DF_0D50_48F6_8E6E_C50BD8188B71

#include <shards/shards.hpp>

namespace shards {
ALWAYS_INLINE FLATTEN void setInlineShardId(Shard *shard, std::string_view name);
ALWAYS_INLINE FLATTEN bool activateShardInline(Shard *blk, SHContext *context, const SHVar &input, SHVar &output);
}

#endif /* F0D8A2DF_0D50_48F6_8E6E_C50BD8188B71 */
