#ifndef B68BA3EA_DC46_428A_9F0D_1E831742B9EB
#define B68BA3EA_DC46_428A_9F0D_1E831742B9EB

// NOTE: this c++17 feature recently landed in the standard libary but it's not implemented everywhere yet
// https://reviews.llvm.org/rG243da90ea5357c1ca324f714ea4813dc9029af27
// use boost where not supported
#ifndef HAVE_CXX_17_MEMORY_RESOURCE
#define HAVE_CXX_17_MEMORY_RESOURCE 0
#endif

#if !HAVE_CXX_17_MEMORY_RESOURCE
#include <boost/container/pmr/monotonic_buffer_resource.hpp>
#include <boost/container/pmr/memory_resource.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>

namespace shards::pmr {
using namespace boost::container::pmr;
} // namespace shards::pmr

#else
#include <memory_resource>
namespace shards::pmr {
using namespace std::pmr;
}
#endif

namespace shards::pmr {
template <typename T = uint8_t> struct PolymorphicAllocator : public polymorphic_allocator<T> {
  using polymorphic_allocator<T>::polymorphic_allocator;
  using polymorphic_allocator<T>::operator=;

  template <class U> PolymorphicAllocator(const polymorphic_allocator<U> &other) : polymorphic_allocator<T>(other.resource()) {}

  void allocate_bytes(size_t size, size_t align = std::alignment_of<T>::value) { return this->resource()->allocate(size, align); }

  template <typename U, typename... TArgs> U *new_object(TArgs... args) {
    U *p = reinterpret_cast<PolymorphicAllocator<U> &>(*this).allocate(1);
    this->construct(p, std::forward<TArgs>(args)...);
    return p;
  }

  template <typename U, typename... TArgs> U *new_objects(size_t count, TArgs... args) {
    U *p = reinterpret_cast<PolymorphicAllocator<U> &>(*this).allocate(count);
    for (size_t i = 0; i < count; i++)
      this->construct(p + i, std::forward<TArgs>(args)...);
    return p;
  }
};
} // namespace shards::pmr

#endif /* B68BA3EA_DC46_428A_9F0D_1E831742B9EB */
