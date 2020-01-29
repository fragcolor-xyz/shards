/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#ifndef CB_CORE_HPP
#define CB_CORE_HPP

#include "ops_internal.hpp"
#include <chainblocks.hpp>

// Included 3rdparty
#include "easylogging++.h"
#include "nameof.hpp"

#include <algorithm>
#include <cassert>
#include <list>
#include <parallel_hashmap/phmap.h>
#include <set>
#include <type_traits>

#include "blockwrapper.hpp"

#ifdef USE_RPMALLOC
#include "rpmalloc/rpmalloc.h"
inline void *rp_init_realloc(void *ptr, size_t size) {
  rpmalloc_initialize();
  return rprealloc(ptr, size);
}
#define CB_REALLOC rp_init_realloc
#define CB_FREE rpfree
#else
#define CB_REALLOC std::realloc
#define CB_FREE std::free
#endif

#define TRACE_LINE DLOG(TRACE) << "#trace#"

CBValidationResult validateConnections(const CBlocks chain,
                                       CBValidationCallback callback,
                                       void *userData, CBInstanceData data);
namespace chainblocks {
constexpr uint32_t FragCC = 'frag'; // 1718772071

enum FlowState { Stopping, Continuing, Returning };

FlowState activateBlocks(CBSeq blocks, CBContext *context,
                         const CBVar &chainInput, CBVar &output);
FlowState activateBlocks(CBlocks blocks, CBContext *context,
                         const CBVar &chainInput, CBVar &output);
CBVar *findVariable(CBContext *ctx, const char *name);
CBVar suspend(CBContext *context, double seconds);
void registerEnumType(int32_t vendorId, int32_t enumId, CBEnumInfo info);

#define cbpause(_time_)                                                        \
  {                                                                            \
    auto chainState = chainblocks::suspend(context, _time_);                   \
    if (chainState.payload.chainState != Continue) {                           \
      return chainState;                                                       \
    }                                                                          \
  }

struct RuntimeObserver;

struct Globals {
  static inline phmap::flat_hash_map<std::string, CBBlockConstructor>
      BlocksRegister;
  static inline phmap::flat_hash_map<int64_t, CBObjectInfo> ObjectTypesRegister;
  static inline phmap::flat_hash_map<int64_t, CBEnumInfo> EnumTypesRegister;

  // map = ordered! we need that for those
  static inline std::map<std::string, CBCallback> RunLoopHooks;
  static inline std::map<std::string, CBCallback> ExitHooks;

  static inline phmap::flat_hash_map<std::string, CBChain *> GlobalChains;

  static inline std::list<std::weak_ptr<RuntimeObserver>> Observers;

  static inline std::string RootPath;

