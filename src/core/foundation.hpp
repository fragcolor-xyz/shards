/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#ifndef CB_CORE_HPP
#define CB_CORE_HPP

#ifdef CB_USE_TSAN
extern "C" {
void *__tsan_get_current_fiber(void);
void *__tsan_create_fiber(unsigned flags);
void __tsan_destroy_fiber(void *fiber);
void __tsan_switch_to_fiber(void *fiber, unsigned flags);
void __tsan_set_fiber_name(void *fiber, const char *name);
const unsigned __tsan_switch_to_fiber_no_sync = 1 << 0;
}
#endif

#include "chainblocks.h"
#include "ops_internal.hpp"
#include <chainblocks.hpp>

// Included 3rdparty
#include "easylogging++.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <deque>
#include <list>
#include <set>
#include <type_traits>
#include <unordered_set>
#include <variant>

#include "blockwrapper.hpp"

// Needed specially for win32/32bit
#include <boost/align/aligned_allocator.hpp>

#ifndef __EMSCRIPTEN__
// For coroutines/context switches
#include <boost/context/continuation.hpp>
typedef boost::context::continuation CBCoro;
#else
#include <emscripten/fiber.h>
struct CBCoro {
  static constexpr int stack_size = 128 * 1024;
  static constexpr int as_stack_size = 4096;

  CBCoro();
  void init(const std::function<void(CBChain *, CBFlow *, CBCoro *)> &func,
            CBChain *chain, CBFlow *flow);
  NO_INLINE void resume();
  NO_INLINE void yield();

  // compatibility with boost
  operator bool() const { return true; }

  emscripten_fiber_t em_fiber;
  std::function<void(CBChain *, CBFlow *, CBCoro *)> func;
  CBChain *chain;
  CBFlow *flow;
  uint8_t asyncify_stack[as_stack_size];
  alignas(16) uint8_t c_stack[stack_size];
};
#endif

#define TRACE_LINE LOG(TRACE) << "#trace#"

CBValidationResult composeChain(const CBlocks chain,
                                CBValidationCallback callback, void *userData,
                                CBInstanceData data);

namespace chainblocks {
constexpr uint32_t FragCC = 'sink'; // 1718772071

CBChainState activateBlocks(CBSeq blocks, CBContext *context,
                            const CBVar &chainInput, CBVar &output,
                            const bool handlesReturn = false);
CBChainState activateBlocks(CBlocks blocks, CBContext *context,
                            const CBVar &chainInput, CBVar &output,
                            const bool handlesReturn = false);
CBVar *referenceGlobalVariable(CBContext *ctx, const char *name);
CBVar *referenceVariable(CBContext *ctx, const char *name);
void releaseVariable(CBVar *variable);
CBChainState suspend(CBContext *context, double seconds);
void registerEnumType(int32_t vendorId, int32_t enumId, CBEnumInfo info);

CBlock *createBlock(std::string_view name);
void registerCoreBlocks();
void registerBlock(std::string_view name, CBBlockConstructor constructor,
                   std::string_view fullTypeName = std::string_view());
void registerObjectType(int32_t vendorId, int32_t typeId, CBObjectInfo info);
void registerEnumType(int32_t vendorId, int32_t typeId, CBEnumInfo info);
void registerRunLoopCallback(std::string_view eventName, CBCallback callback);
void unregisterRunLoopCallback(std::string_view eventName);
void registerExitCallback(std::string_view eventName, CBCallback callback);
void unregisterExitCallback(std::string_view eventName);
void callExitCallbacks();
void registerChain(CBChain *chain);
void unregisterChain(CBChain *chain);

struct RuntimeObserver {
  virtual void registerBlock(const char *fullName,
                             CBBlockConstructor constructor) {}
  virtual void registerObjectType(int32_t vendorId, int32_t typeId,
                                  CBObjectInfo info) {}
  virtual void registerEnumType(int32_t vendorId, int32_t typeId,
                                CBEnumInfo info) {}
};

inline void cloneVar(CBVar &dst, const CBVar &src);
inline void destroyVar(CBVar &src);

struct InternalCore;
using OwnedVar = TOwnedVar<InternalCore>;
} // namespace chainblocks

