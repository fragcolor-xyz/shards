/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#ifndef SH_CORE_FOUNDATION
#define SH_CORE_FOUNDATION

// must go first
#if _WIN32
#include <winsock2.h>
#endif

#ifdef SH_USE_TSAN
extern "C" {
void *__tsan_get_current_fiber(void);
void *__tsan_create_fiber(unsigned flags);
void __tsan_destroy_fiber(void *fiber);
void __tsan_switch_to_fiber(void *fiber, unsigned flags);
void __tsan_set_fiber_name(void *fiber, const char *name);
const unsigned __tsan_switch_to_fiber_no_sync = 1 << 0;
}
#endif

#include "ops_internal.hpp"
#include "shards.h"
#include <shards.hpp>

// Included 3rdparty
#include "spdlog/spdlog.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <deque>
#include <iomanip>
#include <list>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <type_traits>
#include <unordered_set>
#include <variant>

#include "shardwrapper.hpp"

// Needed specially for win32/32bit
#include <boost/align/aligned_allocator.hpp>

// TODO make it into a run-time param
#ifndef NDEBUG
#define SH_BASE_STACK_SIZE 1024 * 1024
#else
#define SH_BASE_STACK_SIZE 128 * 1024
#endif

#if defined(_WIN32) && defined(__clang__)
#define SH_STACK_ALLOCATOR_SUPPORTED 0
#endif

#ifndef SH_STACK_ALLOCATOR_SUPPORTED
#define SH_STACK_ALLOCATOR_SUPPORTED 1
#endif

#ifndef __EMSCRIPTEN__
// For coroutines/context switches
#include <boost/context/continuation.hpp>
typedef boost::context::continuation SHCoro;
#else
#include <emscripten/fiber.h>
struct SHCoro {
  size_t stack_size;
  static constexpr int as_stack_size = 32770;

  SHCoro() : stack_size(SH_BASE_STACK_SIZE) {}
  SHCoro(size_t size) : stack_size(size) {}
  ~SHCoro() {
    if (c_stack)
      ::operator delete[](c_stack, std::align_val_t{16});
  }
  void init(const std::function<void()> &func);
  NO_INLINE void resume();
  NO_INLINE void yield();

  // compatibility with boost
  operator bool() const { return true; }

  emscripten_fiber_t em_fiber;
  emscripten_fiber_t *em_parent_fiber{nullptr};
  std::function<void()> func;
  uint8_t asyncify_stack[as_stack_size];
  uint8_t *c_stack{nullptr};
};
#endif

#ifdef NDEBUG
#define SH_COMPRESSED_STRINGS 1
#endif

#ifdef SH_COMPRESSED_STRINGS
#define SHCCSTR(_str_) ::shards::getCompiledCompressedString(::shards::constant<::shards::crc32(_str_)>::value)
#else
#define SHCCSTR(_str_) ::shards::setCompiledCompressedString(::shards::constant<::shards::crc32(_str_)>::value, _str_)
#endif

#define SHLOG_TRACE SPDLOG_TRACE
#define SHLOG_DEBUG SPDLOG_DEBUG
#define SHLOG_INFO SPDLOG_INFO
#define SHLOG_WARNING SPDLOG_WARN
#define SHLOG_ERROR SPDLOG_ERROR
#define SHLOG_FATAL(...)          \
  {                               \
    SPDLOG_CRITICAL(__VA_ARGS__); \
    std::abort();                 \
  }

namespace shards {
#ifdef SH_COMPRESSED_STRINGS
SHOptionalString getCompiledCompressedString(uint32_t crc);
#else
SHOptionalString setCompiledCompressedString(uint32_t crc, const char *str);
#endif

SHString getString(uint32_t crc);
void setString(uint32_t crc, SHString str);
[[nodiscard]] SHComposeResult composeWire(const Shards wire, SHValidationCallback callback, void *userData, SHInstanceData data);
// caller does not handle return
SHWireState activateShards(SHSeq shards, SHContext *context, const SHVar &wireInput, SHVar &output);
// caller handles return
SHWireState activateShards2(SHSeq shards, SHContext *context, const SHVar &wireInput, SHVar &output);
// caller does not handle return
SHWireState activateShards(Shards shards, SHContext *context, const SHVar &wireInput, SHVar &output);
// caller handles return
SHWireState activateShards2(Shards shards, SHContext *context, const SHVar &wireInput, SHVar &output);
// caller does not handle return
SHWireState activateShards(Shards shards, SHContext *context, const SHVar &wireInput, SHVar &output, SHVar &outHash);
// caller handles return
SHWireState activateShards2(Shards shards, SHContext *context, const SHVar &wireInput, SHVar &output, SHVar &outHash);
SHVar *referenceGlobalVariable(SHContext *ctx, const char *name);
SHVar *referenceVariable(SHContext *ctx, const char *name);
void releaseVariable(SHVar *variable);
void setSharedVariable(const char *name, const SHVar &value);
void unsetSharedVariable(const char *name);
SHVar getSharedVariable(const char *name);
SHWireState suspend(SHContext *context, double seconds);
void registerEnumType(int32_t vendorId, int32_t enumId, SHEnumInfo info);

Shard *createShard(std::string_view name);
void registerCoreShards();
void registerShard(std::string_view name, SHShardConstructor constructor, std::string_view fullTypeName = std::string_view());
void registerObjectType(int32_t vendorId, int32_t typeId, SHObjectInfo info);
void registerEnumType(int32_t vendorId, int32_t typeId, SHEnumInfo info);
void registerRunLoopCallback(std::string_view eventName, SHCallback callback);
void unregisterRunLoopCallback(std::string_view eventName);
void registerExitCallback(std::string_view eventName, SHCallback callback);
void unregisterExitCallback(std::string_view eventName);
void callExitCallbacks();
void registerWire(SHWire *wire);
void unregisterWire(SHWire *wire);

struct RuntimeObserver {
  virtual void registerShard(const char *fullName, SHShardConstructor constructor) {}
  virtual void registerObjectType(int32_t vendorId, int32_t typeId, SHObjectInfo info) {}
  virtual void registerEnumType(int32_t vendorId, int32_t typeId, SHEnumInfo info) {}
};

inline void cloneVar(SHVar &dst, const SHVar &src);
inline void destroyVar(SHVar &src);

struct InternalCore;
using OwnedVar = TOwnedVar<InternalCore>;

#if SH_STACK_ALLOCATOR_SUPPORTED
// utility stack allocator (stolen from stackoverflow)
template <std::size_t Size = 512> struct bumping_memory_resource {
  char buffer[Size];
  std::size_t _used = 0;
  char *_ptr;

  explicit bumping_memory_resource() : _ptr(&buffer[0]) {}

  void *allocate(std::size_t size) {
    _used += size;
    if (_used > Size)
      throw shards::SHException("Stack allocator memory exhausted");

    auto ret = _ptr;
    _ptr += size;
    return ret;
  }

  void deallocate(void *) noexcept {}
};

template <typename T, typename Resource = bumping_memory_resource<512>> struct stack_allocator {
  Resource _res;
  using value_type = T;

  stack_allocator() {}

  stack_allocator(const stack_allocator &) = default;

  template <typename U> stack_allocator(const stack_allocator<U, Resource> &other) : _res(other._res) {}

  T *allocate(std::size_t n) { return static_cast<T *>(_res.allocate(sizeof(T) * n)); }
  void deallocate(T *ptr, std::size_t) { _res.deallocate(ptr); }

  friend bool operator==(const stack_allocator &lhs, const stack_allocator &rhs) { return lhs._res == rhs._res; }

  friend bool operator!=(const stack_allocator &lhs, const stack_allocator &rhs) { return lhs._res != rhs._res; }
};
#else
template <typename T> struct stack_allocator : public std::allocator<T> {
  using Base = std::allocator<T>;
  using Base::allocator;
};
#endif

void freeDerivedInfo(SHTypeInfo info);
SHTypeInfo deriveTypeInfo(const SHVar &value, const SHInstanceData &data, bool *containsVariables = nullptr);
SHTypeInfo cloneTypeInfo(const SHTypeInfo &other);

uint64_t deriveTypeHash(const SHVar &value);
uint64_t deriveTypeHash(const SHTypeInfo &value);

struct TypeInfo {
  TypeInfo() {}