  static inline Var True = Var(true);
  static inline Var False = Var(false);
  static inline Var StopChain = Var::Stop();
  static inline Var RestartChain = Var::Restart();
  static inline Var ReturnPrevious = Var::Return();
  static inline Var RebaseChain = Var::Rebase();
  static inline Var Empty = Var();
};

#define True ::chainblocks::Globals::True
#define False ::chainblocks::Globals::False
#define StopChain ::chainblocks::Globals::StopChain
#define RestartChain ::chainblocks::Globals::RestartChain
#define ReturnPrevious ::chainblocks::Globals::ReturnPrevious
#define RebaseChain ::chainblocks::Globals::RebaseChain
#define Empty ::chainblocks::Globals::Empty

template <typename T>
NO_INLINE void arrayGrow(T &arr, size_t addlen, size_t min_cap = 4);

template <typename T, typename V>
ALWAYS_INLINE inline void arrayPush(T &arr, const V &val) {
  if ((arr.len + 1) > arr.cap)
    arrayGrow(arr, 1);
  arr.elements[arr.len++] = val;
}

template <typename T>
ALWAYS_INLINE inline void arrayResize(T &arr, uint32_t size) {
  if (arr.len < size)
    arrayGrow(arr, size - arr.len);
  arr.len = size;
}

template <typename T, typename V>
ALWAYS_INLINE inline void arrayInsert(T &arr, uint32_t index, const V &val) {
  if ((arr.len + 1) > arr.cap)
    arrayGrow(arr, 1);
  memmove(&arr.elements[index + 1], &arr.elements[index],
          sizeof(*arr.elements) * (arr.len - index));
  arr.len++;
  arr.elements[index] = val;
}

template <typename T>
ALWAYS_INLINE inline void arrayDelFast(T &arr, uint32_t index) {
  arr.elements[index] = arr.elements[arr.len - 1];
  arr.len--;
}

template <typename T, typename V> ALWAYS_INLINE inline V arrayPop(T &arr) {
  arr.len--;
  return arr.elements[arr.len];
}

template <typename T>
void ALWAYS_INLINE inline arrayDel(T &arr, uint32_t index) {
  arr.len--;
  memmove(&arr.elements[index], &arr.elements[index + 1],
          sizeof(*arr.elements) * (arr.len - index));
}

template <typename T> ALWAYS_INLINE inline void arrayFree(T &arr) {
  if (arr.elements)
    CB_FREE(arr.elements);
  arr = {};
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

  std::ptrdiff_t operator-(PtrIterator const &other) const {
    return ptr - other.ptr;
  }

  bool operator!=(const PtrIterator &other) const { return ptr != other.ptr; }
  const T &operator*() const { return *ptr; }

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
  IterableArray(seq_type seq) : _seq(seq), _owned(false) {}

  // implicit converter
  IterableArray(CBVar v) : _seq(v.payload.seqValue), _owned(false) {
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

  IterableArray(const IterableArray &other) : _seq(nullptr), _owned(true) {
    size_t size = other._seq.len;
    arrayResize(_seq, size);
    for (size_t i = 0; i < size; i++) {
      _seq[i] = other._seq[i];
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

NO_INLINE void _destroyVarSlow(CBVar &var);
NO_INLINE void _cloneVarSlow(CBVar &dst, const CBVar &src);

ALWAYS_INLINE inline void destroyVar(CBVar &var) {
  switch (var.valueType) {
  case Table:
  case Seq:
    _destroyVarSlow(var);
    break;
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
  default:
    break;
  };

  var = {};
}

ALWAYS_INLINE inline void cloneVar(CBVar &dst, const CBVar &src) {
  if (src.valueType < EndOfBlittableTypes &&
      dst.valueType < EndOfBlittableTypes) {
    dst.payload = src.payload;
    dst.valueType = src.valueType;
  } else if (src.valueType < EndOfBlittableTypes) {
    destroyVar(dst);
    dst.payload = src.payload;
    dst.valueType = src.valueType;
  } else {
    _cloneVarSlow(dst, src);
  }
}

struct Serialization {
  static inline void varFree(CBVar &output) {
    switch (output.valueType) {
    case CBType::None:
    case CBType::EndOfBlittableTypes:
    case CBType::Any:
    case CBType::Enum:
    case CBType::Bool:
    case CBType::Int:
    case CBType::Int2:
    case CBType::Int3:
    case CBType::Int4:
    case CBType::Int8:
    case CBType::Int16:
    case CBType::Float:
    case CBType::Float2:
    case CBType::Float3:
    case CBType::Float4:
    case CBType::Color:
      break;
    case CBType::Bytes:
      delete[] output.payload.bytesValue;
      break;
    case CBType::String:
    case CBType::ContextVar: {
      delete[] output.payload.stringValue;
      break;
    }
    case CBType::Seq: {
      for (uint32_t i = 0; i < output.payload.seqValue.len; i++) {
        varFree(output.payload.seqValue.elements[i]);
      }
      chainblocks::arrayFree(output.payload.seqValue);
      break;
    }
    case CBType::Table: {
      for (auto i = 0; i < stbds_shlen(output.payload.tableValue); i++) {
        auto &v = output.payload.tableValue[i];
        varFree(v.value);
        delete[] v.key;
      }
      stbds_shfree(output.payload.tableValue);
      output.payload.tableValue = nullptr;
      break;
    }
    case CBType::Image: {
      delete[] output.payload.imageValue.data;
      break;
    }
    case CBType::Vector: {
      delete[] output.payload.vectorValue;
      break;
    }
    case CBType::Object:
    case CBType::Chain:
    case CBType::Block:
      throw CBException("Serialization not supported for the given type: " +
                        type2Name(output.valueType));
    }

    output = {};
  }

  template <class BinaryReader>
  static inline void deserialize(BinaryReader read, CBVar &output) {
    // we try to recycle memory so pass a empty None as first timer!
    CBType nextType;
    read((uint8_t *)&nextType, sizeof(output.valueType));

    // stop trying to recycle, types differ
    auto recycle = true;
    if (output.valueType != nextType) {
      varFree(output);
      recycle = false;
    }

    output.valueType = nextType;

    switch (output.valueType) {
    case CBType::None:
    case CBType::EndOfBlittableTypes:
    case CBType::Any:
    case CBType::Enum:
    case CBType::Bool:
    case CBType::Int:
    case CBType::Int2:
    case CBType::Int3:
    case CBType::Int4:
    case CBType::Int8:
    case CBType::Int16:
    case CBType::Float:
    case CBType::Float2:
    case CBType::Float3:
    case CBType::Float4:
    case CBType::Color:
      read((uint8_t *)&output.payload, sizeof(output.payload));
      break;
    case CBType::Bytes: {
      auto availBytes = recycle ? output.capacity.value : 0;
      read((uint8_t *)&output.payload.bytesSize,
           sizeof(output.payload.bytesSize));

      if (availBytes > 0 && availBytes < output.payload.bytesSize) {
        // not enough space, ideally realloc, but for now just delete
        delete[] output.payload.bytesValue;
        // and re alloc
        output.payload.bytesValue = new uint8_t[output.payload.bytesSize];
      } else if (availBytes == 0) {
        // just alloc
        output.payload.bytesValue = new uint8_t[output.payload.bytesSize];
      } // else got enough space to recycle!

      // record actualSize for further recycling usage
      output.capacity.value = std::max(availBytes, output.payload.bytesSize);

      read((uint8_t *)output.payload.bytesValue, output.payload.bytesSize);
      break;
    }
    case CBType::String:
    case CBType::ContextVar: {
      auto availChars = recycle ? output.capacity.value : 0;
      uint64_t len;
      read((uint8_t *)&len, sizeof(uint64_t));

      if (availChars > 0 && availChars < len) {
        // we need more chars then what we have, realloc
        delete[] output.payload.stringValue;
        output.payload.stringValue = new char[len + 1];
      } else if (availChars == 0) {
        // just alloc
        output.payload.stringValue = new char[len + 1];
      } // else recycling

      // record actualSize
      output.capacity.value = std::max(availChars, len);

      read((uint8_t *)output.payload.stringValue, len);
      const_cast<char *>(output.payload.stringValue)[len] = 0;
      break;
    }
    case CBType::Seq: {
      uint64_t len;
      read((uint8_t *)&len, sizeof(uint64_t));

      uint64_t currentUsed = recycle ? output.payload.seqValue.len : 0;
      if (currentUsed > len) {
        // in this case we need to destroy the excess items
        // before resizing
        for (uint64_t i = len; i < currentUsed; i++) {
          varFree(output.payload.seqValue.elements[i]);
        }
      }

      chainblocks::arrayResize(output.payload.seqValue, len);
      for (uint64_t i = 0; i < len; i++) {
        deserialize(read, output.payload.seqValue.elements[i]);
      }
      break;
    }
    case CBType::Table: {
      if (recycle) // tables are slow for now...
        varFree(output);

      uint64_t len;
      read((uint8_t *)&len, sizeof(uint64_t));
      for (uint64_t i = 0; i < len; i++) {
        CBNamedVar v;
        uint64_t klen;
        read((uint8_t *)&klen, sizeof(uint64_t));
        v.key = new char[klen + 1];
        read((uint8_t *)v.key, len);
        const_cast<char *>(v.key)[klen] = 0;
        deserialize(read, v.value);
        stbds_shputs(output.payload.tableValue, v);
      }
      break;
    }
    case CBType::Image: {
      read((uint8_t *)&output.payload.imageValue.channels,
           sizeof(output.payload.imageValue.channels));
      read((uint8_t *)&output.payload.imageValue.flags,
           sizeof(output.payload.imageValue.flags));
      read((uint8_t *)&output.payload.imageValue.width,
           sizeof(output.payload.imageValue.width));
      read((uint8_t *)&output.payload.imageValue.height,
           sizeof(output.payload.imageValue.height));

      size_t size = output.payload.imageValue.channels *
                    output.payload.imageValue.height *
                    output.payload.imageValue.width;

      size_t currentSize = recycle ? output.capacity.value : 0;
      if (currentSize > 0 && currentSize < size) {
        // delete first & alloc
        delete[] output.payload.imageValue.data;
        output.payload.imageValue.data = new uint8_t[size];
      } else if (currentSize == 0) {
        // just alloc
        output.payload.imageValue.data = new uint8_t[size];
      }

      // record actualSize
      output.capacity.value = std::max(currentSize, (size_t)size);

      read((uint8_t *)output.payload.imageValue.data, size);
      break;
    }
    case CBType::Object:
    case CBType::Chain:
    case CBType::Block:
      throw CBException("WriteFile: Type cannot be serialized (yet?). " +
                        type2Name(output.valueType));
    }
  }

  template <class BinaryWriter>
  static inline size_t serialize(const CBVar &input, BinaryWriter write) {
    size_t total = 0;
    write((const uint8_t *)&input.valueType, sizeof(input.valueType));
    total += sizeof(input.valueType);
    switch (input.valueType) {
    case CBType::None:
    case CBType::EndOfBlittableTypes:
    case CBType::Any:
    case CBType::Enum:
    case CBType::Bool:
    case CBType::Int:
    case CBType::Int2:
    case CBType::Int3:
    case CBType::Int4:
    case CBType::Int8:
    case CBType::Int16:
    case CBType::Float:
    case CBType::Float2:
    case CBType::Float3:
    case CBType::Float4:
    case CBType::Color:
      write((const uint8_t *)&input.payload, sizeof(input.payload));
      total += sizeof(input.payload);
      break;
    case CBType::Bytes:
      write((const uint8_t *)&input.payload.bytesSize,
            sizeof(input.payload.bytesSize));
      total += sizeof(input.payload.bytesSize);
      write((const uint8_t *)input.payload.bytesValue, input.payload.bytesSize);
      total += input.payload.bytesSize;
      break;
    case CBType::String:
    case CBType::ContextVar: {
      uint64_t len = strlen(input.payload.stringValue);
      write((const uint8_t *)&len, sizeof(uint64_t));
      total += sizeof(uint64_t);
      write((const uint8_t *)input.payload.stringValue, len);
      total += len;
      break;
    }
    case CBType::Seq: {
      uint64_t len = input.payload.seqValue.len;
      write((const uint8_t *)&len, sizeof(uint64_t));
      total += sizeof(uint64_t);
      for (uint64_t i = 0; i < len; i++) {
        total += serialize(input.payload.seqValue.elements[i], write);
      }
      break;
    }
    case CBType::Table: {
      uint64_t len = stbds_shlen(input.payload.tableValue);
      write((const uint8_t *)&len, sizeof(uint64_t));
      total += sizeof(uint64_t);
      for (uint64_t i = 0; i < len; i++) {
        auto &v = input.payload.tableValue[i];
        uint64_t klen = strlen(v.key);
        write((const uint8_t *)&klen, sizeof(uint64_t));
        total += sizeof(uint64_t);
        write((const uint8_t *)v.key, len);
        total += len;
        total += serialize(v.value, write);
      }
      break;
    }
    case CBType::Image: {
      write((const uint8_t *)&input.payload.imageValue.channels,
            sizeof(input.payload.imageValue.channels));
      total += sizeof(input.payload.imageValue.channels);
      write((const uint8_t *)&input.payload.imageValue.flags,
            sizeof(input.payload.imageValue.flags));
      total += sizeof(input.payload.imageValue.flags);
      write((const uint8_t *)&input.payload.imageValue.width,
            sizeof(input.payload.imageValue.width));
      total += sizeof(input.payload.imageValue.width);
      write((const uint8_t *)&input.payload.imageValue.height,
            sizeof(input.payload.imageValue.height));
      total += sizeof(input.payload.imageValue.height);
      auto size = input.payload.imageValue.channels *
                  input.payload.imageValue.height *
                  input.payload.imageValue.width;
      write((const uint8_t *)input.payload.imageValue.data, size);
      total += size;
      break;
    }
    case CBType::Object:
    case CBType::Chain:
    case CBType::Block:
      throw CBException("Type cannot be serialized. " +
                        type2Name(input.valueType));
    }
    return total;
  }
};

struct InternalCore {
  // need to emulate dllblock Core a bit
  static CBVar *findVariable(CBContext *context, const char *name) {
    return chainblocks::findVariable(context, name);
  }

  static void cloneVar(CBVar &dst, const CBVar &src) {
    chainblocks::cloneVar(dst, src);
  }

  static void destroyVar(CBVar &var) { chainblocks::destroyVar(var); }

  template <typename T> static void arrayFree(T &arr) {
    chainblocks::arrayFree<T>(arr);
  }

  static void expTypesFree(CBExposedTypesInfo &arr) { arrayFree(arr); }

  static void throwException(const char *msg) {
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
    return validateConnections(blocks, callback, userData, data);
  }

  static CBVar runBlocks(CBlocks blocks, CBContext *context, CBVar input) {
    CBVar output{};
    chainblocks::activateBlocks(blocks, context, input, output);
    return output;
  }
};

typedef TParamVar<InternalCore> ParamVar;

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
    CBExposedTypeInfo res = {name, help, type, isMutable, isTableField};
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
}; // namespace chainblocks

#endif
