#ifndef BB403623_76BF_4A60_A8F3_434E3DDB28A9
#define BB403623_76BF_4A60_A8F3_434E3DDB28A9

#include <cstdint>
#include <string_view>

namespace shards::fast_string {
struct Storage;
extern Storage *storage;

// Create the default storage instance
void init();
uint64_t store(std::string_view);
std::string_view load(uint64_t);

} // namespace shards::fast_string

#endif /* BB403623_76BF_4A60_A8F3_434E3DDB28A9 */