  TypeInfo(const SHVar &var, const SHInstanceData &data, bool *containsVariables = nullptr) {
    _info = deriveTypeInfo(var, data, containsVariables);
  }

  TypeInfo(const SHTypeInfo &info) { _info = cloneTypeInfo(info); }

  TypeInfo(TypeInfo &&info) {
    _info = info._info;
    info._info = {};
  }

  TypeInfo &operator=(const SHTypeInfo &info) {
    freeDerivedInfo(_info);
    _info = cloneTypeInfo(info);
    return *this;
  }

  ~TypeInfo() { freeDerivedInfo(_info); }

  operator const SHTypeInfo &() { return _info; }

  const SHTypeInfo *operator->() const { return &_info; }

  const SHTypeInfo &operator*() const { return _info; }

private:
  SHTypeInfo _info{};
};
} // namespace shards

#ifndef __EMSCRIPTEN__
struct SHStackAllocator {
  size_t size{SH_BASE_STACK_SIZE};
  uint8_t *mem{nullptr};

  boost::context::stack_context allocate() {
    boost::context::stack_context ctx;
    ctx.size = size;
    ctx.sp = mem + size;
#if defined(BOOST_USE_VALGRIND)
    ctx.valgrind_stack_id = VALGRIND_STACK_REGISTER(ctx.sp, mem);
#endif
    return ctx;
  }

  void deallocate(boost::context::stack_context &sctx) {
#if defined(BOOST_USE_VALGRIND)
    VALGRIND_STACK_DEREGISTER(sctx.valgrind_stack_id);
#endif
  }
};
#endif

struct SHWire : public std::enable_shared_from_this<SHWire> {
  static std::shared_ptr<SHWire> make(std::string_view wire_name) { return std::shared_ptr<SHWire>(new SHWire(wire_name)); }

  static std::shared_ptr<SHWire> make() { return std::shared_ptr<SHWire>(new SHWire()); }

  static std::shared_ptr<SHWire> *makePtr(std::string_view wire_name) {
    return new std::shared_ptr<SHWire>(new SHWire(wire_name));
  }

  enum State { Stopped, Prepared, Starting, Iterating, IterationEnded, Failed, Ended };

  ~SHWire() {
    reset();
#ifdef SH_USE_TSAN
    if (tsan_coro) {
      __tsan_destroy_fiber(tsan_coro);
    }
#endif
    SHLOG_TRACE("Destroying wire {}", name);
  }

  void warmup(SHContext *context);

  void cleanup(bool force = false);

  // Also the wire takes ownership of the shard!
  void addShard(Shard *blk) {
    assert(!blk->owned);
    blk->owned = true;
    shards.push_back(blk);
  }

  // Also removes ownership of the shard
  void removeShard(Shard *blk) {
    auto findIt = std::find(shards.begin(), shards.end(), blk);
    if (findIt != shards.end()) {
      shards.erase(findIt);
      blk->owned = false;
    } else {
      throw shards::SHException("removeShard: shard not found!");
    }
  }

  // Attributes
  bool looped{false};
  bool unsafe{false};

  std::string name;

  std::optional<SHCoro> coro;
#ifdef SH_USE_TSAN
  void *tsan_coro{nullptr};
#endif

  std::atomic<State> state{Stopped};

  SHVar rootTickInput{};
  SHVar currentInput{};
  SHVar previousOutput{};

  shards::OwnedVar finishedOutput{};
  std::string finishedError{};

