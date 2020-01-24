/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#ifndef CB_CORE_HPP
#define CB_CORE_HPP

#include "chainblocks.hpp"
#include "ops.hpp"
#include "stbpp.hpp"

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
#define CB_REALLOC rprealloc
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
FlowState activateBlocks(CBlocks blocks, int nblocks, CBContext *context,
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
};

template <typename T>
void arrayGrow(T &arr, size_t addlen, size_t min_cap = 4) {
  size_t min_len = arr.len + addlen;

  // compute the minimum capacity needed
  if (min_len > min_cap)
    min_cap = min_len;

  if (min_cap <= arr.cap)
    return;

  // increase needed capacity to guarantee O(1) amortized
  if (min_cap < 2 * arr.cap)
    min_cap = 2 * arr.cap;

  arr.elements = (decltype(arr.elements))CB_REALLOC(
      arr.elements, sizeof(*arr.elements) * min_cap);
  arr.cap = min_cap;
}

template <typename T, typename V> void arrayPush(T &arr, const V &val) {
  if (!arr.elements || (arr.len + 1) > arr.cap)
    arrayGrow(arr, 1);
  arr.elements[arr.len++] = val;
}

template <typename T> void arrayResize(T &arr, uint32_t size) {
  if (arr.len < size)
    arrayGrow(arr, size - arr.len);
  arr.len = size;
}

template <typename T, typename V>
void arrayInsert(T &arr, uint32_t index, const V &val) {
  if (!arr.elements || (arr.len + 1) > arr.cap)
    arrayGrow(arr, 1);
  memmove(&arr.elements[index + 1], &arr.elements[index],
          sizeof(*arr.elements) * (arr.len - index));
  arr.len++;
  arr.elements[index] = val;
}

template <typename T> void arrayDelFast(T &arr, uint32_t index) {
  arr.elements[index] = arr.elements[arr.len - 1];
  arr.len--;
}

template <typename T, typename V> V arrayPop(T &arr) {
  arr.len--;
  return arr.elements[arr.len];
}

template <typename T> void arrayDel(T &arr, uint32_t index) {
  arr.len--;
  memmove(&arr.elements[index], &arr.elements[index + 1],
          sizeof(*arr.elements) * (arr.len - index));
}

