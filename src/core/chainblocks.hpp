#pragma once

#include "chainblocks.h"
#include "ops.hpp"

// Included 3rdparty
#include "3rdparty/easylogging++.h"
#include "3rdparty/json.hpp"
#include "3rdparty/parallel_hashmap/phmap.h"

#include <cassert>
#include <type_traits>

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
  explicit CBException(std::string &errmsg) : errorMessage(errmsg) {}

  [[nodiscard]] const char *what() const noexcept override {
    return errorMessage.c_str();
  }

private:
  std::string errorMessage;
};

ALWAYS_INLINE inline int destroyVar(CBVar &var);
ALWAYS_INLINE inline int cloneVar(CBVar &dst, const CBVar &src);

static int _destroyVarSlow(CBVar &var) {
  int freeCount = 0;
  switch (var.valueType) {
  case Seq: {
    int len = stbds_arrlen(var.payload.seqValue);
    for (int i = 0; i < len; i++) {
      freeCount += destroyVar(var.payload.seqValue[i]);
    }
    stbds_arrfree(var.payload.seqValue);
    freeCount++;
  } break;
  case Table: {
    auto len = stbds_shlen(var.payload.tableValue);
    for (auto i = 0; i < len; i++) {
      freeCount += destroyVar(var.payload.tableValue[i].value);
    }
    stbds_shfree(var.payload.tableValue);
    freeCount++;
  } break;
  default:
    break;
  };

  memset((void *)&var, 0x0, sizeof(CBVar));

  return freeCount;
}