  SHVar composedHash{};
  bool warmedUp{false};
  bool isRoot{false};
  std::unordered_set<void *> wireUsers;

  // we need to clone this, as might disappear, since outside wire
  mutable shards::TypeInfo inputType{};
  // this one is a shard inside the wire, so won't disappear
  mutable SHTypeInfo outputType{};
  mutable std::vector<std::string> requiredVariables;

  SHContext *context{nullptr};
  SHWire *resumer{nullptr}; // used in Resume/Start shards

  std::weak_ptr<SHMesh> mesh;

  std::vector<Shard *> shards;

  std::unordered_map<std::string, SHVar, std::hash<std::string>, std::equal_to<std::string>,
                     boost::alignment::aligned_allocator<std::pair<const std::string, SHVar>, 16>>
      variables;

  // variables with lifetime managed externally
  std::unordered_map<std::string, SHVar *> externalVariables;
  // used only in the case of external variables
  std::unordered_map<uint64_t, shards::TypeInfo> typesCache;

  // this is the eventual coroutine stack memory buffer
  uint8_t *stackMem{nullptr};
  size_t stackSize{SH_BASE_STACK_SIZE};

  static std::shared_ptr<SHWire> sharedFromRef(SHWireRef ref) { return *reinterpret_cast<std::shared_ptr<SHWire> *>(ref); }

  static void deleteRef(SHWireRef ref) {
    auto pref = reinterpret_cast<std::shared_ptr<SHWire> *>(ref);
    SHLOG_TRACE("{} wire deleteRef - use_count: {}", (*pref)->name, pref->use_count());
    delete pref;
  }

  SHWireRef newRef() { return reinterpret_cast<SHWireRef>(new std::shared_ptr<SHWire>(shared_from_this())); }

  static SHWireRef weakRef(const std::shared_ptr<SHWire> &shared) {
    return reinterpret_cast<SHWireRef>(&const_cast<std::shared_ptr<SHWire> &>(shared));
  }

  static SHWireRef addRef(SHWireRef ref) {
    auto cref = sharedFromRef(ref);
    SHLOG_TRACE("{} wire addRef - use_count: {}", cref->name, cref.use_count());
    auto res = new std::shared_ptr<SHWire>(cref);
    return reinterpret_cast<SHWireRef>(res);
  }

  std::vector<std::function<void()>> onStart;
  std::vector<std::function<void()>> onStop;

private:
  SHWire(std::string_view wire_name) : name(wire_name) { SHLOG_TRACE("Creating wire: {}", name); }

  SHWire() { SHLOG_TRACE("Creating wire"); }

  void reset();
};

namespace shards {
using SHHashSet =
    std::unordered_set<OwnedVar, std::hash<SHVar>, std::equal_to<SHVar>, boost::alignment::aligned_allocator<OwnedVar, 16>>;
using SHHashSetIt = SHHashSet::iterator;

using SHMap = std::unordered_map<std::string, OwnedVar, std::hash<std::string>, std::equal_to<std::string>,
                                 boost::alignment::aligned_allocator<std::pair<const std::string, OwnedVar>, 16>>;
using SHMapIt = SHMap::iterator;

struct StopWireException : public SHException {
  StopWireException() : SHException("The wire has been stopped") {}
};
struct RestartWireException : public SHException {
  RestartWireException() : SHException("The wire has been restarted") {}
};

struct Globals {
  // sporadically used, don't abuse. And don't use in real time code.
  std::mutex GlobalMutex;
  std::unordered_map<std::string, OwnedVar> Settings;

  int SigIntTerm{0};
  std::unordered_map<std::string_view, SHShardConstructor> ShardsRegister;
  std::unordered_map<std::string_view, std::string_view> ShardNamesToFullTypeNames;
  std::unordered_map<int64_t, SHObjectInfo> ObjectTypesRegister;
  std::unordered_map<int64_t, SHEnumInfo> EnumTypesRegister;

  // map = ordered! we need that for those
  std::map<std::string_view, SHCallback> RunLoopHooks;
  std::map<std::string_view, SHCallback> ExitHooks;

  std::unordered_map<std::string, std::shared_ptr<SHWire>> GlobalWires;

  std::list<std::weak_ptr<RuntimeObserver>> Observers;

  std::string RootPath;
  std::string ExePath;

  std::exception_ptr StopWireEx;
  std::exception_ptr RestartWireEx;

  std::unordered_map<uint32_t, SHOptionalString> *CompressedStrings{nullptr};

  SHTableInterface TableInterface{
      .tableGetIterator =
          [](SHTable table, SHTableIterator *outIter) {
            if (outIter == nullptr)
              SHLOG_FATAL("tableGetIterator - outIter was nullptr");
            shards::SHMap *map = reinterpret_cast<shards::SHMap *>(table.opaque);
            shards::SHMapIt *mapIt = reinterpret_cast<shards::SHMapIt *>(outIter);
            *mapIt = map->begin();
          },
      .tableNext =
          [](SHTable table, SHTableIterator *inIter, SHString *outKey, SHVar *outVar) {
            if (inIter == nullptr)
              SHLOG_FATAL("tableGetIterator - inIter was nullptr");
            shards::SHMap *map = reinterpret_cast<shards::SHMap *>(table.opaque);
            shards::SHMapIt *mapIt = reinterpret_cast<shards::SHMapIt *>(inIter);
            if ((*mapIt) != map->end()) {
              *outKey = (*(*mapIt)).first.c_str();
              *outVar = (*(*mapIt)).second;
              (*mapIt)++;
              return true;
            } else {
              return false;
            }
          },
      .tableSize =
          [](SHTable table) {
            shards::SHMap *map = reinterpret_cast<shards::SHMap *>(table.opaque);
            return map->size();
          },
      .tableContains =
          [](SHTable table, const char *key) {
            shards::SHMap *map = reinterpret_cast<shards::SHMap *>(table.opaque);
            return map->count(key) > 0;
          },
      .tableAt =
          [](SHTable table, const char *key) {
            shards::SHMap *map = reinterpret_cast<shards::SHMap *>(table.opaque);
            SHVar &vref = (*map)[key];
            return &vref;
          },
      .tableRemove =
          [](SHTable table, const char *key) {
            shards::SHMap *map = reinterpret_cast<shards::SHMap *>(table.opaque);
            map->erase(key);
          },
      .tableClear =
          [](SHTable table) {
            shards::SHMap *map = reinterpret_cast<shards::SHMap *>(table.opaque);
            map->clear();
          },
      .tableFree =
          [](SHTable table) {
            shards::SHMap *map = reinterpret_cast<shards::SHMap *>(table.opaque);
            delete map;
          },
  };

