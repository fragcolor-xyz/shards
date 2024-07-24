#ifndef F233B826_CF94_4663_A3F0_6D181E5D6979
#define F233B826_CF94_4663_A3F0_6D181E5D6979

#include "wrapper.hpp"
#include <string>

#if !HAVE_CXX_17_MEMORY_RESOURCE
namespace shards::pmr {
using string = std::basic_string<char, std::char_traits<char>, PolymorphicAllocator<char>>;
}
#endif

#endif /* F233B826_CF94_4663_A3F0_6D181E5D6979 */