static int _cloneVarSlow(CBVar &dst, const CBVar &src) {
  int freeCount = 0;
  switch (src.valueType) {
  case Seq: {
    int srcLen = stbds_arrlen(src.payload.seqValue);
    // reuse if seq and we got enough capacity
    if (dst.valueType != Seq ||
        (int)stbds_arrcap(dst.payload.seqValue) < srcLen) {
      freeCount += destroyVar(dst);
      dst.valueType = Seq;
      dst.payload.seqLen = -1;
      dst.payload.seqValue = nullptr;
    } else {
      int dstLen = stbds_arrlen(dst.payload.seqValue);
      if (srcLen < dstLen) {
        // need to destroy leftovers
        for (auto i = srcLen; i < dstLen; i++) {
          freeCount += destroyVar(dst.payload.seqValue[i]);
        }
      }
    }

    stbds_arrsetlen(dst.payload.seqValue, (unsigned)srcLen);
    for (auto i = 0; i < srcLen; i++) {
      auto &subsrc = src.payload.seqValue[i];
      memset(&dst.payload.seqValue[i], 0x0, sizeof(CBVar));
      freeCount += cloneVar(dst.payload.seqValue[i], subsrc);
    }
  } break;
  case String:
  case ContextVar: {
    auto srcLen = strlen(src.payload.stringValue);
    if ((dst.valueType != String && dst.valueType != ContextVar) ||
        strlen(dst.payload.stringValue) < srcLen) {
      freeCount += destroyVar(dst);
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
      freeCount += destroyVar(dst);
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
    freeCount += destroyVar(dst);
    dst.valueType = Table;
    dst.payload.tableLen = -1;
    dst.payload.tableValue = nullptr;
    stbds_sh_new_arena(dst.payload.tableValue);
    auto srcLen = stbds_shlen(src.payload.tableValue);
    for (auto i = 0; i < srcLen; i++) {
      CBVar clone{};
      freeCount += cloneVar(clone, src.payload.tableValue[i].value);
      stbds_shput(dst.payload.tableValue, src.payload.tableValue[i].key, clone);
    }
  } break;
  default:
    break;
  };
  return freeCount;
}

ALWAYS_INLINE inline int destroyVar(CBVar &var) {
  int freeCount = 0;
  switch (var.valueType) {
  case Table:
  case Seq:
    return _destroyVarSlow(var);
  case String:
  case ContextVar: {
    delete[] var.payload.stringValue;
    freeCount++;
    break;
  }
  case Image: {
    delete[] var.payload.imageValue.data;
    freeCount++;
    break;
  }
  default:
    break;
  };

  memset((void *)&var, 0x0, sizeof(CBVar));

  return freeCount;
}

ALWAYS_INLINE inline int cloneVar(CBVar &dst, const CBVar &src) {
  int freeCount = 0;
  if (src.valueType < EndOfBlittableTypes &&
      dst.valueType < EndOfBlittableTypes) {
    memcpy((void *)&dst, (const void *)&src, sizeof(CBVar));
  } else if (src.valueType < EndOfBlittableTypes) {
    freeCount += destroyVar(dst);
    memcpy((void *)&dst, (const void *)&src, sizeof(CBVar));
  } else {
    return _cloneVarSlow(dst, src);
  }
  return freeCount;
}

class IterableSeq {
public:
  IterableSeq(CBSeq seq) : _seq(seq) {}
  class iterator {
  public:
    iterator(CBVar *ptr) : ptr(ptr) {}
    iterator operator++() {
      ++ptr;
      return *this;
    }
    bool operator!=(const iterator &other) const { return ptr != other.ptr; }
    const CBVar &operator*() const { return *ptr; }

  private:
    CBVar *ptr;
  };

private:
  CBSeq _seq;

public:
  iterator begin() const { return iterator(&_seq[0]); }
  iterator end() const { return iterator(&_seq[0] + stbds_arrlen(_seq)); }
};

struct Var : public CBVar {
  ~Var() {
    // Dangerous... need to pull this out or something, TODO
    if (valueType == Seq) {
      for (auto i = 0; stbds_arrlen(payload.seqValue) > i; i++) {
        cbDestroyVar(&payload.seqValue[i]);
      }
      stbds_arrfree(payload.seqValue);
    }
  }

  explicit Var() : CBVar() {
    valueType = None;
    payload.chainState = CBChainState::Continue;
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

  explicit Var(int src) : CBVar() {
    valueType = Int;
    payload.intValue = src;
  }

  explicit Var(int a, int b) : CBVar() {
    valueType = Int2;
    payload.int2Value[0] = a;
    payload.int2Value[1] = b;
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
    payload.seqLen = -1;
    payload.seqValue = seq;
  }

  explicit Var(CBChain *src) : CBVar() {
    valueType = Chain;
    payload.chainValue = src;
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

  explicit Var(std::string &src) : CBVar() {
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

  explicit Var(const std::vector<CBlock *> &blocks) : CBVar() {
    valueType = Seq;
    payload.seqValue = nullptr;
    payload.seqLen = -1;
    for (auto block : blocks) {
      CBVar blockVar{};
      blockVar.valueType = Block;
      blockVar.payload.blockValue = block;
      stbds_arrpush(payload.seqValue, blockVar);
    }
  }
};

static Var True = Var(true);
static Var False = Var(false);
static Var StopChain = Var::Stop();
static Var RestartChain = Var::Restart();
static Var ReturnPrevious = Var::Return();
static Var RebaseChain = Var::Rebase();
static Var Empty = Var();

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

  template <typename... Types> explicit ParamsInfo(Types... types) {
    std::vector<CBParameterInfo> vec = {types...};
    _innerInfo = nullptr;
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

  template <typename... Types> explicit ExposedInfo(Types... types) {
    std::vector<CBExposedTypeInfo> vec = {types...};
    _innerInfo = nullptr;
    for (auto pi : vec) {
      stbds_arrpush(_innerInfo, pi);
    }
  }

  static CBExposedTypeInfo Variable(const char *name, const char *help,
                                    CBTypeInfo type) {
    CBExposedTypeInfo res = {name, help, type};
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

  const char *str() { return cache.str(); }
};
}; // namespace chainblocks