  SHSetInterface SetInterface{
      .setGetIterator =
          [](SHSet shset, SHSetIterator *outIter) {
            if (outIter == nullptr)
              SHLOG_FATAL("setGetIterator - outIter was nullptr");
            shards::SHHashSet *set = reinterpret_cast<shards::SHHashSet *>(shset.opaque);
            shards::SHHashSetIt *setIt = reinterpret_cast<shards::SHHashSetIt *>(outIter);
            *setIt = set->begin();
          },
      .setNext =
          [](SHSet shset, SHSetIterator *inIter, SHVar *outVar) {
            if (inIter == nullptr)
              SHLOG_FATAL("setGetIterator - inIter was nullptr");
            shards::SHHashSet *set = reinterpret_cast<shards::SHHashSet *>(shset.opaque);
            shards::SHHashSetIt *setIt = reinterpret_cast<shards::SHHashSetIt *>(inIter);
            if ((*setIt) != set->end()) {
              *outVar = (*(*setIt));
              (*setIt)++;
              return true;
            } else {
              return false;
            }
          },
      .setSize =
          [](SHSet shset) {
            shards::SHHashSet *set = reinterpret_cast<shards::SHHashSet *>(shset.opaque);
            return set->size();
          },
      .setContains =
          [](SHSet shset, SHVar value) {
            shards::SHHashSet *set = reinterpret_cast<shards::SHHashSet *>(shset.opaque);
            return set->count(value) > 0;
          },
      .setInclude =
          [](SHSet shset, SHVar value) {
            shards::SHHashSet *set = reinterpret_cast<shards::SHHashSet *>(shset.opaque);
            auto [_, inserted] = set->emplace(value);
            return inserted;
          },
      .setExclude =
          [](SHSet shset, SHVar value) {
            shards::SHHashSet *set = reinterpret_cast<shards::SHHashSet *>(shset.opaque);
            return set->erase(value) > 0;
          },
      .setClear =
          [](SHSet shset) {
            shards::SHHashSet *set = reinterpret_cast<shards::SHHashSet *>(shset.opaque);
            set->clear();
          },
      .setFree =
          [](SHSet shset) {
            shards::SHHashSet *set = reinterpret_cast<shards::SHHashSet *>(shset.opaque);
            delete set;
          },
  };
};

Globals &GetGlobals();

template <typename T> inline void arrayGrow(T &arr, size_t addlen, size_t min_cap = 4) {
  // safety check to make sure this is not a borrowed foreign array!
  assert((arr.cap == 0 && arr.elements == nullptr) || (arr.cap > 0 && arr.elements != nullptr));

  size_t min_len = arr.len + addlen;

  // compute the minimum capacity needed
  if (min_len > min_cap)
    min_cap = min_len;

  if (min_cap <= arr.cap)
    return;

  // increase needed capacity to guarantee O(1) amortized
  if (min_cap < 2 * arr.cap)
    min_cap = 2 * arr.cap;

  // TODO investigate realloc
  auto newbuf = new (std::align_val_t{16}) uint8_t[sizeof(arr.elements[0]) * min_cap];
  if (arr.elements) {
    memcpy(newbuf, arr.elements, sizeof(arr.elements[0]) * arr.len);
    ::operator delete[](arr.elements, std::align_val_t{16});
  }
  arr.elements = (decltype(arr.elements))newbuf;

  // also memset to 0 new memory in order to make cloneVars valid on new items
  size_t size = sizeof(arr.elements[0]) * (min_cap - arr.len);
  memset(arr.elements + arr.len, 0x0, size);

  if (min_cap > UINT32_MAX) {
    // this is the case for now for many reasons, but should be just fine
    SHLOG_FATAL("Int array overflow, we don't support more then UINT32_MAX.");
  }
  arr.cap = uint32_t(min_cap);
}

template <typename T, typename V> inline void arrayPush(T &arr, const V &val) {
  if ((arr.len + 1) > arr.cap) {
    shards::arrayGrow(arr, 1);
  }
  arr.elements[arr.len++] = val;
}

template <typename T> inline void arrayResize(T &arr, uint32_t size) {
  if (arr.cap < size) {
    shards::arrayGrow(arr, size - arr.len);
  }
  arr.len = size;
}

template <typename T, typename V> inline void arrayInsert(T &arr, uint32_t index, const V &val) {
  if ((arr.len + 1) > arr.cap) {
    arrayGrow(arr, 1);
  }
  memmove(&arr.elements[index + 1], &arr.elements[index], sizeof(V) * (arr.len - index));
  arr.len++;
  arr.elements[index] = val;
}

template <typename T, typename V> inline V arrayPop(T &arr) {
  assert(arr.len > 0);
  arr.len--;
  return arr.elements[arr.len];
}

template <typename T> inline void arrayDelFast(T &arr, uint32_t index) {
  assert(arr.len > 0);
  arr.len--;
  // this allows eventual destroyVar/cloneVar magic
  // avoiding allocations even in nested seqs
  std::swap(arr.elements[index], arr.elements[arr.len]);
}

template <typename T> inline void arrayDel(T &arr, uint32_t index) {
  assert(arr.len > 0);
  // this allows eventual destroyVar/cloneVar magic
  // avoiding allocations even in nested seqs
  arr.len--;
  while (index < arr.len) {
    std::swap(arr.elements[index], arr.elements[index + 1]);
    index++;
  }
}

template <typename T> inline void arrayFree(T &arr) {
  if (arr.elements)
    ::operator delete[](arr.elements, std::align_val_t{16});
  memset(&arr, 0x0, sizeof(T));
}

template <typename T> class PtrIterator {
public:
  typedef T value_type;

  PtrIterator(T *ptr) : ptr(ptr) {}

  PtrIterator &operator+=(std::ptrdiff_t s) {
    ptr += s;
    return *this;
  }
  PtrIterator &operator-=(std::ptrdiff_t s) {
    ptr -= s;
    return *this;
  }

  PtrIterator operator+(std::ptrdiff_t s) {
    PtrIterator it(ptr);
    return it += s;
  }
  PtrIterator operator-(std::ptrdiff_t s) {
    PtrIterator it(ptr);
    return it -= s;
  }

  PtrIterator operator++() {
    ++ptr;
    return *this;
  }
  PtrIterator operator++(int s) {
    ptr += s;
    return *this;
  }

  PtrIterator operator--() {
    --ptr;
    return *this;
  }
  PtrIterator operator--(int s) {
    ptr -= s;
    return *this;
  }

  std::ptrdiff_t operator-(PtrIterator const &other) const { return ptr - other.ptr; }

  T &operator*() const { return *ptr; }

  bool operator==(const PtrIterator &rhs) const { return ptr == rhs.ptr; }
  bool operator!=(const PtrIterator &rhs) const { return ptr != rhs.ptr; }
  bool operator>(const PtrIterator &rhs) const { return ptr > rhs.ptr; }
  bool operator<(const PtrIterator &rhs) const { return ptr < rhs.ptr; }
  bool operator>=(const PtrIterator &rhs) const { return ptr >= rhs.ptr; }
  bool operator<=(const PtrIterator &rhs) const { return ptr <= rhs.ptr; }

private:
  T *ptr;
};

template <typename S, typename T, typename Allocator = std::allocator<T>> class IterableArray {
public:
  typedef S seq_type;
  typedef T value_type;
  typedef size_t size_type;

  typedef PtrIterator<T> iterator;
  typedef PtrIterator<const T> const_iterator;

  IterableArray() : _seq({}), _owned(true) {}

  // Not OWNING!
  IterableArray(const seq_type &seq) : _seq(seq), _owned(false) {}

  // implicit converter
  IterableArray(const SHVar &v) : _seq(v.payload.seqValue), _owned(false) { assert(v.valueType == Seq); }

  IterableArray(size_t s) : _seq({}), _owned(true) { arrayResize(_seq, s); }

  IterableArray(size_t s, T v) : _seq({}), _owned(true) {
    arrayResize(_seq, s);
    for (size_t i = 0; i < s; i++) {
      _seq[i] = v;
    }
  }

  IterableArray(iterator first, iterator last) : _seq({}), _owned(true) {
    size_t size = last - first;
    arrayResize(_seq, size);
    for (size_t i = 0; i < size; i++) {
      _seq.elements[i] = *(first + i);
    }
  }

  IterableArray(const_iterator first, const_iterator last) : _seq({}), _owned(true) {
    size_t size = last - first;
    arrayResize(_seq, size);
    for (size_t i = 0; i < size; i++) {
      _seq.elements[i] = *(first + i);
    }
  }

  IterableArray(const IterableArray &other) : _seq({}), _owned(true) {
    size_t size = other._seq.len;
    arrayResize(_seq, size);
    for (size_t i = 0; i < size; i++) {
      _seq.elements[i] = other._seq.elements[i];
    }
  }

  IterableArray &operator=(SHVar &var) {
    assert(var.valueType == Seq);
    _seq = var.payload.seqValue;
    _owned = false;
    return *this;
  }

  IterableArray &operator=(IterableArray &other) {
    _seq = other._seq;
    _owned = other._owned;
    return *this;
  }

  IterableArray &operator=(const IterableArray &other) {
    _seq = {};
    _owned = true;
    size_t size = other._seq.len;
    arrayResize(_seq, size);
    for (size_t i = 0; i < size; i++) {
      _seq.elements[i] = other._seq.elements[i];
    }
    return *this;
  }

  ~IterableArray() {
    if (_owned) {
      arrayFree(_seq);
    }
  }

  bool isOwned() const { return _owned; }

private:
  seq_type _seq;
  bool _owned;

public:
  iterator begin() { return iterator(&_seq.elements[0]); }
  const_iterator begin() const { return const_iterator(&_seq.elements[0]); }
  const_iterator shegin() const { return const_iterator(&_seq.elements[0]); }
  iterator end() { return iterator(&_seq.elements[0] + size()); }
  const_iterator end() const { return const_iterator(&_seq.elements[0] + size()); }
  const_iterator cend() const { return const_iterator(&_seq.elements[0] + size()); }
  T &operator[](int index) { return _seq.elements[index]; }
  const T &operator[](int index) const { return _seq.elements[index]; }
  T &front() { return _seq.elements[0]; }
  const T &front() const { return _seq.elements[0]; }
  T &back() { return _seq.elements[size() - 1]; }
  const T &back() const { return _seq.elements[size() - 1]; }
  T *data() { return _seq; }
  size_t size() const { return _seq.len; }
  bool empty() const { return _seq.elements == nullptr || size() == 0; }
  void resize(size_t nsize) { arrayResize(_seq, nsize); }
  void push_back(const T &value) { arrayPush(_seq, value); }
  void clear() { arrayResize(_seq, 0); }
  operator seq_type() const { return _seq; }
};

using IterableSeq = IterableArray<SHSeq, SHVar>;
using IterableExposedInfo = IterableArray<SHExposedTypesInfo, SHExposedTypeInfo>;
using IterableTypesInfo = IterableArray<SHTypesInfo, SHTypeInfo>;
} // namespace shards

