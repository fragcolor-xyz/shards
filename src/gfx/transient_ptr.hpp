#ifndef B5E2C236_27A8_4E17_B272_BC06498D4859
#define B5E2C236_27A8_4E17_B272_BC06498D4859

#include <memory>

namespace gfx::detail {
// A wrapper to store opaque values passed around between renderer components
// The pointers should be allocated using a transient allocator, since this class does not free the memory
// It is only able optionally run destructors on objects by calling destroy()
//
// The point of this is to allow a specialized render component to return data, e.g.:
//   TransientPtr someFunction() { return new SomeObject(); }
// The TransientPtr can then be passed around by the caller and later passed back to the same component:
//   void someOtherFunction(TransientPtr ptr) {
//     SomeObject& myObject = ptr.Get<SomeObject>();
//   }
// After that the called can run: ptr.destroy()
struct TransientPtr {
private:
  using Deleter = void (*)(void *);
  void *ptr{};
  Deleter deleter{};

public:
  TransientPtr() = default;
  template <typename T> TransientPtr(T *ptr) : ptr(ptr), deleter([](void *v) { std::destroy_at((T *)v); }) {}
  TransientPtr(void *ptr, Deleter &&deleter) : ptr(ptr), deleter(deleter) {}
  TransientPtr(TransientPtr &&other) : ptr(other.ptr), deleter(other.deleter) {
    other.ptr = nullptr;
    other.deleter = nullptr;
  }
  TransientPtr &operator=(TransientPtr &&other) {
    ptr = other.ptr;
    deleter = other.deleter;
    other.ptr = nullptr;
    other.deleter = nullptr;
    return *this;
  }
  TransientPtr(const TransientPtr &) = default;
  TransientPtr &operator=(const TransientPtr &) = default;

  operator bool() const { return ptr; }

  void destroy() {
    if (ptr) {
      deleter(ptr);
      ptr = nullptr;
      deleter = nullptr;
    }
  }

  template <typename T> T &get() {
    assert(ptr);
    return *static_cast<T *>(ptr);
  }

  template <typename T> const T &get() const {
    assert(ptr);
    return *static_cast<const T *>(ptr);
  }

  template <typename T> static inline TransientPtr make(T *ptr) { return TransientPtr(ptr); }
};
} // namespace gfx::detail

#endif /* B5E2C236_27A8_4E17_B272_BC06498D4859 */
