/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#ifndef CB_CHAINBLOCKS_HPP
#define CB_CHAINBLOCKS_HPP

#include "chainblocks.h"
#include <string>
#include <utility>
#include <vector>

namespace chainblocks {
class CBException : public std::exception {
public:
  explicit CBException(const char *errmsg) : errorMessage(errmsg) {}
  explicit CBException(std::string errmsg) : errorMessage(std::move(errmsg)) {}

  [[nodiscard]] const char *what() const noexcept override {
    return errorMessage.c_str();
  }

private:
  std::string errorMessage;
};

CBlock *createBlock(const char *name);

struct Type {
  CBTypeInfo _type;

  Type(CBTypeInfo type) { _type = type; }

  operator CBTypesInfo() {
    CBTypesInfo res{&_type, 1, 0};
    return res;
  }

  operator CBTypeInfo() { return _type; }
};

struct Types {
  std::vector<CBTypeInfo> _types;

  Types(std::initializer_list<CBTypeInfo> types) : _types(types) {}

  Types(const Types &others, std::initializer_list<CBTypeInfo> types) {
    for (auto &type : others._types) {
      _types.push_back(type);
    }
    for (auto &type : types) {
      _types.push_back(type);
    }
  }

  operator CBTypesInfo() {
    CBTypesInfo res{&_types[0], (uint32_t)_types.size(), 0};
    return res;
  }
};

struct ParameterInfo {
  const char *_name;
  const char *_help;
  Types _types;

  ParameterInfo(const char *name, const char *help, Types types)
      : _name(name), _help(help), _types(types) {}
  ParameterInfo(const char *name, Types types)
      : _name(name), _help(""), _types(types) {}

  operator CBParameterInfo() {
    CBParameterInfo res{_name, _help, _types};
    return res;
  }
};

struct Parameters {
  std::vector<ParameterInfo> _infos;
  std::vector<CBParameterInfo> _pinfos;

  Parameters(std::initializer_list<ParameterInfo> infos) : _infos(infos) {
    for (auto &info : _infos) {
      _pinfos.push_back(info);
    }
  }

  Parameters(const Parameters &others,
             std::initializer_list<ParameterInfo> infos) {
    for (auto &info : others._infos) {
      _infos.push_back(info);
    }
    for (auto &info : infos) {
      _infos.push_back(info);
    }
    for (auto &info : _infos) {
      _pinfos.push_back(info);
    }
  }

  operator CBParametersInfo() {
    CBParametersInfo res{&_pinfos[0], (uint32_t)_pinfos.size(), 0};
    return res;
  }
};

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

  explicit Var(int a, int b, int c) : CBVar() {
    valueType = Int3;
    payload.int3Value[0] = a;
    payload.int3Value[1] = b;
    payload.int3Value[2] = c;
  }

