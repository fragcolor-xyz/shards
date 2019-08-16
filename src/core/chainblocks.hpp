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

class CBException : public std::exception {
public:
  explicit CBException(const char *errmsg) : errorMessage(errmsg) {}

  [[nodiscard]] const char *what() const noexcept override {
    return errorMessage;
  }

private:
  const char *errorMessage;
};

struct Var : public CBVar {
  ~Var() {
    // Dangerous... need to pull this out or something, TODO
    if (valueType == Seq) {
      for (auto i = 0; stbds_arrlen(payload.seqValue) > i; i++) {
        chainblocks_DestroyVar(&payload.seqValue[i]);
      }
      stbds_arrfree(payload.seqValue);
    }
  }

  explicit Var() : CBVar() {
    valueType = None;
    payload.chainState = Continue;
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

  explicit Var(int src) : CBVar() {
    valueType = Int;
    payload.intValue = src;
  }

  explicit Var(double src) : CBVar() {
    valueType = Float;
    payload.intValue = src;
  }

  explicit Var(bool src) : CBVar() {
    valueType = Bool;
    payload.boolValue = src;
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
static Var Empty = Var();

struct TypesInfo {
  // TODO this is messy, need to make some decisions on storage and clean up!!
  TypesInfo() {
    _innerInfo = nullptr;
    CBTypeInfo t = {None};
    stbds_arrpush(_innerInfo, t);
  }

  TypesInfo(const TypesInfo &other) {
    _innerInfo = nullptr;
    for (auto i = 0; stbds_arrlen(other._innerInfo) > i; i++) {
      stbds_arrpush(_innerInfo, other._innerInfo[i]);
      if (other._innerInfo[i].basicType == Table) {
        auto otherInfo = other._innerInfo[i];
        _innerInfo[i].tableKeys = nullptr;
        for (auto y = 0; y < stbds_arrlen(otherInfo.tableKeys); y++) {
          stbds_arrpush(_innerInfo[i].tableKeys, otherInfo.tableKeys[y]);
        }
      }
    }
  }

  TypesInfo &operator=(const TypesInfo &other) {
    stbds_arrsetlen(_innerInfo, 0);
    for (auto i = 0; stbds_arrlen(other._innerInfo) > i; i++) {
      stbds_arrpush(_innerInfo, other._innerInfo[i]);
      if (other._innerInfo[i].basicType == Table) {
        auto otherInfo = other._innerInfo[i];
        _innerInfo[i].tableKeys = nullptr;
        for (auto y = 0; y < stbds_arrlen(otherInfo.tableKeys); y++) {
          stbds_arrpush(_innerInfo[i].tableKeys, otherInfo.tableKeys[y]);
        }
      }
    }
    return *this;
  }

  explicit TypesInfo(CBTypeInfo singleType, bool canBeSeq = false) {
    _innerInfo = nullptr;
    stbds_arrpush(_innerInfo, singleType);
    if (canBeSeq) {
      _subTypes.emplace_back(CBType::Seq, CBTypesInfo(*this));
      stbds_arrpush(_innerInfo, CBTypeInfo(_subTypes[0]));
    }
  }

  explicit TypesInfo(CBTypesInfo types, bool canBeSeq = false) {
    _innerInfo = nullptr;
    for (auto i = 0; i < stbds_arrlen(types); i++) {
      stbds_arrpush(_innerInfo, types[i]);
    }
    if (canBeSeq) {
      _subTypes.emplace_back(CBType::Seq, CBTypesInfo(*this));
      stbds_arrpush(_innerInfo, CBTypeInfo(_subTypes[0]));
    }
  }

  static TypesInfo Object(int32_t objectVendorId, int32_t objectTypeId,
                          bool canBeSeq = false) {
    TypesInfo result;
    result._innerInfo = nullptr;
    CBTypeInfo objType = {CBType::Object};
    objType.objectVendorId = objectVendorId;
    objType.objectTypeId = objectTypeId;
    stbds_arrpush(result._innerInfo, objType);
    if (canBeSeq) {
      result._subTypes.emplace_back(CBType::Seq, CBTypesInfo(result));
      stbds_arrpush(result._innerInfo, CBTypeInfo(result._subTypes[0]));
    }
    return result;
  }

  TypesInfo(CBType singleType, CBTypesInfo contentsInfo,
            const char *name = nullptr) {
    _innerInfo = nullptr;
    CBTypeInfo t = {singleType};
    stbds_arrpush(_innerInfo, t);

    assert(_innerInfo[0].basicType == Table || _innerInfo[0].basicType == Seq);

    if (_innerInfo[0].basicType == Table) {
      _innerInfo[0].tableTypes = CBTypesInfo(contentsInfo);
      _innerInfo[0].tableKeys = nullptr;
      stbds_arrpush(_innerInfo[0].tableKeys, name);
    } else if (_innerInfo[0].basicType == Seq)
      _innerInfo[0].seqTypes = CBTypesInfo(contentsInfo);
  }

  explicit TypesInfo(CBType singleType, bool canBeSeq = false) {
    _innerInfo = nullptr;
    CBTypeInfo t = {singleType};
    stbds_arrpush(_innerInfo, t);
    if (canBeSeq) {
      _subTypes.emplace_back(CBType::Seq, CBTypesInfo(*this));
      stbds_arrpush(_innerInfo, CBTypeInfo(_subTypes[0]));
    }
  }

  template <typename... Args> static TypesInfo FromMany(Args... types) {
    TypesInfo result;
    result._innerInfo = nullptr;
    std::vector<CBType> vec = {types...};
    for (auto type : vec) {
      CBTypeInfo t = {type};
      stbds_arrpush(result._innerInfo, t);
    }
    return result;
  }

  template <typename... Args> static TypesInfo FromManyTypes(Args... types) {
    TypesInfo result;
    result._innerInfo = nullptr;
    std::vector<CBTypeInfo> vec = {types...};
    for (auto type : vec) {
      stbds_arrpush(result._innerInfo, type);
    }
    return result;
  }

  ~TypesInfo() {
    if (_innerInfo) {
      if (stbds_arrlen(_innerInfo) > 0) {
        if (_innerInfo[0].basicType == Table && _innerInfo[0].tableKeys)
          stbds_arrfree(_innerInfo[0].tableKeys);
      }
      stbds_arrfree(_innerInfo);
    }
  }

  explicit operator CBTypesInfo() const { return _innerInfo; }

  explicit operator CBTypeInfo() const { return _innerInfo[0]; }

  CBTypesInfo _innerInfo;
  std::vector<TypesInfo> _subTypes;
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

  ~VarStringStream() { chainblocks_DestroyVar(&previousValue); }

  void write(const CBVar &var) {
    if (var != previousValue) {
      cache.reset();
      std::ostream stream(&cache);
      stream << var;
      cache.done();
      chainblocks_CloneVar(&previousValue, &var);
    }
  }

  const char *str() { return cache.str(); }
};
}; // namespace chainblocks