// needed by some c++ library
namespace std {
template <typename T> class iterator_traits<shards::PtrIterator<T>> {
public:
  using difference_type = ptrdiff_t;
  using size_type = size_t;
  using value_type = T;
  using pointer = T *;
  using reference = T &;
  using iterator_category = random_access_iterator_tag;
};

inline void swap(SHVar &v1, SHVar &v2) {
  auto t = v1;
  v1 = v2;
  v2 = t;
}
} // namespace std

namespace shards {
NO_INLINE void _destroyVarSlow(SHVar &var);
NO_INLINE void _cloneVarSlow(SHVar &dst, const SHVar &src);

ALWAYS_INLINE inline void destroyVar(SHVar &var) {
  switch (var.valueType) {
  case Table:
  case SHType::Set:
  case Seq:
    _destroyVarSlow(var);
    break;
  case SHType::Path:
  case SHType::String:
  case ContextVar:
    delete[] var.payload.stringValue;
    break;
  case Image:
    delete[] var.payload.imageValue.data;
    break;
  case Audio:
    delete[] var.payload.audioValue.samples;
    break;
  case Bytes:
    delete[] var.payload.bytesValue;
    break;
  case SHType::Array:
    arrayFree(var.payload.arrayValue);
    break;
  case Object:
    if ((var.flags & SHVAR_FLAGS_USES_OBJINFO) == SHVAR_FLAGS_USES_OBJINFO && var.objectInfo && var.objectInfo->release) {
      // in this case the custom object needs actual destruction
      var.objectInfo->release(var.payload.objectValue);
    }
    break;
  case SHType::Wire:
    SHWire::deleteRef(var.payload.wireValue);
    break;
  default:
    break;
  };

  memset(&var.payload, 0x0, sizeof(SHVarPayload));
  var.valueType = SHType::None;
}

ALWAYS_INLINE inline void cloneVar(SHVar &dst, const SHVar &src) {
  if (src.valueType < EndOfBlittableTypes && dst.valueType < EndOfBlittableTypes) {
    dst.valueType = src.valueType;
    memcpy(&dst.payload, &src.payload, sizeof(SHVarPayload));
  } else if (src.valueType < EndOfBlittableTypes) {
    destroyVar(dst);
    dst.valueType = src.valueType;
    memcpy(&dst.payload, &src.payload, sizeof(SHVarPayload));
  } else {
    _cloneVarSlow(dst, src);
  }
}

struct InternalCore {
  // need to emulate dllshard Core a bit
  static SHTable tableNew() {
    SHTable res;
    res.api = &shards::GetGlobals().TableInterface;
    res.opaque = new shards::SHMap();
    return res;
  }

