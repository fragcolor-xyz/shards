#ifndef B32C6203_C3B8_4256_865D_753B6E2FDA4A
#define B32C6203_C3B8_4256_865D_753B6E2FDA4A

#include <shards/shards.h>

namespace shards::lang {} // namespace shards::lang

extern "C" {
struct SHFileRegistryHandle;
SHFileRegistryHandle *shlang_fr_static();
uint32_t shlang_fr_get_file_id(SHFileRegistryHandle *handle, SHStringWithLen path);
SHStringWithLen shlang_fr_get_file_name(SHFileRegistryHandle *handle, uint32_t file_id);
}

#endif /* B32C6203_C3B8_4256_865D_753B6E2FDA4A */
