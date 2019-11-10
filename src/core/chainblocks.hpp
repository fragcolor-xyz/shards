/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#pragma once

#include "chainblocks.h"
#include "ops.hpp"
#include "stbpp.hpp"

// Included 3rdparty
#include "easylogging++.h"
#include "parallel_hashmap/phmap.h"

#include <algorithm>
#include <cassert>
#include <type_traits>

#define TRACE_LINE DLOG(TRACE) << "#trace#"

namespace chainblocks {
constexpr uint32_t FragCC = 'frag'; // 1718772071

enum FlowState { Stopping, Continuing, Returning };

FlowState activateBlocks(CBSeq blocks, CBContext *context,
                         const CBVar &chainInput, CBVar &output);
FlowState activateBlocks(CBlocks blocks, int nblocks, CBContext *context,
                         const CBVar &chainInput, CBVar &output);
CBVar *contextVariable(CBContext *ctx, const char *name, bool global = false);
CBVar suspend(CBContext *context, double seconds);

#define cbpause(_time_)                                                        \
  {                                                                            \
    auto chainState = chainblocks::suspend(context, _time_);                   \
    if (chainState.payload.chainState != Continue) {                           \
      return chainState;                                                       \
    }                                                                          \
  }

class CBException : public std::exception {
public:
  explicit CBException(const char *errmsg) : errorMessage(errmsg) {}
  explicit CBException(std::string errmsg) : errorMessage(errmsg) {}