  static SHSet setNew() {
    SHSet res;
    res.api = &shards::GetGlobals().SetInterface;
    res.opaque = new shards::SHHashSet();
    return res;
  }

  static SHVar *referenceVariable(SHContext *context, const char *name) { return shards::referenceVariable(context, name); }

  static void releaseVariable(SHVar *variable) { shards::releaseVariable(variable); }

  static void cloneVar(SHVar &dst, const SHVar &src) { shards::cloneVar(dst, src); }

  static void destroyVar(SHVar &var) { shards::destroyVar(var); }

  template <typename T> static void arrayFree(T &arr) { shards::arrayFree<T>(arr); }

  static void seqPush(SHSeq *s, const SHVar *v) { arrayPush(*s, *v); }

  static void seqResize(SHSeq *s, uint32_t size) { arrayResize(*s, size); }

  static void expTypesFree(SHExposedTypesInfo &arr) { arrayFree(arr); }

  static void log(const char *msg) { SHLOG_INFO(msg); }

  static void registerEnumType(int32_t vendorId, int32_t enumId, SHEnumInfo info) {
    shards::registerEnumType(vendorId, enumId, info);
  }

  static void registerObjectType(int32_t vendorId, int32_t objectId, SHObjectInfo info) {
    shards::registerObjectType(vendorId, objectId, info);
  }

  static SHComposeResult composeShards(Shards shards, SHValidationCallback callback, void *userData, SHInstanceData data) {
    return shards::composeWire(shards, callback, userData, data);
  }

  static SHWireState runShards(Shards shards, SHContext *context, const SHVar &input, SHVar &output) {
    return shards::activateShards(shards, context, input, output);
  }

  static SHWireState runShards2(Shards shards, SHContext *context, const SHVar &input, SHVar &output) {
    return shards::activateShards2(shards, context, input, output);
  }

  static SHWireState runShardsHashed(Shards shards, SHContext *context, const SHVar &input, SHVar &output, SHVar &outHash) {
    return shards::activateShards(shards, context, input, output, outHash);
  }

  static SHWireState runShardsHashed2(Shards shards, SHContext *context, const SHVar &input, SHVar &output, SHVar &outHash) {
    return shards::activateShards2(shards, context, input, output, outHash);
  }

