#ifndef CB1C0D43_D513_42DB_836B_5A9791E43B67
#define CB1C0D43_D513_42DB_836B_5A9791E43B67

#include "wrapper.hpp"
#include <vector>

#if !HAVE_CXX_17_MEMORY_RESOURCE
namespace shards::pmr {
template <typename T> using vector = std::vector<T, PolymorphicAllocator<T>>;
}
#endif

#endif /* CB1C0D43_D513_42DB_836B_5A9791E43B67 */
