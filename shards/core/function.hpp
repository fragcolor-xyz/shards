#ifndef F4364F08_DB97_488E_AE6D_CF2D26335C36
#define F4364F08_DB97_488E_AE6D_CF2D26335C36
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <type_traits>

namespace shards {
// Acts like std::function but inlines all data into this structure itself
// the maximum size is defined by the first template argument
// although the actual contained size is slightly smalled due to type erasure overhead (~1 pointer / 8 bytes)
template <size_t InlineSize, typename Sig> struct FunctionBase;
template <size_t InlineSize, typename R, typename... Args> struct FunctionBase<InlineSize, R(Args...)> {
  struct ICall {
    virtual ~ICall() = default;
    virtual R call(Args...) const = 0;
    virtual ICall *clone(void *into) const = 0;
  };

  template <typename L> struct CallImpl final : public ICall {
    L callable;
    constexpr CallImpl(L &&f) : callable(f) {}
    virtual R call(Args... args) const { return callable(args...); }
    virtual ICall *clone(void *into) const { return new (into) CallImpl(*this); }
  };

  static constexpr size_t Overhead = 1;
  static constexpr size_t MemSize = (InlineSize - Overhead);

private:
  __attribute__((aligned(16))) uint8_t mem[MemSize]{};
  uint8_t flags = 0;

  template <typename T> T &getMemAsRef() { return *reinterpret_cast<T *>(&mem); }
  template <typename T> const T &getMemAsRef() const { return *reinterpret_cast<const T *>(&mem); }

  const ICall *getCallable() const {
    if (isCallable()) {
      return &getMemAsRef<ICall>();
    }
    return nullptr;
  }

  ICall *getCallable() {
    if (isCallable()) {
      return &getMemAsRef<ICall>();
    }
    return nullptr;
  }

  bool isCallable() const {
    // NOTE: This assumes a layout of CallImpl with the vtable pointer at the start
    return getMemAsRef<void *>() != nullptr;
  }

public:
  FunctionBase() = default;
  ~FunctionBase() { reset(); }

  template <typename L>
  FunctionBase(L &&f,
               typename std::enable_if<!std::is_same_v<std::decay_t<L>, FunctionBase> && std::is_invocable<L, Args...>::value,
                                       int>::type dummy = 0) {
    reset(std::forward<L>(f));
  }

  template <typename L>
  typename std::enable_if<!std::is_same_v<std::decay_t<L>, FunctionBase> && std::is_invocable<L, Args...>::value,
                          FunctionBase &>::type
  operator=(L &&f) {
    reset(std::forward<L>(f));
    return *this;
  }

  FunctionBase(const FunctionBase &other) {
    if (const ICall *call = other.getCallable()) {
      call->clone(mem);
    }
  }

  FunctionBase &operator=(const FunctionBase &other) {
    if (const ICall *call = other.getCallable()) {
      call->clone(mem);
    }
    return *this;
  }

  template <typename L> void reset(L &&callable) {
    constexpr size_t RequestedSize = sizeof(CallImpl<L>);
    static_assert(RequestedSize < MemSize, "Lambda is larger than maximum inlined size");
    reset();
    new (mem) CallImpl<L>(std::forward<L>(callable));
  }

  void reset() {
    if (ICall *callable = getCallable()) {
      callable->~ICall();
      getMemAsRef<void *>() = nullptr;
    }
  }

  R operator()(Args... args) const {
    if (const ICall *callable = getCallable()) {
      return callable->call(args...);
    }
  }

  operator bool() const { return isCallable(); }
};

template <typename Sig> using Function = FunctionBase<256, Sig>;
} // namespace shards

#endif /* F4364F08_DB97_488E_AE6D_CF2D26335C36 */
