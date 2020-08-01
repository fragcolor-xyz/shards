/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

/*
Utility to auto load auto discover blocks from DLLs
All that is needed is to declare a chainblocks::registerBlocks
At runtime just dlopen the dll, that's it!
*/

#ifndef CB_DLLBLOCK_HPP
#define CB_DLLBLOCK_HPP

#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif
#include <cassert>
#include <functional>

#include "blockwrapper.hpp"
#include "common_types.hpp"

namespace chainblocks {
// this must be defined in the external
extern void registerBlocks();

struct CoreLoader {
  CBCore _core;

  CoreLoader() {
    CBChainblocksInterface ifaceproc;
#ifdef _WIN32
    auto handle = GetModuleHandle(NULL);
    ifaceproc =
        (CBChainblocksInterface)GetProcAddress(handle, "chainblocksInterface");
#else
    auto handle = dlopen(NULL, RTLD_NOW);
    ifaceproc = (CBChainblocksInterface)dlsym(handle, "chainblocksInterface");
#endif

    if (!ifaceproc) {
      // try again.. see if we are libcb
#ifdef _WIN32
      handle = GetModuleHandleA("libcb.dll");
      if (handle)
        ifaceproc = (CBChainblocksInterface)GetProcAddress(
            handle, "chainblocksInterface");
#else
      handle = dlopen("libcb.so", RTLD_NOW);
      if (handle)
        ifaceproc =
            (CBChainblocksInterface)dlsym(handle, "chainblocksInterface");
#endif
    }
    assert(ifaceproc);
    assert(ifaceproc(CHAINBLOCKS_CURRENT_ABI, &_core));
    _core.log("loading external blocks...");
    registerBlocks();
  }
};

class Core {
public:
  static void registerBlock(const char *fullName,
                            CBBlockConstructor constructor) {
    sCore._core.registerBlock(fullName, constructor);
  }

  static void registerObjectType(int32_t vendorId, int32_t typeId,
                                 CBObjectInfo info) {
    sCore._core.registerObjectType(vendorId, typeId, info);
  }

  static void registerEnumType(int32_t vendorId, int32_t typeId,
                               CBEnumInfo info) {
    sCore._core.registerEnumType(vendorId, typeId, info);
  }

  static void registerRunLoopCallback(const char *eventName,
                                      CBCallback callback) {
    sCore._core.registerRunLoopCallback(eventName, callback);
  }

  static void registerExitCallback(const char *eventName, CBCallback callback) {
    sCore._core.registerExitCallback(eventName, callback);
  }

  static void unregisterRunLoopCallback(const char *eventName) {
    sCore._core.unregisterRunLoopCallback(eventName);
  }

  static void unregisterExitCallback(const char *eventName) {
    sCore._core.unregisterExitCallback(eventName);
  }

  static CBVar *referenceVariable(CBContext *context, const char *name) {
    return sCore._core.referenceVariable(context, name);
  }

  static void releaseVariable(CBVar *variable) {
    return sCore._core.releaseVariable(variable);
  }

  static CBChainState suspend(CBContext *context, double seconds) {
    return sCore._core.suspend(context, seconds);
  }

  static void cloneVar(CBVar &dst, const CBVar &src) {
    sCore._core.cloneVar(&dst, &src);
  }

