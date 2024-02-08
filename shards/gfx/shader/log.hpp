#ifndef DA5BB6B8_41E8_4742_B134_6E275652F2F2
#define DA5BB6B8_41E8_4742_B134_6E275652F2F2

#include <log/log.hpp>
namespace gfx::shader {
inline shards::logging::Logger getLogger() { return shards::logging::getOrCreate("shader"); }
} // namespace gfx::shader

#endif /* DA5BB6B8_41E8_4742_B134_6E275652F2F2 */