  explicit Var(int a, int b, int c, int d) : CBVar() {
    valueType = Int4;
    payload.int4Value[0] = a;
    payload.int4Value[1] = b;
    payload.int4Value[2] = c;
    payload.int4Value[3] = d;
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

  explicit Var(double a, double b, double c) : CBVar() {
    valueType = Float3;
    payload.float3Value[0] = a;
    payload.float3Value[1] = b;
    payload.float3Value[2] = c;
  }

  explicit Var(double a, double b, double c, double d) : CBVar() {
    valueType = Float4;
    payload.float4Value[0] = a;
    payload.float4Value[1] = b;
    payload.float4Value[2] = c;
    payload.float4Value[3] = d;
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
};

class ChainProvider {
  // used specially for live editing chains, from host languages
public:
  class Info {
  public:
    Info() {
      CBTypeInfo none{};
      _infos.push_back(none);
      CBTypeInfo objInfo{CBType::Object};
      objInfo.objectVendorId = 'frag';
      objInfo.objectTypeId = 'chnp';
      _infos.push_back(objInfo);
      _providerOrNone.elements = &_infos[0];
      _providerOrNone.len = 2;
    }

    operator CBTypeInfo() const { return _infos[1]; }

    operator CBTypesInfo() const { return _providerOrNone; }

  private:
    std::vector<CBTypeInfo> _infos;
    CBTypesInfo _providerOrNone = {};
  };

  struct Update {
    const char *error;
    CBChain *chain;
  };

  static inline Info Info{};

  ChainProvider() {
    _provider.userData = this;
    _provider.reset = [](CBChainProvider *provider) {
      auto p = reinterpret_cast<ChainProvider *>(provider->userData);
      p->reset();
    };
    _provider.ready = [](CBChainProvider *provider) {
      auto p = reinterpret_cast<ChainProvider *>(provider->userData);
      return p->ready();
    };
    _provider.setup = [](CBChainProvider *provider, const char *path,
                         CBInstanceData data) {
      auto p = reinterpret_cast<ChainProvider *>(provider->userData);
      p->setup(path, data);
    };
    _provider.updated = [](CBChainProvider *provider) {
      auto p = reinterpret_cast<ChainProvider *>(provider->userData);
      return p->updated();
    };
    _provider.acquire = [](CBChainProvider *provider) {
      auto p = reinterpret_cast<ChainProvider *>(provider->userData);
      return p->acquire();
    };
    _provider.release = [](CBChainProvider *provider, CBChain *chain) {
      auto p = reinterpret_cast<ChainProvider *>(provider->userData);
      return p->release(chain);
    };
  }
  ~ChainProvider() {}

  virtual void reset() = 0;

  virtual bool ready() = 0;
  virtual void setup(const char *path, const CBInstanceData &data) = 0;

  virtual bool updated() = 0;
  virtual CBChainProviderUpdate acquire() = 0;

  virtual void release(CBChain *chain) = 0;

  operator CBVar() {
    CBVar res{};
    res.valueType = Object;
    res.payload.objectVendorId = 'frag';
    res.payload.objectTypeId = 'chnp';
    res.payload.objectValue = &_provider;
    return res;
  }

private:
  CBChainProvider _provider;
};

class Chain {
public:
  template <typename String>
  Chain(String name) : _name(name), _looped(false), _unsafe(false) {}
  Chain() : _looped(false), _unsafe(false) {}

  template <typename String, typename... Vars>
  Chain &block(String name, Vars... params) {
    std::string blockName(name);
    auto blk = createBlock(blockName.c_str());
    blk->setup(blk);

    std::vector<Var> vars = {Var(params)...};
    for (size_t i = 0; i < vars.size(); i++) {
      blk->setParam(blk, i, vars[i]);
    }

    _blocks.push_back(blk);
    return *this;
  }

  template <typename V> Chain &let(V value) {
    auto blk = createBlock("Const");
    blk->setup(blk);
    blk->setParam(blk, 0, Var(value));
    _blocks.push_back(blk);
    return *this;
  }

  Chain &looped(bool looped) {
    _looped = looped;
    return *this;
  }

  Chain &unsafe(bool unsafe) {
    _unsafe = unsafe;
    return *this;
  }

  template <typename String> Chain &name(String name) {
    _name = name;
    return *this;
  }

  operator CBChain *();

  // -- LEAKS --
  // operator CBVar() {
  //   CBVar res{};
  //   res.valueType = Seq;
  //   for (auto blk : _blocks) {
  //     CBVar blkVar{};
  //     blkVar.valueType = Block;
  //     blkVar.payload.blockValue = blk;
  //     stbds_arrpush(res.payload.seqValue, blkVar);
  //   }
  //   // blocks are unique so drain them here
  //   _blocks.clear();
  //   return res;
  // }

private:
  std::string _name;
  bool _looped;
  bool _unsafe;
  std::vector<CBlock *> _blocks;
};
} // namespace chainblocks

#endif