  static SHWireState suspend(SHContext *ctx, double seconds) { return shards::suspend(ctx, seconds); }
};

typedef TParamVar<InternalCore> ParamVar;

template <Parameters &Params, size_t NPARAMS, Type &InputType, Type &OutputType>
struct SimpleShard : public TSimpleShard<InternalCore, Params, NPARAMS, InputType, OutputType> {};

template <typename E> class EnumInfo : public TEnumInfo<InternalCore, E, false> {
public:
  EnumInfo(const char *name, int32_t vendorId, int32_t enumId) : TEnumInfo<InternalCore, E, false>(name, vendorId, enumId) {}
};

template <typename E> class FlagsInfo : public TEnumInfo<InternalCore, E, true> {
public:
  FlagsInfo(const char *name, int32_t vendorId, int32_t enumId) : TEnumInfo<InternalCore, E, true>(name, vendorId, enumId) {}
};

#define REGISTER_ENUM(_NAME_, _CC_)                                             \
  static constexpr uint32_t _NAME_##CC = _CC_;                                  \
  static inline EnumInfo<_NAME_> _NAME_##EnumInfo{#_NAME_, CoreCC, _NAME_##CC}; \
  static inline Type _NAME_##Type = Type::Enum(CoreCC, _NAME_##CC)

#define REGISTER_FLAGS(_NAME_, _CC_)                                             \
  static constexpr uint32_t _NAME_##CC = _CC_;                                   \
  static inline FlagsInfo<_NAME_> _NAME_##EnumInfo{#_NAME_, CoreCC, _NAME_##CC}; \
  static inline Type _NAME_##Type = Type::Enum(CoreCC, _NAME_##CC)

template <typename E> static E getFlags(SHVar var) {
  E flags{};
  switch (var.valueType) {
  case SHType::Enum:
    flags = E(var.payload.enumValue);
    break;
  case SHType::Seq: {
    assert(var.payload.seqValue.len == 0 || var.payload.seqValue.elements[0].valueType == SHType::Enum);
    for (uint32_t i = 0; i < var.payload.seqValue.len; i++) {
      // note: can't use namespace magic_enum::bitwise_operators because of
      // potential conflicts with other implementations
      flags = static_cast<E>(static_cast<magic_enum::underlying_type_t<E>>(flags) |
                             static_cast<magic_enum::underlying_type_t<E>>(var.payload.seqValue.elements[i].payload.enumValue));
    }
  } break;
  default:
    break;
  }
  return flags;
};

template <typename E, std::vector<uint8_t> (*Serializer)(const E &) = nullptr,
          E (*Deserializer)(const std::string_view &) = nullptr, void (*BeforeDelete)(const E &) = nullptr>
class ObjectVar : public TObjectVar<InternalCore, E, Serializer, Deserializer, BeforeDelete> {
public:
  ObjectVar(const char *name, int32_t vendorId, int32_t objectId)
      : TObjectVar<InternalCore, E, Serializer, Deserializer, BeforeDelete>(name, vendorId, objectId) {}
};

typedef TShardsVar<InternalCore> ShardsVar;

typedef TTableVar<InternalCore> TableVar;
typedef TSeqVar<InternalCore> SeqVar;

struct ParamsInfo {
  /*
   DO NOT USE ANYMORE, THIS IS DEPRECATED AND MESSY
   IT WAS USEFUL WHEN DYNAMIC ARRAYS WERE STB ARRAYS
   BUT NOW WE CAN JUST USE DESIGNATED/AGGREGATE INITIALIZERS
 */
  ParamsInfo(const ParamsInfo &other) {
    shards::arrayResize(_innerInfo, 0);
    for (uint32_t i = 0; i < other._innerInfo.len; i++) {
      shards::arrayPush(_innerInfo, other._innerInfo.elements[i]);
    }
  }

  ParamsInfo &operator=(const ParamsInfo &other) {
    _innerInfo = {};
    for (uint32_t i = 0; i < other._innerInfo.len; i++) {
      shards::arrayPush(_innerInfo, other._innerInfo.elements[i]);
    }
    return *this;
  }

  template <typename... Types> explicit ParamsInfo(SHParameterInfo first, Types... types) {
    std::vector<SHParameterInfo> vec = {first, types...};
    _innerInfo = {};
    for (auto pi : vec) {
      shards::arrayPush(_innerInfo, pi);
    }
  }

  template <typename... Types> explicit ParamsInfo(const ParamsInfo &other, SHParameterInfo first, Types... types) {
    _innerInfo = {};

    for (uint32_t i = 0; i < other._innerInfo.len; i++) {
      shards::arrayPush(_innerInfo, other._innerInfo.elements[i]);
    }

    std::vector<SHParameterInfo> vec = {first, types...};
    for (auto pi : vec) {
      shards::arrayPush(_innerInfo, pi);
    }
  }

  static SHParameterInfo Param(SHString name, SHOptionalString help, SHTypesInfo types) {
    SHParameterInfo res = {name, help, types};
    return res;
  }

  ~ParamsInfo() { shards::arrayFree(_innerInfo); }

  explicit operator SHParametersInfo() const { return _innerInfo; }

  SHParametersInfo _innerInfo{};
};

struct ExposedInfo {
  /*
   DO NOT USE ANYMORE, THIS IS DEPRECATED AND MESSY
   IT WAS USEFUL WHEN DYNAMIC ARRAYS WERE STB ARRAYS
   BUT NOW WE CAN JUST USE DESIGNATED/AGGREGATE INITIALIZERS
 */
  ExposedInfo() {}

  ExposedInfo(const ExposedInfo &other) {
    for (uint32_t i = 0; i < other._innerInfo.len; i++) {
      shards::arrayPush(_innerInfo, other._innerInfo.elements[i]);
    }
  }

  ExposedInfo &operator=(const ExposedInfo &other) {
    shards::arrayResize(_innerInfo, 0);
    for (uint32_t i = 0; i < other._innerInfo.len; i++) {
      shards::arrayPush(_innerInfo, other._innerInfo.elements[i]);
    }
    return *this;
  }

  template <typename... Types> explicit ExposedInfo(const ExposedInfo &other, Types... types) {
    for (uint32_t i = 0; i < other._innerInfo.len; i++) {
      shards::arrayPush(_innerInfo, other._innerInfo.elements[i]);
    }

    std::vector<SHExposedTypeInfo> vec = {types...};
    for (auto pi : vec) {
      shards::arrayPush(_innerInfo, pi);
    }
  }

  template <typename... Types> explicit ExposedInfo(const SHExposedTypesInfo other, Types... types) {
    for (uint32_t i = 0; i < other.len; i++) {
      shards::arrayPush(_innerInfo, other.elements[i]);
    }

    std::vector<SHExposedTypeInfo> vec = {types...};
    for (auto pi : vec) {
      shards::arrayPush(_innerInfo, pi);
    }
  }

