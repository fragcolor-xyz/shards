#include <shards/core/foundation.hpp>
#include <shards/core/ops_internal.hpp>

extern "C" {
#if SH_ANDROID
// This is just here to force the other symbol to be included into the final library
// whole-archive doesn't seem to work correctly
__attribute__((visibility("default"))) SHCore *shardsInterface1(uint32_t abi_version) { return shardsInterface(abi_version); }
#endif
}