  static void destroyVar(CBVar &var) { sCore._core.destroyVar(&var); }

#define CB_ARRAY_INTERFACE(_arr_, _val_, _short_)                              \
  static void _short_##Free(_arr_ &seq) { sCore._core._short_##Free(&seq); };  \
                                                                               \
  static void _short_##Resize(_arr_ &seq, uint64_t size) {                     \
    sCore._core._short_##Resize(&seq, size);                                   \
  };                                                                           \
                                                                               \
  static void _short_##Push(_arr_ &seq, const _val_ &value) {                  \
    sCore._core._short_##Push(&seq, &value);                                   \
  };                                                                           \
                                                                               \
  static void _short_##Insert(_arr_ &seq, uint64_t index,                      \
                              const _val_ &value) {                            \
    sCore._core._short_##Insert(&seq, index, &value);                          \
  };                                                                           \
                                                                               \
  static _val_ _short_##Pop(_arr_ &seq) {                                      \
    return sCore._core._short_##Pop(&seq);                                     \
  };                                                                           \
                                                                               \
  static void _short_##FastDelete(_arr_ &seq, uint64_t index) {                \
    sCore._core._short_##FastDelete(&seq, index);                              \
  };                                                                           \
                                                                               \
  static void _short_##SlowDelete(_arr_ &seq, uint64_t index) {                \
    sCore._core._short_##SlowDelete(&seq, index);                              \
  }

  CB_ARRAY_INTERFACE(CBSeq, CBVar, seq);
  CB_ARRAY_INTERFACE(CBTypesInfo, CBTypeInfo, types);
  CB_ARRAY_INTERFACE(CBParametersInfo, CBParameterInfo, params);
  CB_ARRAY_INTERFACE(CBlocks, CBlockPtr, blocks);
  CB_ARRAY_INTERFACE(CBExposedTypesInfo, CBExposedTypeInfo, expTypes);
  CB_ARRAY_INTERFACE(CBStrings, CBString, strings);

  static CBComposeResult composeChain(CBChainRef chain,
                                      CBValidationCallback callback,
                                      void *userData, CBInstanceData data) {
    return sCore._core.composeChain(chain, callback, userData, data);
  }

  static CBRunChainOutput runChain(CBChainRef chain, CBContext *context,
                                   CBVar input) {
    return sCore._core.runChain(chain, context, input);
  }

  static CBComposeResult composeBlocks(CBlocks blocks,
                                       CBValidationCallback callback,
                                       void *userData, CBInstanceData data) {
    return sCore._core.composeBlocks(blocks, callback, userData, data);
  }

  static CBChainState runBlocks(CBlocks blocks, CBContext *context, CBVar input,
                                CBVar *output,
                                const bool handleReturn = false) {
    return sCore._core.runBlocks(blocks, context, input, output, handleReturn);
  }

  static void log(const char *msg) { sCore._core.log(msg); }

  static const char *rootPath() { return sCore._core.getRootPath(); }

private:
  static inline CoreLoader sCore{};
};

using ParamVar = TParamVar<Core>;
using BlocksVar = TBlocksVar<Core>;
using OwnedVar = TOwnedVar<Core>;

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

template <typename S, typename T, void (*arrayResize)(S &, uint64_t),
          void (*arrayFree)(S &), void (*arrayPush)(S &, const T &),
          typename Allocator = std::allocator<T>>
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
      _seq.elements[i] = *first++;
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
  // those (T&) casts are a bit unsafe
  // but needed when overriding CBVar with utility classes
  T &operator[](int index) { return (T &)_seq.elements[index]; }
  const T &operator[](int index) const { return (T &)_seq.elements[index]; }
  T &front() { return (T &)_seq.elements[0]; }
  const T &front() const { return (T &)_seq.elements[0]; }
  T &back() { return (T &)_seq.elements[size() - 1]; }
  const T &back() const { return (T &)_seq.elements[size() - 1]; }
  T *data() { return (T *)_seq; }
  size_t size() const { return _seq.len; }
  bool empty() const { return _seq.elements == nullptr || size() == 0; }
  void resize(size_t nsize) { arrayResize(_seq, nsize); }
  void push_back(const T &value) { arrayPush(_seq, value); }
  void clear() { arrayResize(_seq, 0); }
  operator seq_type() const { return _seq; }
};

using IterableSeq = IterableArray<CBSeq, CBVar, &Core::seqResize,
                                  &Core::seqFree, &Core::seqPush>;
using IterableExposedInfo =
    IterableArray<CBExposedTypesInfo, CBExposedTypeInfo, &Core::expTypesResize,
                  &Core::expTypesFree, &Core::expTypesPush>;

template <typename E> class EnumInfo : public TEnumInfo<Core, E> {
public:
  EnumInfo(const char *name, int32_t vendorId, int32_t enumId)
      : TEnumInfo<Core, E>(name, vendorId, enumId) {}
};

inline void registerBlock(const char *fullName, CBBlockConstructor constructor,
                          std::string_view _) {
  Core::registerBlock(fullName, constructor);
}
}; // namespace chainblocks

#endif