  template <typename... Types> explicit ExposedInfo(const SHExposedTypeInfo first, Types... types) {
    std::vector<SHExposedTypeInfo> vec = {first, types...};
    for (auto pi : vec) {
      shards::arrayPush(_innerInfo, pi);
    }
  }

  constexpr static SHExposedTypeInfo Variable(SHString name, SHOptionalString help, SHTypeInfo type, bool isMutable = false,
                                              bool isTableField = false) {
    SHExposedTypeInfo res = {name, help, type, isMutable, false, isTableField, false};
    return res;
  }

  constexpr static SHExposedTypeInfo ProtectedVariable(SHString name, SHOptionalString help, SHTypeInfo type,
                                                       bool isMutable = false) {
    SHExposedTypeInfo res = {name, help, type, isMutable, true, false, false};
    return res;
  }

  constexpr static SHExposedTypeInfo GlobalVariable(SHString name, SHOptionalString help, SHTypeInfo type, bool isMutable = false,
                                                    bool isTableField = false) {
    SHExposedTypeInfo res = {name, help, type, isMutable, false, isTableField, true};
    return res;
  }

  ~ExposedInfo() { shards::arrayFree(_innerInfo); }

  explicit operator SHExposedTypesInfo() const { return _innerInfo; }

  SHExposedTypesInfo _innerInfo{};
};

struct CachedStreamBuf : std::streambuf {
  std::vector<char> data;

  void reset() { data.clear(); }

  std::streamsize xsputn(const char *s, std::streamsize n) override {
    data.insert(data.end(), &s[0], s + n);
    return n;
  }

  int overflow(int c) override {
    data.push_back(static_cast<char>(c));
    return 0;
  }

  void done() { data.push_back('\0'); }

  const char *str() { return &data[0]; }
};

struct StringStreamBuf : std::streambuf {
  StringStreamBuf(const std::string_view &s) : data(s) {}

  std::string_view data;
  uint32_t index{0};
  bool done{false};

  std::streamsize xsgetn(char *s, std::streamsize n) override {
    if (unlikely(done)) {
      return 0;
    } else if ((size_t(index) + n) > data.size()) {
      const auto len = data.size() - index;
      memcpy(s, &data[index], len);
      done = true; // flag to indicate we are done
      return len;
    } else {
      memcpy(s, &data[index], n);
      index += n;
      return n;
    }
  }

  int underflow() override {
    if (index >= data.size())
      return EOF;

    return data[index++];
  }
};

struct VarStringStream {
  CachedStreamBuf cache;
  SHVar previousValue{};

  ~VarStringStream() { destroyVar(previousValue); }

  void write(const SHVar &var) {
    if (var != previousValue) {
      cache.reset();
      std::ostream stream(&cache);
      stream << var;
      cache.done();
      cloneVar(previousValue, var);
    }
  }

  void tryWriteHex(const SHVar &var) {
    if (var != previousValue) {
      cache.reset();
      std::ostream stream(&cache);
      if (var.valueType == Int) {
        stream << "0x" << std::hex << std::setw(2) << std::setfill('0') << var.payload.intValue;
      } else if (var.valueType == Bytes) {
        stream << "0x" << std::hex;
        for (uint32_t i = 0; i < var.payload.bytesSize; i++)
          stream << std::setw(2) << std::setfill('0') << (int)var.payload.bytesValue[i];
      } else if (var.valueType == String) {
        stream << "0x" << std::hex;
        auto len = var.payload.stringLen;
        if (len == 0 && var.payload.stringValue) {
          len = strlen(var.payload.stringValue);
        }
        for (uint32_t i = 0; i < len; i++)
          stream << std::setw(2) << std::setfill('0') << (int)var.payload.stringValue[i];
      } else {
        throw ActivationError("Cannot convert type to hex");
      }
      cache.done();
      cloneVar(previousValue, var);
    }
  }

  const char *str() { return cache.str(); }
};

using ShardsCollection = std::variant<const SHWire *, ShardPtr, Shards, SHVar>;

struct ShardInfo {
  ShardInfo(const std::string_view &name, const Shard *shard) : name(name), shard(shard) {}
  std::string_view name;
  const Shard *shard;
};

void gatherShards(const ShardsCollection &coll, std::vector<ShardInfo> &out);

struct VariableResolver {
  // this an utility to resolve nested variables, like we do in Const, Match etc

  std::vector<SHVar *> _vals;
  std::vector<SHVar *> _refs;

  void warmup(const SHVar &base, SHVar &slot, SHContext *context) {
    if (base.valueType == SHType::ContextVar) {
      _refs.emplace_back(referenceVariable(context, base.payload.stringValue));
      _vals.emplace_back(&slot);
    } else if (base.valueType == SHType::Seq) {
      for (uint32_t i = 0; i < base.payload.seqValue.len; i++) {
        warmup(base.payload.seqValue.elements[i], slot.payload.seqValue.elements[i], context);
      }
    } else if (base.valueType == SHType::Table) {
      ForEach(base.payload.tableValue, [&](auto key, auto &val) {
        auto vptr = slot.payload.tableValue.api->tableAt(slot.payload.tableValue, key);
        warmup(val, *vptr, context);
      });
    }
  }

  void cleanup() {
    if (_refs.size() > 0) {
      for (auto val : _vals) {
        // we do this to avoid double freeing, we don't really own this value
        *val = shards::Var::Empty;
      }
      for (auto ref : _refs) {
        releaseVariable(ref);
      }
      _refs.clear();
      _vals.clear();
    }
  }

  void reassign() {
    size_t len = _refs.size();
    for (size_t i = 0; i < len; i++) {
      *_vals[i] = *_refs[i];
    }
  }
};
}; // namespace shards

#endif // SH_CORE_FOUNDATION