template <typename T> void arrayFree(T &arr) {
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
    for (auto i = 0; i < s; i++) {
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
    _seq = nullptr;
    _owned = true;
    size_t size = other._seq.len;
    arrayResize(_seq, size);
    for (size_t i = 0; i < size; i++) {
      _seq[i] = other._seq[i];
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

ALWAYS_INLINE inline void destroyVar(CBVar &var);
ALWAYS_INLINE inline void cloneVar(CBVar &dst, const CBVar &src);

static void _destroyVarSlow(CBVar &var) {
  switch (var.valueType) {
  case Seq: {
    assert(var.payload.seqValue.cap >= var.capacity.value);
    assert(var.payload.seqValue.len <= var.capacity.value);
    for (size_t i = var.capacity.value; i > 0; i--) {
      destroyVar(var.payload.seqValue.elements[i - 1]);
    }
    chainblocks::arrayFree(var.payload.seqValue);
  } break;
  case Table: {
    auto len = stbds_shlen(var.payload.tableValue);
    for (auto i = len; i > 0; i--) {
      destroyVar(var.payload.tableValue[i - 1].value);
    }
    stbds_shfree(var.payload.tableValue);
  } break;
  default:
    break;
  };
}

static void _cloneVarSlow(CBVar &dst, const CBVar &src) {
  if (src == dst)
    return;

  switch (src.valueType) {
  case Seq: {
    size_t srcLen = src.payload.seqValue.len;
#if 0
    destroyVar(dst);
    dst.valueType = Seq;
    dst.payload.seqValue = nullptr;
    for (size_t i = 0; i < srcLen; i++) {
      auto &subsrc = src.payload.seqValue.elements[i];
      CBVar tmp{};
      cloneVar(tmp, subsrc);
      chainblocks::arrayPush(dst.payload.seqValue, tmp);
    }
#else
    // try our best to re-use memory
    if (dst.valueType != Seq) {
      destroyVar(dst);
      dst.valueType = Seq;
      for (size_t i = 0; i < srcLen; i++) {
        auto &subsrc = src.payload.seqValue.elements[i];
        CBVar tmp{};
        cloneVar(tmp, subsrc);
        chainblocks::arrayPush(dst.payload.seqValue, tmp);
      }
    } else {
      size_t dstLen = dst.payload.seqValue.len;
      assert(dst.capacity.value >= dstLen);
      if (srcLen <= dst.capacity.value) {
        // clone on top of current values
        chainblocks::arrayResize(dst.payload.seqValue, srcLen);
        for (size_t i = 0; i < srcLen; i++) {
          auto &subsrc = src.payload.seqValue.elements[i];
          cloneVar(dst.payload.seqValue.elements[i], subsrc);
        }
      } else {
        // re-use avail ones
        for (size_t i = 0; i < dstLen; i++) {
          auto &subsrc = src.payload.seqValue.elements[i];
          cloneVar(dst.payload.seqValue.elements[i], subsrc);
        }
        // append new values
        for (size_t i = dstLen; i < srcLen; i++) {
          auto &subsrc = src.payload.seqValue.elements[i];
          CBVar tmp{};
          cloneVar(tmp, subsrc);
          chainblocks::arrayPush(dst.payload.seqValue, subsrc);
        }
      }
    }
#endif
    // take note of 'Var' capacity
    dst.capacity.value =
        std::max(dst.capacity.value,
                 decltype(dst.capacity.value)(dst.payload.seqValue.len));
  } break;
  case String:
  case ContextVar: {
    auto srcSize = strlen(src.payload.stringValue) + 1;
    if ((dst.valueType != String && dst.valueType != ContextVar) ||
        dst.capacity.value < srcSize) {
      destroyVar(dst);
      dst.payload.stringValue = new char[srcSize];
      dst.capacity.value = srcSize;
    }

    dst.valueType = src.valueType;
    memcpy((void *)dst.payload.stringValue, (void *)src.payload.stringValue,
           srcSize - 1);
    ((char *)dst.payload.stringValue)[srcSize - 1] = 0;
  } break;
  case Image: {
    size_t srcImgSize = src.payload.imageValue.height *
                        src.payload.imageValue.width *
                        src.payload.imageValue.channels;
    if (dst.valueType != Image || srcImgSize > dst.capacity.value) {
      destroyVar(dst);
      dst.valueType = Image;
      dst.payload.imageValue.data = new uint8_t[srcImgSize];
      dst.capacity.value = srcImgSize;
    }

    dst.payload.imageValue.flags = dst.payload.imageValue.flags;
    dst.payload.imageValue.height = src.payload.imageValue.height;
    dst.payload.imageValue.width = src.payload.imageValue.width;
    dst.payload.imageValue.channels = src.payload.imageValue.channels;
    memcpy(dst.payload.imageValue.data, src.payload.imageValue.data,
           srcImgSize);
  } break;
  case Table: {
    // Slowest case, it's a full copy using arena tho
    destroyVar(dst);
    dst.valueType = Table;
    dst.payload.tableValue = nullptr;
    stbds_sh_new_arena(dst.payload.tableValue);
    auto srcLen = stbds_shlen(src.payload.tableValue);
    for (auto i = 0; i < srcLen; i++) {
      CBVar clone{};
      cloneVar(clone, src.payload.tableValue[i].value);
      stbds_shput(dst.payload.tableValue, src.payload.tableValue[i].key, clone);
    }
  } break;
  case Bytes: {
    if (dst.valueType != Bytes || dst.capacity.value < src.payload.bytesSize) {
      destroyVar(dst);
      dst.valueType = Bytes;
      dst.payload.bytesValue = new uint8_t[src.payload.bytesSize];
      dst.capacity.value = src.payload.bytesSize;
    }

    dst.payload.bytesSize = src.payload.bytesSize;
    memcpy((void *)dst.payload.bytesValue, (void *)src.payload.bytesValue,
           src.payload.bytesSize);
  } break;
  default:
    break;
  };
}

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

static Var True = Var(true);
static Var False = Var(false);
static Var StopChain = Var::Stop();
static Var RestartChain = Var::Restart();
static Var ReturnPrevious = Var::Return();
static Var RebaseChain = Var::Rebase();
static Var Empty = Var();

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
    case CBType::Object:
    case CBType::Chain:
    case CBType::Block:
      break;
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

  static void arrayFree(void *arr) { stbds_arrfree(arr); }

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
    chainblocks::activateBlocks(blocks, (int)stbds_arrlen(blocks), context,
                                input, output);
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

struct TypeInfo : public CBTypeInfo {
  TypeInfo() { basicType = None; }

  TypeInfo(CBType type) {
    basicType = type;
    tableKeys = nullptr;
    tableTypes = nullptr;
  }

  TypeInfo(CBTypeInfo other) {
    basicType = other.basicType;
    tableKeys = nullptr;
    tableTypes = nullptr;

    switch (basicType) {
    case CBType::Object: {
      objectVendorId = other.objectVendorId;
      objectTypeId = other.objectTypeId;
    } break;
    case CBType::Enum: {
      enumVendorId = other.enumVendorId;
      enumTypeId = other.enumTypeId;
    } break;
    case Seq: {
      if (other.seqType) {
        seqType = other.seqType;
      }
    } break;
    case Table: {
      if (other.tableTypes) {
        for (auto i = 0; i < stbds_arrlen(other.tableTypes); i++) {
          stbds_arrpush(tableTypes, other.tableTypes[i]);
        }
        for (auto i = 0; i < stbds_arrlen(other.tableKeys); i++) {
          stbds_arrpush(tableKeys, other.tableKeys[i]);
        }
      }
    } break;
    default:
      break;
    }
  }

  static TypeInfo Object(int32_t objectVendorId, int32_t objectTypeId) {
    TypeInfo result;
    result.basicType = CBType::Object;
    result.objectVendorId = objectVendorId;
    result.objectTypeId = objectTypeId;
    return result;
  }

  static TypeInfo Enum(int32_t enumVendorId, int32_t enumTypeId) {
    TypeInfo result;
    result.basicType = CBType::Enum;
    result.enumVendorId = enumVendorId;
    result.enumTypeId = enumTypeId;
    return result;
  }

  static TypeInfo Sequence(TypeInfo &contentType) {
    TypeInfo result;
    result.basicType = Seq;
    result.seqType = &contentType;
    return result;
  }

  static TypeInfo TableRecord(CBTypeInfo contentType, const char *keyName) {
    TypeInfo result;
    result.basicType = Table;
    result.tableTypes = nullptr;
    result.tableKeys = nullptr;
    stbds_arrpush(result.tableTypes, contentType);
    stbds_arrpush(result.tableKeys, keyName);
    return result;
  }

  static TypeInfo SingleTypeTable(CBTypeInfo contentType) {
    TypeInfo result;
    result.basicType = Table;
    result.tableTypes = nullptr;
    result.tableKeys = nullptr;
    stbds_arrpush(result.tableTypes, contentType);
    return result;
  }

  TypeInfo(const TypeInfo &other) : CBTypeInfo(other) {
    basicType = other.basicType;
    tableKeys = nullptr;
    tableTypes = nullptr;

    switch (basicType) {
    case CBType::Object: {
      objectVendorId = other.objectVendorId;
      objectTypeId = other.objectTypeId;
    } break;
    case CBType::Enum: {
      enumVendorId = other.enumVendorId;
      enumTypeId = other.enumTypeId;
    } break;
    case Seq: {
      if (other.seqType) {
        seqType = other.seqType;
      }
    } break;
    case Table: {
      tableKeys = nullptr;
      tableTypes = nullptr;
      if (other.tableTypes) {
        for (auto i = 0; i < stbds_arrlen(other.tableTypes); i++) {
          stbds_arrpush(tableTypes, other.tableTypes[i]);
        }
        for (auto i = 0; i < stbds_arrlen(other.tableKeys); i++) {
          stbds_arrpush(tableKeys, other.tableKeys[i]);
        }
      }
    } break;
    default:
      break;
    }
  }

  TypeInfo &operator=(const TypeInfo &other) {
    switch (basicType) {
    case Seq: {
      if (seqType)
        delete seqType;
    } break;
    case Table: {
      if (other.tableTypes) {
        stbds_arrfree(tableKeys);
        stbds_arrfree(tableTypes);
      }
    } break;
    default:
      break;
    }

    basicType = other.basicType;
    tableKeys = nullptr;
    tableTypes = nullptr;

    switch (basicType) {
    case CBType::Object: {
      objectVendorId = other.objectVendorId;
      objectTypeId = other.objectTypeId;
    } break;
    case CBType::Enum: {
      enumVendorId = other.enumVendorId;
      enumTypeId = other.enumTypeId;
    } break;
    case Seq: {
      if (other.seqType) {
        seqType = other.seqType;
      }
    } break;
    case Table: {
      if (other.tableTypes) {
        for (auto i = 0; i < stbds_arrlen(other.tableTypes); i++) {
          stbds_arrpush(tableTypes, other.tableTypes[i]);
        }
        for (auto i = 0; i < stbds_arrlen(other.tableKeys); i++) {
          stbds_arrpush(tableKeys, other.tableKeys[i]);
        }
      }
    } break;
    default:
      break;
    }

    return *this;
  }

  ~TypeInfo() {
    if (basicType == Table) {
      if (tableTypes) {
        stbds_arrfree(tableTypes);
        stbds_arrfree(tableKeys);
      }
    }
  }
};

struct TypesInfo {
  TypesInfo() { _innerInfo = nullptr; }

  TypesInfo(const TypesInfo &other) {
    _innerTypes = other._innerTypes;
    _innerInfo = nullptr;
    for (auto &type : _innerTypes) {
      stbds_arrpush(_innerInfo, type);
    }
  }

  TypesInfo &operator=(const TypesInfo &other) {
    _innerTypes = other._innerTypes;
    stbds_arrsetlen(_innerInfo, 0);
    for (auto &type : _innerTypes) {
      stbds_arrpush(_innerInfo, type);
    }
    return *this;
  }

  explicit TypesInfo(const TypeInfo &singleType, bool canBeSeq = false,
                     bool canBeNone = false) {
    _innerInfo = nullptr;
    // NOTICE , we NEED to reserve in other to preserve those memory addresses!
    // See that we use .back and push "ref" to them
    _innerTypes.reserve(3);
    _innerTypes.push_back(singleType);
    stbds_arrpush(_innerInfo, _innerTypes.back());
    if (canBeSeq) {
      _innerTypes.push_back(TypeInfo::Sequence(_innerTypes.back()));
      stbds_arrpush(_innerInfo, _innerTypes.back());
    }
    if (canBeNone) {
      _innerTypes.push_back(TypeInfo(CBType::None));
      stbds_arrpush(_innerInfo, _innerTypes.back());
    }
  }

  template <typename... Args>
  static TypesInfo FromMany(bool canBeSeq, Args... types) {
    TypesInfo result;
    result._innerInfo = nullptr;
    std::vector<TypeInfo> vec = {types...};

    // Preallocate in order to be able to have always valid addresses
    auto size = vec.size();
    if (canBeSeq)
      size *= 2;
    // NOTICE , we NEED to reserve in other to preserve those memory addresses!
    // See that we use .back and push "ref" to them
    result._innerTypes.reserve(size);

    for (auto &type : vec) {
      result._innerTypes.push_back(type);
      auto &nonSeq = result._innerTypes.back();
      stbds_arrpush(result._innerInfo, nonSeq);
      if (canBeSeq) {
        result._innerTypes.push_back(TypeInfo::Sequence(nonSeq));
        auto &seqType = result._innerTypes.back();
        stbds_arrpush(result._innerInfo, seqType);
      }
    }
    return result;
  }

  ~TypesInfo() {
    if (_innerInfo) {
      stbds_arrfree(_innerInfo);
    }
  }

  explicit operator CBTypesInfo() const { return _innerInfo; }

  explicit operator CBTypeInfo() const { return _innerInfo[0]; }

  CBTypesInfo _innerInfo{};
  std::vector<TypeInfo> _innerTypes;
};

struct ParamsInfo {
  ParamsInfo(const ParamsInfo &other) {
    stbds_arrsetlen(_innerInfo, 0);
    for (auto i = 0; i < stbds_arrlen(other._innerInfo); i++) {
      stbds_arrpush(_innerInfo, other._innerInfo[i]);
    }
  }

  ParamsInfo &operator=(const ParamsInfo &other) {
    _innerInfo = nullptr;
    for (auto i = 0; i < stbds_arrlen(other._innerInfo); i++) {
      stbds_arrpush(_innerInfo, other._innerInfo[i]);
    }
    return *this;
  }

  template <typename... Types>
  explicit ParamsInfo(CBParameterInfo first, Types... types) {
    std::vector<CBParameterInfo> vec = {first, types...};
    _innerInfo = nullptr;
    for (auto pi : vec) {
      stbds_arrpush(_innerInfo, pi);
    }
  }

  template <typename... Types>
  explicit ParamsInfo(const ParamsInfo &other, CBParameterInfo first,
                      Types... types) {
    _innerInfo = nullptr;

    for (auto i = 0; i < stbds_arrlen(other._innerInfo); i++) {
      stbds_arrpush(_innerInfo, other._innerInfo[i]);
    }

    std::vector<CBParameterInfo> vec = {first, types...};
    for (auto pi : vec) {
      stbds_arrpush(_innerInfo, pi);
    }
  }

  static CBParameterInfo Param(const char *name, const char *help,
                               CBTypesInfo types) {
    CBParameterInfo res = {name, help, types};
    return res;
  }

  ~ParamsInfo() {
    if (_innerInfo)
      stbds_arrfree(_innerInfo);
  }

  explicit operator CBParametersInfo() const { return _innerInfo; }

  CBParametersInfo _innerInfo{};
};

struct ExposedInfo {
  ExposedInfo() { _innerInfo = nullptr; }

  ExposedInfo(const ExposedInfo &other) {
    _innerInfo = nullptr;
    for (auto i = 0; i < stbds_arrlen(other._innerInfo); i++) {
      stbds_arrpush(_innerInfo, other._innerInfo[i]);
    }
  }

  ExposedInfo &operator=(const ExposedInfo &other) {
    stbds_arrsetlen(_innerInfo, 0);
    for (auto i = 0; i < stbds_arrlen(other._innerInfo); i++) {
      stbds_arrpush(_innerInfo, other._innerInfo[i]);
    }
    return *this;
  }

  template <typename... Types>
  explicit ExposedInfo(const ExposedInfo &other, Types... types) {
    _innerInfo = nullptr;

    for (auto i = 0; i < stbds_arrlen(other._innerInfo); i++) {
      stbds_arrpush(_innerInfo, other._innerInfo[i]);
    }

    std::vector<CBExposedTypeInfo> vec = {types...};
    for (auto pi : vec) {
      stbds_arrpush(_innerInfo, pi);
    }
  }

  template <typename... Types>
  explicit ExposedInfo(const CBExposedTypesInfo other, Types... types) {
    _innerInfo = nullptr;

    for (auto i = 0; i < stbds_arrlen(other); i++) {
      stbds_arrpush(_innerInfo, other[i]);
    }

    std::vector<CBExposedTypeInfo> vec = {types...};
    for (auto pi : vec) {
      stbds_arrpush(_innerInfo, pi);
    }
  }

  template <typename... Types>
  explicit ExposedInfo(const CBExposedTypeInfo first, Types... types) {
    std::vector<CBExposedTypeInfo> vec = {first, types...};
    _innerInfo = nullptr;
    for (auto pi : vec) {
      stbds_arrpush(_innerInfo, pi);
    }
  }

  static CBExposedTypeInfo Variable(const char *name, const char *help,
                                    CBTypeInfo type, bool isMutable = false) {
    CBExposedTypeInfo res = {name, help, type, isMutable};
    return res;
  }

  ~ExposedInfo() {
    if (_innerInfo)
      stbds_arrfree(_innerInfo);
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