struct CBChain : public std::enable_shared_from_this<CBChain> {
  static std::shared_ptr<CBChain> make(std::string_view chain_name) {
    return std::shared_ptr<CBChain>(new CBChain(chain_name));
  }

  static std::shared_ptr<CBChain> *makePtr(std::string_view chain_name) {
    return new std::shared_ptr<CBChain>(new CBChain(chain_name));
  }

  enum State {
    Stopped,
    Prepared,
    Starting,
    Iterating,
    IterationEnded,
    Failed,
    Ended
  };

  ~CBChain() {
    reset();
#ifdef CB_USE_TSAN
    if (tsan_coro) {
      __tsan_destroy_fiber(tsan_coro);
    }
#endif
    LOG(TRACE) << "~CBChain() " << name;
  }

  void warmup(CBContext *context);

  void cleanup(bool force = false);

  // Also the chain takes ownership of the block!
  void addBlock(CBlock *blk) {
    assert(!blk->owned);
    blk->owned = true;
    blocks.push_back(blk);
  }

  // Also removes ownership of the block
  void removeBlock(CBlock *blk) {
    auto findIt = std::find(blocks.begin(), blocks.end(), blk);
    if (findIt != blocks.end()) {
      blocks.erase(findIt);
      blk->owned = false;
    } else {
      throw chainblocks::CBException("removeBlock: block not found!");
    }
  }

  // Attributes
  bool looped{false};
  bool unsafe{false};

  std::string name;

  CBCoro *coro{nullptr};
#ifdef CB_USE_TSAN
  void *tsan_coro{nullptr};
#endif

  std::atomic<State> state{Stopped};

  CBVar rootTickInput{};
  CBVar previousOutput{};

  chainblocks::OwnedVar finishedOutput{};

  // those are mostly used internally in chains.cpp
  std::size_t composedHash{};
  bool warmedUp{false};
  std::unordered_set<void *> chainUsers;

  mutable CBTypeInfo inputType{};
  mutable CBTypeInfo outputType{};
  mutable std::vector<std::string> requiredVariables;

  CBContext *context{nullptr};

  std::weak_ptr<CBNode> node;

  std::vector<CBlock *> blocks;

  std::unordered_map<std::string, CBVar, std::hash<std::string>,
                     std::equal_to<std::string>,
                     boost::alignment::aligned_allocator<
                         std::pair<const std::string, CBVar>, 16>>
      variables;

  static std::shared_ptr<CBChain> sharedFromRef(CBChainRef ref) {
    return *reinterpret_cast<std::shared_ptr<CBChain> *>(ref);
  }

  static void deleteRef(CBChainRef ref) {
    auto pref = reinterpret_cast<std::shared_ptr<CBChain> *>(ref);
    LOG(TRACE) << (*pref)->name
               << " chain deleteRef - use_count: " << pref->use_count();
    delete pref;
  }

  CBChainRef newRef() {
    return reinterpret_cast<CBChainRef>(
        new std::shared_ptr<CBChain>(shared_from_this()));
  }

  static CBChainRef weakRef(std::shared_ptr<CBChain> &shared) {
    return reinterpret_cast<CBChainRef>(&shared);
  }

  static CBChainRef addRef(CBChainRef ref) {
    auto cref = sharedFromRef(ref);
    LOG(TRACE) << cref->name
               << " chain addRef - use_count: " << cref.use_count();
    auto res = new std::shared_ptr<CBChain>(cref);
    return reinterpret_cast<CBChainRef>(res);
  }

private:
  CBChain(std::string_view chain_name) : name(chain_name) {
    LOG(TRACE) << "CBChain() " << name;
  }

  void reset();
};

namespace chainblocks {
using CBMap = std::unordered_map<
    std::string, OwnedVar, std::hash<std::string>, std::equal_to<std::string>,
    boost::alignment::aligned_allocator<std::pair<const std::string, OwnedVar>,
                                        16>>;
using CBMapIt = std::unordered_map<
    std::string, OwnedVar, std::hash<std::string>, std::equal_to<std::string>,
    boost::alignment::aligned_allocator<std::pair<const std::string, OwnedVar>,
                                        16>>::iterator;

struct Globals {
  static inline std::unordered_map<std::string_view, CBBlockConstructor>
      BlocksRegister;
  static inline std::unordered_map<std::string_view, std::string_view>
      BlockNamesToFullTypeNames;
  static inline std::unordered_map<int64_t, CBObjectInfo> ObjectTypesRegister;
  static inline std::unordered_map<int64_t, CBEnumInfo> EnumTypesRegister;