  [[nodiscard]] const char *what() const noexcept override {
    return errorMessage.c_str();
  }

private:
  std::string errorMessage;
};

ALWAYS_INLINE inline void destroyVar(CBVar &var);
ALWAYS_INLINE inline void cloneVar(CBVar &dst, const CBVar &src);

static void _destroyVarSlow(CBVar &var) {
  switch (var.valueType) {
  case Seq: {
    int len = stbds_arrlen(var.payload.seqValue);
    for (int i = 0; i < len; i++) {
      destroyVar(var.payload.seqValue[i]);
    }
    stbds_arrfree(var.payload.seqValue);
  } break;
  case Table: {
    auto len = stbds_shlen(var.payload.tableValue);
    for (auto i = 0; i < len; i++) {
      destroyVar(var.payload.tableValue[i].value);
    }
    stbds_shfree(var.payload.tableValue);
  } break;
  default:
    break;
  };

  memset((void *)&var, 0x0, sizeof(CBVar));
}

static void _cloneVarSlow(CBVar &dst, const CBVar &src) {
  switch (src.valueType) {
  case Seq: {
    auto srcLen = stbds_arrlen(src.payload.seqValue);
    // reuse if seq and we got enough capacity
    if (dst.valueType != Seq || stbds_arrcap(dst.payload.seqValue) < srcLen) {
      destroyVar(dst);
      dst.valueType = Seq;
      dst.payload.seqValue = nullptr;
    } else {
      auto dstLen = stbds_arrlen(dst.payload.seqValue);
      if (srcLen < dstLen) {
        // need to destroy leftovers
        for (auto i = srcLen; i < dstLen; i++) {
          destroyVar(dst.payload.seqValue[i]);
        }
      }
    }

    stbds_arrsetlen(dst.payload.seqValue, (unsigned)srcLen);
    for (auto i = 0; i < srcLen; i++) {
      auto &subsrc = src.payload.seqValue[i];
      memset(&dst.payload.seqValue[i], 0x0, sizeof(CBVar));
      cloneVar(dst.payload.seqValue[i], subsrc);
    }
  } break;
  case String:
  case ContextVar: {
    auto srcLen = strlen(src.payload.stringValue);
    if ((dst.valueType != String && dst.valueType != ContextVar) ||
        strlen(dst.payload.stringValue) < srcLen) {
      destroyVar(dst);
      dst.payload.stringValue = new char[srcLen + 1];
    }

    dst.valueType = src.valueType;
    memcpy((void *)dst.payload.stringValue, (void *)src.payload.stringValue,
           srcLen);
    ((char *)dst.payload.stringValue)[srcLen] = '\0';
  } break;
  case Image: {
    auto srcImgSize = src.payload.imageValue.height *
                      src.payload.imageValue.width *
                      src.payload.imageValue.channels;
    auto dstImgSize = dst.payload.imageValue.height *
                      dst.payload.imageValue.width *
                      dst.payload.imageValue.channels;
    if (dst.valueType != Image || srcImgSize > dstImgSize) {
      destroyVar(dst);
      dst.valueType = Image;
      dst.payload.imageValue.height = src.payload.imageValue.height;
      dst.payload.imageValue.width = src.payload.imageValue.width;
      dst.payload.imageValue.channels = src.payload.imageValue.channels;
      dst.payload.imageValue.data = new uint8_t[dstImgSize];
    }

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
  default:
    break;
  };
}

ALWAYS_INLINE inline void destroyVar(CBVar &var) {
  switch (var.valueType) {
  case Table:
  case Seq:
    _destroyVarSlow(var);
    return;
  case CBType::String:
  case ContextVar: {
    delete[] var.payload.stringValue;
    break;
  }
  case Image: {
    delete[] var.payload.imageValue.data;
    break;
  }
  default:
    break;
  };

  memset((void *)&var.payload, 0x0, sizeof(CBVarPayload));
  var.valueType = None;
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

struct Var : public CBVar {
  explicit Var() : CBVar() {
    valueType = None;
    payload.chainState = CBChainState::Continue;
  }

  explicit Var(const CBVar &other) {
    memcpy((void *)this, (void *)&other, sizeof(CBVar));
  }

  explicit operator bool() const {
    if (valueType != Bool) {
      throw CBException("Invalid variable casting! expected Bool");
    }
    return payload.boolValue;
  }

  explicit operator int() const {
    if (valueType != Int) {
      throw CBException("Invalid variable casting! expected Int");
    }
    return static_cast<int>(payload.intValue);
  }

  explicit operator uintptr_t() const {
    if (valueType != Int) {
      throw CBException("Invalid variable casting! expected Int");
    }
    return static_cast<uintptr_t>(payload.intValue);
  }

  explicit operator int16_t() const {
    if (valueType != Int) {
      throw CBException("Invalid variable casting! expected Int");
    }
    return static_cast<int16_t>(payload.intValue);
  }

  explicit operator uint8_t() const {
    if (valueType != Int) {
      throw CBException("Invalid variable casting! expected Int");
    }
    return static_cast<uint8_t>(payload.intValue);
  }

  explicit operator int64_t() const {
    if (valueType != Int) {
      throw CBException("Invalid variable casting! expected Int");
    }
    return payload.intValue;
  }

  explicit operator float() const {
    if (valueType == Float) {
      return static_cast<float>(payload.floatValue);
    } else if (valueType == Int) {
      return static_cast<float>(payload.intValue);
    } else {
      throw CBException("Invalid variable casting! expected Float");
    }
  }

  explicit operator double() const {
    if (valueType == Float) {
      return payload.floatValue;
    } else if (valueType == Int) {
      return static_cast<double>(payload.intValue);
    } else {
      throw CBException("Invalid variable casting! expected Float");
    }
  }

  static Var Stop() {
    Var res;
    res.valueType = None;
    res.payload.chainState = CBChainState::Stop;
    return res;
  }

  static Var Restart() {
    Var res;
    res.valueType = None;
    res.payload.chainState = CBChainState::Restart;
    return res;
  }

  static Var Return() {
    Var res;
    res.valueType = None;
    res.payload.chainState = CBChainState::Return;
    return res;
  }

  static Var Rebase() {
    Var res;
    res.valueType = None;
    res.payload.chainState = CBChainState::Rebase;
    return res;
  }

  template <typename T>
  static Var Object(T valuePtr, uint32_t objectVendorId,
                    uint32_t objectTypeId) {
    Var res;
    res.valueType = CBType::Object;
    res.payload.objectValue = valuePtr;
    res.payload.objectVendorId = objectVendorId;
    res.payload.objectTypeId = objectTypeId;
    return res;
  }

  template <typename T>
  static Var Enum(T value, uint32_t enumVendorId, uint32_t enumTypeId) {
    Var res;
    res.valueType = CBType::Enum;
    res.payload.enumValue = CBEnum(value);
    res.payload.enumVendorId = enumVendorId;
    res.payload.enumTypeId = enumTypeId;
    return res;
  }

  Var(uint8_t *ptr, int64_t size) : CBVar() {
    valueType = Bytes;
    payload.bytesSize = size;
    payload.bytesValue = ptr;
  }

  explicit Var(int src) : CBVar() {
    valueType = Int;
    payload.intValue = src;
  }

  explicit Var(int a, int b) : CBVar() {
    valueType = Int2;
    payload.int2Value[0] = a;
    payload.int2Value[1] = b;
  }

  explicit Var(int64_t a, int64_t b) : CBVar() {
    valueType = Int2;
    payload.int2Value[0] = a;
    payload.int2Value[1] = b;
  }

  explicit Var(double a, double b) : CBVar() {
    valueType = Float2;
    payload.float2Value[0] = a;
    payload.float2Value[1] = b;
  }

  explicit Var(float a, float b) : CBVar() {
    valueType = Float2;
    payload.float2Value[0] = a;
    payload.float2Value[1] = b;
  }

  explicit Var(double src) : CBVar() {
    valueType = Float;
    payload.floatValue = src;
  }

  explicit Var(bool src) : CBVar() {
    valueType = Bool;
    payload.boolValue = src;
  }

  explicit Var(CBSeq seq) : CBVar() {
    valueType = Seq;
    payload.seqValue = seq;
  }

  explicit Var(CBChain *src) : CBVar() {
    valueType = Chain;
    payload.chainValue = src;
  }

  explicit Var(CBImage img) : CBVar() {
    valueType = Image;
    payload.imageValue = img;
  }

  explicit Var(uint64_t src) : CBVar() {
    valueType = Int;
    payload.intValue = src;
  }

  explicit Var(int64_t src) : CBVar() {
    valueType = Int;
    payload.intValue = src;
  }

  explicit Var(const char *src) : CBVar() {
    valueType = CBType::String;
    payload.stringValue = src;
  }

  explicit Var(const std::string &src) : CBVar() {
    valueType = CBType::String;
    payload.stringValue = src.c_str();
  }

  explicit Var(CBTable &src) : CBVar() {
    valueType = Table;
    payload.tableValue = src;
  }

  explicit Var(CBColor color) : CBVar() {
    valueType = Color;
    payload.colorValue = color;
  }

  explicit Var(CBSeq &storage, const std::vector<std::string> &strings)
      : CBVar() {
    valueType = Seq;
    for (auto &str : strings) {
      stbds_arrpush(storage, Var(str));
    }
    payload.seqValue = storage;
  }
};

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
      for (auto i = 0; i < stbds_arrlen(output.payload.seqValue); i++) {
        varFree(output.payload.seqValue[i]);
      }
      stbds_arrfree(output.payload.seqValue);
      output.payload.seqValue = nullptr;
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

    memset(&output, 0x0, sizeof(CBVar));
  }

  template <class BinaryReader>
  static inline void deserialize(BinaryReader read, CBVar &output) {
    // we try to recycle memory so pass a empty None as first timer!
    CBType nextType;
    read((uint8_t *)&nextType, sizeof(output.valueType));

    // use some of the reserved bytes... because we can
    auto oactualSize = reinterpret_cast<uint64_t *>(output.reserved);

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
      auto availBytes = recycle ? *oactualSize : 0;
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
      *oactualSize = std::max(availBytes, output.payload.bytesSize);

      read((uint8_t *)output.payload.bytesValue, output.payload.bytesSize);
      break;
    }
    case CBType::String:
    case CBType::ContextVar: {
      auto availChars = recycle ? *oactualSize : 0;
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
      *oactualSize = std::max(availChars, len);

      read((uint8_t *)output.payload.stringValue, len);
      const_cast<char *>(output.payload.stringValue)[len] = 0;
      break;
    }
    case CBType::Seq: {
      uint64_t len;
      read((uint8_t *)&len, sizeof(uint64_t));

      auto currentUsed = recycle ? stbds_arrlen(output.payload.seqValue) : 0;
      if (currentUsed > len) {
        // in this case we need to destroy the excess items
        // before resizing
        for (auto i = len; i < currentUsed; i++) {
          varFree(output.payload.seqValue[i]);
        }
      }

      stbds_arrsetlen(output.payload.seqValue, len);
      for (auto i = 0; i < len; i++) {
        deserialize(read, output.payload.seqValue[i]);
      }
      break;
    }
    case CBType::Table: {
      if (recycle) // tables are slow for now...
        varFree(output);

      uint64_t len;
      read((uint8_t *)&len, sizeof(uint64_t));
      for (auto i = 0; i < len; i++) {
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

      auto size = output.payload.imageValue.channels *
                  output.payload.imageValue.height *
                  output.payload.imageValue.width;

      auto currentSize = recycle ? *oactualSize : 0;
      if (currentSize > 0 && currentSize < size) {
        // delete first & alloc
        delete[] output.payload.imageValue.data;
        output.payload.imageValue.data = new uint8_t[size];
      } else if (currentSize == 0) {
        // just alloc
        output.payload.imageValue.data = new uint8_t[size];
      }

      // record actualSize
      *oactualSize = std::max(currentSize, (uint64_t)size);

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
      uint64_t len = stbds_arrlen(input.payload.seqValue);
      write((const uint8_t *)&len, sizeof(uint64_t));
      total += sizeof(uint64_t);
      for (auto i = 0; i < len; i++) {
        total += serialize(input.payload.seqValue[i], write);
      }
      break;
    }
    case CBType::Table: {
      uint64_t len = stbds_shlen(input.payload.tableValue);
      write((const uint8_t *)&len, sizeof(uint64_t));
      total += sizeof(uint64_t);
      for (auto i = 0; i < len; i++) {
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

struct ContextableVar {
  CBVar _v{};
  CBVar *_cp = nullptr;
  CBContext *_ctx = nullptr;

  ContextableVar() {}

  ContextableVar(CBVar initialValue) {
    // notice, no cloning here!, purely utility
    _v = initialValue;
  }

  ContextableVar(CBVar initialValue, bool clone) {
    if (clone) {
      cloneVar(_v, initialValue);
    } else {
      _v = initialValue;
    }
  }

  ~ContextableVar() { destroyVar(_v); }

  void setParam(const CBVar &value) {
    cloneVar(_v, value);
    _cp = nullptr; // reset this!
  }

  CBVar &getParam() { return _v; }

  CBVar &get(CBContext *ctx, bool global = false) {
    if (unlikely(ctx != _ctx)) {
      // reset the ptr if context changed (stop/restart etc)
      _cp = nullptr;
      _ctx = ctx;
    }

    if (_v.valueType == ContextVar) {
      if (unlikely(!_cp)) {
        _cp = contextVariable(ctx, _v.payload.stringValue, global);
        return *_cp;
      } else {
        return *_cp;
      }
    } else {
      return _v;
    }
  }

  bool isVariable() { return _v.valueType == ContextVar; }

  const char *variableName() {
    if (isVariable())
      return _v.payload.stringValue;
    else
      return nullptr;
  }
};

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

  TypeInfo(const TypeInfo &other) {
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

  explicit TypesInfo(TypeInfo singleType, bool canBeSeq = false) {
    _innerInfo = nullptr;
    _innerTypes.reserve(canBeSeq ? 2 : 1);
    _innerTypes.push_back(singleType);
    stbds_arrpush(_innerInfo, _innerTypes.back());
    if (canBeSeq) {
      _innerTypes.push_back(TypeInfo::Sequence(_innerTypes.back()));
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

  ~VarStringStream() { cbDestroyVar(&previousValue); }

  void write(const CBVar &var) {
    if (var != previousValue) {
      cache.reset();
      std::ostream stream(&cache);
      stream << var;
      cache.done();
      cbCloneVar(&previousValue, &var);
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
      cbCloneVar(&previousValue, &var);
    }
  }

  const char *str() { return cache.str(); }
};
}; // namespace chainblocks