  // map = ordered! we need that for those
  static inline std::map<std::string_view, CBCallback> RunLoopHooks;
  static inline std::map<std::string_view, CBCallback> ExitHooks;

  static inline std::unordered_map<std::string, std::shared_ptr<CBChain>>
      GlobalChains;

  static inline std::list<std::weak_ptr<RuntimeObserver>> Observers;

  static inline std::string RootPath;
  static inline std::string ExePath;

  static inline CBTableInterface TableInterface{
      .tableForEach =
          [](CBTable table, CBTableForEachCallback cb, void *data) {
            chainblocks::CBMap *map =
                reinterpret_cast<chainblocks::CBMap *>(table.opaque);
            for (auto &entry : *map) {
              if (!cb(entry.first.c_str(), &entry.second, data))
                break;
            }
          },
      .tableSize =
          [](CBTable table) {
            chainblocks::CBMap *map =
                reinterpret_cast<chainblocks::CBMap *>(table.opaque);
            return map->size();
          },
      .tableContains =
          [](CBTable table, const char *key) {
            chainblocks::CBMap *map =
                reinterpret_cast<chainblocks::CBMap *>(table.opaque);
            return map->count(key) > 0;
          },
      .tableAt =
          [](CBTable table, const char *key) {
            chainblocks::CBMap *map =
                reinterpret_cast<chainblocks::CBMap *>(table.opaque);
            CBVar &vref = (*map)[key];
            return &vref;
          },
      .tableRemove =
          [](CBTable table, const char *key) {
            chainblocks::CBMap *map =
                reinterpret_cast<chainblocks::CBMap *>(table.opaque);
            map->erase(key);
          },
      .tableClear =
          [](CBTable table) {
            chainblocks::CBMap *map =
                reinterpret_cast<chainblocks::CBMap *>(table.opaque);
            map->clear();
          },
      .tableFree =
          [](CBTable table) {
            chainblocks::CBMap *map =
                reinterpret_cast<chainblocks::CBMap *>(table.opaque);
            delete map;
          }};
};

template <typename T>
NO_INLINE void arrayGrow(T &arr, size_t addlen, size_t min_cap = 4);

template <typename T, typename V> inline void arrayPush(T &arr, const V &val) {
  if ((arr.len + 1) > arr.cap) {
    arrayGrow(arr, 1);
  }
  arr.elements[arr.len++] = val;
}

template <typename T> inline void arrayResize(T &arr, uint32_t size) {
  if (arr.cap < size) {
    arrayGrow(arr, size - arr.len);
  }
  arr.len = size;
}

template <typename T, typename V>
inline void arrayInsert(T &arr, uint32_t index, const V &val) {
  if ((arr.len + 1) > arr.cap) {
    arrayGrow(arr, 1);
  }
  memmove(&arr.elements[index + 1], &arr.elements[index],
          sizeof(V) * (arr.len - index));
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

  std::ptrdiff_t operator-(PtrIterator const &other) const {
    return ptr - other.ptr;
  }

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

template <typename S, typename T, typename Allocator = std::allocator<T>>
class IterableArray {
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
  IterableArray(const CBVar &v) : _seq(v.payload.seqValue), _owned(false) {
    assert(v.valueType == Seq);
  }

  IterableArray(size_t s) : _seq({}), _owned(true) { arrayResize(_seq, s); }

  IterableArray(size_t s, T v) : _seq({}), _owned(true) {
    arrayResize(_seq, s);
    for (size_t i = 0; i < s; i++) {
      _seq[i] = v;
    }
  }

  IterableArray(const_iterator first, const_iterator last)
      : _seq({}), _owned(true) {
    size_t size = last - first;
    arrayResize(_seq, size);
    for (size_t i = 0; i < size; i++) {
      _seq[i] = *first++;
    }
  }

  IterableArray(const IterableArray &other) : _seq({}), _owned(true) {
    size_t size = other._seq.len;
    arrayResize(_seq, size);
    for (size_t i = 0; i < size; i++) {
      _seq.elements[i] = other._seq.elements[i];
    }
  }

  IterableArray &operator=(IterableArray &other) {
    std::swap(_seq, other._seq);
    std::swap(_owned, other._owned);
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

private:
  seq_type _seq;
  bool _owned;

public:
  iterator begin() { return iterator(&_seq.elements[0]); }
  const_iterator begin() const { return const_iterator(&_seq.elements[0]); }
  const_iterator cbegin() const { return const_iterator(&_seq.elements[0]); }
  iterator end() { return iterator(&_seq.elements[0] + size()); }
  const_iterator end() const {
    return const_iterator(&_seq.elements[0] + size());
  }
  const_iterator cend() const {
    return const_iterator(&_seq.elements[0] + size());
  }
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

using IterableSeq = IterableArray<CBSeq, CBVar>;
using IterableExposedInfo =
    IterableArray<CBExposedTypesInfo, CBExposedTypeInfo>;
} // namespace chainblocks

// needed by some c++ library
namespace std {
template <typename T> class iterator_traits<chainblocks::PtrIterator<T>> {
public:
  using difference_type = ptrdiff_t;
  using size_type = size_t;
  using value_type = T;
  using pointer = T *;
  using reference = T &;
  using iterator_category = random_access_iterator_tag;
};

inline void swap(CBVar &v1, CBVar &v2) {
  auto t = v1;
  v1 = v2;
  v2 = t;
}
} // namespace std

namespace chainblocks {
NO_INLINE void _destroyVarSlow(CBVar &var);
NO_INLINE void _cloneVarSlow(CBVar &dst, const CBVar &src);

inline void destroyVar(CBVar &var) {
  switch (var.valueType) {
  case Table:
  case Seq:
    _destroyVarSlow(var);
    break;
  case CBType::Path:
  case CBType::String:
  case ContextVar:
    delete[] var.payload.stringValue;
    break;
  case Image:
    delete[] var.payload.imageValue.data;
    break;
  case Bytes:
    delete[] var.payload.bytesValue;
    break;
  case CBType::Array:
    arrayFree(var.payload.arrayValue);
    break;
  case Object:
    if ((var.flags & CBVAR_FLAGS_USES_OBJINFO) == CBVAR_FLAGS_USES_OBJINFO &&
        var.objectInfo && var.objectInfo->release) {
      // in this case the custom object needs actual destruction
      var.objectInfo->release(var.payload.objectValue);
    }
    break;
  case CBType::Chain:
    CBChain::deleteRef(var.payload.chainValue);
    break;
  default:
    break;
  };

  memset(&var.payload, 0x0, sizeof(CBVarPayload));
  var.valueType = CBType::None;
}

ALWAYS_INLINE inline void cloneVar(CBVar &dst, const CBVar &src) {
  if (src.valueType < EndOfBlittableTypes &&
      dst.valueType < EndOfBlittableTypes) {
    dst.valueType = src.valueType;
    memcpy(&dst.payload, &src.payload, sizeof(CBVarPayload));
  } else if (src.valueType < EndOfBlittableTypes) {
    destroyVar(dst);
    dst.valueType = src.valueType;
    memcpy(&dst.payload, &src.payload, sizeof(CBVarPayload));
  } else {
    _cloneVarSlow(dst, src);
  }
}

struct InternalCore {
  // need to emulate dllblock Core a bit
  static CBVar *referenceVariable(CBContext *context, const char *name) {
    return chainblocks::referenceVariable(context, name);
  }

  static void releaseVariable(CBVar *variable) {
    chainblocks::releaseVariable(variable);
  }

  static void cloneVar(CBVar &dst, const CBVar &src) {
    chainblocks::cloneVar(dst, src);
  }

  static void destroyVar(CBVar &var) { chainblocks::destroyVar(var); }

  template <typename T> static void arrayFree(T &arr) {
    chainblocks::arrayFree<T>(arr);
  }

  static void expTypesFree(CBExposedTypesInfo &arr) { arrayFree(arr); }

  [[noreturn]] static void throwException(const char *msg) {
    throw chainblocks::CBException(msg);
  }

  static void log(const char *msg) { LOG(INFO) << msg; }

  static void registerEnumType(int32_t vendorId, int32_t enumId,
                               CBEnumInfo info) {
    chainblocks::registerEnumType(vendorId, enumId, info);
  }

  static CBValidationResult validateBlocks(CBlocks blocks,
                                           CBValidationCallback callback,
                                           void *userData,
                                           CBInstanceData data) {
    return composeChain(blocks, callback, userData, data);
  }

  static CBChainState runBlocks(CBlocks blocks, CBContext *context, CBVar input,
                                CBVar *output, const CBBool handleReturn) {
    return chainblocks::activateBlocks(blocks, context, input, *output,
                                       handleReturn);
  }

  static CBChainState suspend(CBContext *ctx, double seconds) {
    return chainblocks::suspend(ctx, seconds);
  }
};

typedef TParamVar<InternalCore> ParamVar;

template <Parameters &Params, size_t NPARAMS, Type &InputType, Type &OutputType>
struct SimpleBlock : public TSimpleBlock<InternalCore, Params, NPARAMS,
                                         InputType, OutputType> {};

template <typename E> class EnumInfo : public TEnumInfo<InternalCore, E> {
public:
  EnumInfo(const char *name, int32_t vendorId, int32_t enumId)
      : TEnumInfo<InternalCore, E>(name, vendorId, enumId) {}
};

typedef TBlocksVar<InternalCore> BlocksVar;

struct ParamsInfo {
  /*
   DO NOT USE ANYMORE, THIS IS DEPRECATED AND MESSY
   IT WAS USEFUL WHEN DYNAMIC ARRAYS WERE STB ARRAYS
   BUT NOW WE CAN JUST USE DESIGNATED/AGGREGATE INITIALIZERS
 */
  ParamsInfo(const ParamsInfo &other) {
    chainblocks::arrayResize(_innerInfo, 0);
    for (uint32_t i = 0; i < other._innerInfo.len; i++) {
      chainblocks::arrayPush(_innerInfo, other._innerInfo.elements[i]);
    }
  }

  ParamsInfo &operator=(const ParamsInfo &other) {
    _innerInfo = {};
    for (uint32_t i = 0; i < other._innerInfo.len; i++) {
      chainblocks::arrayPush(_innerInfo, other._innerInfo.elements[i]);
    }
    return *this;
  }

  template <typename... Types>
  explicit ParamsInfo(CBParameterInfo first, Types... types) {
    std::vector<CBParameterInfo> vec = {first, types...};
    _innerInfo = {};
    for (auto pi : vec) {
      chainblocks::arrayPush(_innerInfo, pi);
    }
  }

  template <typename... Types>
  explicit ParamsInfo(const ParamsInfo &other, CBParameterInfo first,
                      Types... types) {
    _innerInfo = {};

    for (uint32_t i = 0; i < other._innerInfo.len; i++) {
      chainblocks::arrayPush(_innerInfo, other._innerInfo.elements[i]);
    }

    std::vector<CBParameterInfo> vec = {first, types...};
    for (auto pi : vec) {
      chainblocks::arrayPush(_innerInfo, pi);
    }
  }

  static CBParameterInfo Param(const char *name, const char *help,
                               CBTypesInfo types) {
    CBParameterInfo res = {name, help, types};
    return res;
  }

  ~ParamsInfo() {
    if (_innerInfo.elements)
      chainblocks::arrayFree(_innerInfo);
  }

  explicit operator CBParametersInfo() const { return _innerInfo; }

  CBParametersInfo _innerInfo{};
};

struct ExposedInfo {
  /*
   DO NOT USE ANYMORE, THIS IS DEPRECATED AND MESSY
   IT WAS USEFUL WHEN DYNAMIC ARRAYS WERE STB ARRAYS
   BUT NOW WE CAN JUST USE DESIGNATED/AGGREGATE INITIALIZERS
 */
  ExposedInfo() { _innerInfo = {}; }

  ExposedInfo(const ExposedInfo &other) {
    _innerInfo = {};
    for (uint32_t i = 0; i < other._innerInfo.len; i++) {
      chainblocks::arrayPush(_innerInfo, other._innerInfo.elements[i]);
    }
  }

  ExposedInfo &operator=(const ExposedInfo &other) {
    chainblocks::arrayResize(_innerInfo, 0);
    for (uint32_t i = 0; i < other._innerInfo.len; i++) {
      chainblocks::arrayPush(_innerInfo, other._innerInfo.elements[i]);
    }
    return *this;
  }

  template <typename... Types>
  explicit ExposedInfo(const ExposedInfo &other, Types... types) {
    _innerInfo = {};

    for (uint32_t i = 0; i < other._innerInfo.len; i++) {
      chainblocks::arrayPush(_innerInfo, other._innerInfo.elements[i]);
    }

    std::vector<CBExposedTypeInfo> vec = {types...};
    for (auto pi : vec) {
      chainblocks::arrayPush(_innerInfo, pi);
    }
  }

  template <typename... Types>
  explicit ExposedInfo(const CBExposedTypesInfo other, Types... types) {
    _innerInfo = {};

    for (uint32_t i = 0; i < other.len; i++) {
      chainblocks::arrayPush(_innerInfo, other.elements[i]);
    }

    std::vector<CBExposedTypeInfo> vec = {types...};
    for (auto pi : vec) {
      chainblocks::arrayPush(_innerInfo, pi);
    }
  }

  template <typename... Types>
  explicit ExposedInfo(const CBExposedTypeInfo first, Types... types) {
    std::vector<CBExposedTypeInfo> vec = {first, types...};
    _innerInfo = {};
    for (auto pi : vec) {
      chainblocks::arrayPush(_innerInfo, pi);
    }
  }

  static CBExposedTypeInfo Variable(const char *name, const char *help,
                                    CBTypeInfo type, bool isMutable = false,
                                    bool isTableField = false) {
    CBExposedTypeInfo res = {name, help, type, isMutable, isTableField, false};
    return res;
  }

  static CBExposedTypeInfo GlobalVariable(const char *name, const char *help,
                                          CBTypeInfo type,
                                          bool isMutable = false,
                                          bool isTableField = false) {
    CBExposedTypeInfo res = {name, help, type, isMutable, isTableField, true};
    return res;
  }

  ~ExposedInfo() {
    if (_innerInfo.elements)
      chainblocks::arrayFree(_innerInfo);
  }

  explicit operator CBExposedTypesInfo() const { return _innerInfo; }

  CBExposedTypesInfo _innerInfo;
};

struct CachedStreamBuf : std::streambuf {
  std::vector<char> data;
  void reset() { data.clear(); }

  int overflow(int c) override {
    data.push_back(static_cast<char>(c));
    return 0;
  }

  void done() { data.push_back('\0'); }

  const char *str() { return &data[0]; }
};

struct VarStringStream {
  CachedStreamBuf cache;
  CBVar previousValue{};

  ~VarStringStream() { destroyVar(previousValue); }

  void write(const CBVar &var) {
    if (var != previousValue) {
      cache.reset();
      std::ostream stream(&cache);
      stream << var;
      cache.done();
      cloneVar(previousValue, var);
    }
  }

  void tryWriteHex(const CBVar &var) {
    if (var != previousValue) {
      cache.reset();
      std::ostream stream(&cache);
      if (var.valueType == Int) {
        stream << "0x" << std::hex << var.payload.intValue << std::dec;
      } else {
        stream << var;
      }
      cache.done();
      cloneVar(previousValue, var);
    }
  }

  const char *str() { return cache.str(); }
};

using BlocksCollection =
    std::variant<const CBChain *, CBlockPtr, CBlocks, CBVar>;

struct CBlockInfo {
  CBlockInfo(const std::string_view &name, const CBlock *block)
      : name(name), block(block) {}
  std::string_view name;
  const CBlock *block;
};

void gatherBlocks(const BlocksCollection &coll, std::vector<CBlockInfo> &out);
}; // namespace chainblocks

#endif
