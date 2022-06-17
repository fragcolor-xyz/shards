/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#ifndef SH_UTILITY_HPP
#define SH_UTILITY_HPP

#include "shards.hpp"
#include <cassert>
#include <future>
#include <magic_enum.hpp>
#include <memory>
#include <mutex>
#include <nameof.hpp>
#include <string>
#include <vector>

namespace shards {
// SHVar strings can have an optional len field populated
#define SHSTRLEN(_v_) \
  (_v_.payload.stringLen > 0 || _v_.payload.stringValue == nullptr ? _v_.payload.stringLen : strlen(_v_.payload.stringValue))

#define SHSTRVIEW(_v_) std::string_view(_v_.payload.stringValue, SHSTRLEN(_v_))

// compile time CRC32
constexpr uint32_t crc32(std::string_view str) {
  constexpr uint32_t crc_table[] = {
      0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4,
      0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
      0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
      0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
      0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac, 0x51de003a,
      0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
      0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f,
      0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
      0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
      0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65, 0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
      0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5,
      0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
      0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6,
      0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
      0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1, 0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
      0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
      0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c,
      0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
      0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31,
      0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
      0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
      0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777, 0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
      0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7,
      0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
      0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8,
      0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d};
  uint32_t crc = 0xffffffff;
  for (auto c : str)
    crc = (crc >> 8) ^ crc_table[(crc ^ c) & 0xff];
  return crc ^ 0xffffffff;
}

template <auto V> struct constant { constexpr static decltype(V) value = V; };

inline SHOptionalString operator"" _optional(const char *s, size_t) { return SHOptionalString{s}; }

// SFINAE tests
#define SH_HAS_MEMBER_TEST(_name_)                               \
  template <typename T> class has_##_name_ {                     \
    typedef char one;                                            \
    struct two {                                                 \
      char x[2];                                                 \
    };                                                           \
    template <typename C> static one test(decltype(&C::_name_)); \
    template <typename C> static two test(...);                  \
                                                                 \
  public:                                                        \
    enum { value = sizeof(test<T>(0)) == sizeof(char) };         \
  }

/*
  Lazy initialization of a static variable.

  The idea is that shards on the same thread share this variable
  And that as long as there is an instance of a shard we don't deallocate
  anything Once all shards are gone we cleanup

  To be used WITHOUT static keywords
*/
template <typename T, void (*OnDelete)(T *p) = nullptr> class ThreadShared {
public:
  ThreadShared() { addRef(); }

  ~ThreadShared() { decRef(); }

  T &get() {
    if (!_tp) {
      // create
      auto p = new T(); // keep for debugging sanity
      auto mem = &_tp;  // keep for debugging sanity
      _tp = p;
      // lock now since ptrs is shared
      std::unique_lock<std::mutex> lock(getState().mutex);

      // store thread local location
      getState().ptrs.push_back(mem);
      // also store the pointer in the global list
      getState().objs.push_back(p);
    }

    return *_tp;
  }

  T &operator()() { return get(); }
  T &operator*() { return get(); }
  T *operator->() { return &get(); }

private:
  void addRef() {
    std::unique_lock<std::mutex> lock(getState().mutex);

    getState().refs++;
  }

  void decRef() {
    std::unique_lock<std::mutex> lock(getState().mutex);

    getState().refs--;

    if (getState().refs == 0) {
      for (auto ptr : getState().ptrs) {
        // null the thread local
        // the idea is that we clean up even the thread still exists
        *ptr = nullptr;
      }
      getState().ptrs.clear();

      for (auto ptr : getState().objs) {
        if (!ptr)
          continue;

        if constexpr (OnDelete != nullptr) {
          OnDelete(ptr);
        }
        // delete the internal object
        delete ptr;
      }
      getState().objs.clear();
    }
  }

  struct State {
    std::mutex mutex;
    uint64_t refs = 0;
    std::vector<T **> ptrs;
    std::vector<T *> objs;
  };
  static inline thread_local T *_tp = nullptr;

  static State &getState() {
    static State state;
    return state;
  }
};

/*
  Lazy initialization of a static variable.
*/
template <typename T, typename ARG = void *, ARG defaultValue = nullptr> class Shared {
public:
  Shared() { addRef(); }

  ~Shared() { decRef(); }

  T &get() {
    if (!getState().tp) {
      std::unique_lock<std::mutex> lock(getState().mutex);
      // check again as another thread might have locked
      if constexpr (!std::is_same<ARG, void *>::value) {
        if (!getState().tp)
          getState().tp = new T(defaultValue);
      } else {
        if (!getState().tp)
          getState().tp = new T();
      }
    }

    return *getState().tp;
  }

  T &operator()() { return get(); }
  T &operator*() { return get(); }
  T *operator->() { return &get(); }

private:
  void addRef() {
    std::unique_lock<std::mutex> lock(getState().mutex);

    getState().refs++;
  }

  void decRef() {
    std::unique_lock<std::mutex> lock(getState().mutex);

    getState().refs--;

    if (getState().refs == 0 && getState().tp) {
      // delete the internal object
      delete getState().tp;
      // null the thread local
      getState().tp = nullptr;
    }
  }

  struct State {
    std::mutex mutex;
    uint64_t refs = 0;
    T *tp = nullptr;
  };

  static State &getState() {
    static State state;
    return state;
  }
};

template <class SH_CORE> class TParamVar {
private:
  SHVar _v{};
  SHVar *_cp = nullptr;

public:
  TParamVar() {}

  explicit TParamVar(SHVar initialValue) { SH_CORE::cloneVar(_v, initialValue); }

  TParamVar(const TParamVar &other) = delete;
  TParamVar &operator=(const TParamVar &other) = delete;

  TParamVar(TParamVar &&other) : _v(other._v), _cp(other._cp) {
    other._cp = nullptr;
    memset(&other._v, 0, sizeof(SHVar));
  }

  TParamVar &operator=(TParamVar &&other) {
    _v = other._v;
    _cp = other._cp;
    other._cp = nullptr;
    memset(&other._v, 0, sizeof(SHVar));
  }

  ~TParamVar() {
    cleanup();
    SH_CORE::destroyVar(_v);
  }

  void warmup(SHContext *ctx) {
    assert(!_cp);
    if (_v.valueType == ContextVar) {
      assert(!_cp);
      _cp = SH_CORE::referenceVariable(ctx, _v.payload.stringValue);
    } else {
      _cp = &_v;
    }
    assert(_cp);
  }

  void cleanup() {
    if (_v.valueType == ContextVar) {
      SH_CORE::releaseVariable(_cp);
    }
    _cp = nullptr;
  }

  SHVar &operator=(const SHVar &value) {
    cleanup();
    SH_CORE::cloneVar(_v, value);
    return _v;
  }

  operator SHVar() const { return _v; }
  const SHVar *operator->() const { return &_v; }

  SHVar &get() {
    assert(_cp);
    return *_cp;
  }

  bool isVariable() { return _v.valueType == ContextVar; }

  const char *variableName() {
    if (isVariable())
      return _v.payload.stringValue;
    else
      return nullptr;
  }
};

template <class SH_CORE, typename E, bool isFlag = false> class TEnumInfo {
private:
  static constexpr auto enum_names() {
    if constexpr (isFlag) {
      return magic_enum::flags::enum_names<E>();
    } else {
      return magic_enum::enum_names<E>();
    }
  }
  static constexpr auto enum_values() {
    if constexpr (isFlag) {
      return magic_enum::flags::enum_values<E>();
    } else {
      return magic_enum::enum_values<E>();
    }
  }
  static constexpr auto eseq = enum_names();
  static constexpr auto vseq = enum_values();
  SHEnumInfo info;
  std::vector<std::string> labels;
  std::vector<SHString> clabels;
  std::vector<SHEnum> values;

public:
  TEnumInfo(const char *name, int32_t vendorId, int32_t enumId) {
    info.name = name;
    for (auto &view : eseq) {
      labels.emplace_back(view);
    }
    for (auto &s : labels) {
      clabels.emplace_back(s.c_str());
    }
    info.labels.elements = &clabels[0];
    info.labels.len = uint32_t(labels.size());

    for (auto &v : vseq) {
      values.emplace_back(SHEnum(v));
    }
    info.values.elements = &values[0];
    info.values.len = uint32_t(values.size());

    assert(info.values.len == info.labels.len);
    SH_CORE::registerEnumType(vendorId, enumId, info);
  }
};

template <class SH_CORE, typename E, std::vector<uint8_t> (*Serializer)(const E &) = nullptr,
          E (*Deserializer)(const std::string_view &) = nullptr, void (*BeforeDelete)(const E &) = nullptr>
class TObjectVar {
private:
  SHObjectInfo info;
  int32_t vendorId;
  int32_t typeId;

  struct ObjectRef {
    E shared;
    uint32_t refcount;
  };

public:
  TObjectVar(const char *name, int32_t vendorId, int32_t typeId) : vendorId(vendorId), typeId(typeId) {
    info = {};
    info.name = name;
    info.reference = [](SHPointer ptr) {
      auto p = reinterpret_cast<ObjectRef *>(ptr);
      p->refcount++;
    };
    info.release = [](SHPointer ptr) {
      auto p = reinterpret_cast<ObjectRef *>(ptr);
      p->refcount--;
      if (p->refcount == 0) {
        delete p;
      }
    };
    if constexpr (Serializer != nullptr && Deserializer != nullptr) {
      info.serialize = [](SHPointer obj, uint8_t **outData, size_t *outLen, SHPointer *customHandle) {
        auto tobj = reinterpret_cast<E *>(obj);
        auto holder = new std::vector<uint8_t>();
        *holder = Serializer(*tobj);
        *customHandle = holder;
        *outData = holder->data();
        *outLen = holder->size();
        return true;
      };
      info.free = [](SHPointer handle) {
        auto holder = reinterpret_cast<std::vector<uint8_t> *>(handle);
        delete holder;
      };
      info.deserialize = [](uint8_t *data, size_t len) {
        auto r = new ObjectRef();
        r->shared = Deserializer(std::string_view((char *)data, len));
        // don't bump ref count, deserializer is supposed to do that
        return (SHPointer)r;
      };
    }
    SH_CORE::registerObjectType(vendorId, typeId, info);
  }

  // the following methods are generally used by the shard
  // that creates the object, that's it.
  // other shards use regular referenceVariable etc.

  E *New() {
    auto r = new ObjectRef();
    r->refcount = 1;
    return &r->shared;
  }

  void Release(E *obj) {
    auto r = reinterpret_cast<ObjectRef *>(obj);
    r->refcount--;
    if (r->refcount == 0) {
      if constexpr (BeforeDelete != nullptr) {
        BeforeDelete(*obj);
      }
      delete r;
    }
  }

  SHVar Get(E *obj) {
    SHVar res;
    res.valueType = SHType::Object;
    res.payload.objectValue = obj;
    res.payload.objectVendorId = vendorId;
    res.payload.objectTypeId = typeId;
    res.flags = SHVAR_FLAGS_USES_OBJINFO;
    res.objectInfo = &info;
    return res;
  }
};

template <class SH_CORE> class TShardsVar {
private:
  SHVar _shardsParam{};               // param cache
  Shards _shards{};                   // var wrapper we pass to validate and activate
  std::vector<ShardPtr> _shardsArray; // shards actual storage
  SHComposeResult _wireValidation{};

  void destroy() {
    for (auto it = _shardsArray.rbegin(); it != _shardsArray.rend(); ++it) {
      auto blk = *it;
      auto errors = blk->cleanup(blk);
      if (errors.code != 0) {
        auto msg = "TShardsVar: Error during blocks cleanup: " + std::string(errors.message);
        SH_CORE::log(msg.c_str());
      }
      blk->destroy(blk);
    }
    _shardsArray.clear();
  }

public:
  ~TShardsVar() {
    destroy();
    SH_CORE::destroyVar(_shardsParam);
    SH_CORE::expTypesFree(_wireValidation.exposedInfo);
    SH_CORE::expTypesFree(_wireValidation.requiredInfo);
  }

  void cleanup() {
    for (auto it = _shardsArray.rbegin(); it != _shardsArray.rend(); ++it) {
      auto blk = *it;

      auto errors = blk->cleanup(blk);
      if (errors.code != 0) {
        auto msg = "TShardsVar: Error during blocks cleanup: " + std::string(errors.message);
        SH_CORE::log(msg.c_str());
      }
    }
  }

  void warmup(SHContext *context) {
    for (auto &blk : _shardsArray) {
      if (blk->warmup) {
        auto errors = blk->warmup(blk, context);
        if (errors.code != 0) {
          throw WarmupError(errors.message);
        }
      }
    }
  }

  SHVar &operator=(const SHVar &value) {
    shassert(value.valueType == None || value.valueType == ShardRef || value.valueType == Seq);

    SH_CORE::cloneVar(_shardsParam, value);

    destroy();
    if (_shardsParam.valueType == ShardRef) {
      assert(!_shardsParam.payload.shardValue->owned);
      _shardsParam.payload.shardValue->owned = true;
      _shardsArray.push_back(_shardsParam.payload.shardValue);
    } else {
      for (uint32_t i = 0; i < _shardsParam.payload.seqValue.len; i++) {
        auto blk = _shardsParam.payload.seqValue.elements[i].payload.shardValue;
        assert(!blk->owned);
        blk->owned = true;
        _shardsArray.push_back(blk);
      }
    }

    // We want to avoid copies in hot paths
    // So we write here the var we pass to CORE
    const auto nshards = _shardsArray.size();
    _shards.elements = nshards > 0 ? &_shardsArray[0] : nullptr;
    _shards.len = uint32_t(nshards);

    return _shardsParam;
  }

  operator SHVar() const { return _shardsParam; }

  SHComposeResult compose(const SHInstanceData &data) {
    // Free any previous result!
    SH_CORE::expTypesFree(_wireValidation.exposedInfo);
    SH_CORE::expTypesFree(_wireValidation.requiredInfo);

    _wireValidation = SH_CORE::composeShards(
        _shards,
        [](const Shard *errorShard, const char *errorTxt, bool nonfatalWarning, void *userData) {
          if (!nonfatalWarning) {
            auto msg = "Error during inner wire validation: " + std::string(errorTxt);
            SH_CORE::log(msg.c_str());
            throw shards::ComposeError("Failed inner wire validation.");
          } else {
            auto msg = "Warning during inner wire validation: " + std::string(errorTxt);
            SH_CORE::log(msg.c_str());
          }
        },
        this, data);
    return _wireValidation;
  }

  template <bool CALLER_HANDLES_RETURN = false> SHWireState activate(SHContext *context, const SHVar &input, SHVar &output) {
    if constexpr (CALLER_HANDLES_RETURN)
      return SH_CORE::runShards2(_shards, context, input, output);
    else
      return SH_CORE::runShards(_shards, context, input, output);
  }

  template <bool CALLER_HANDLES_RETURN = false>
  SHWireState activateHashed(SHContext *context, const SHVar &input, SHVar &output, SHVar &outHash) {
    if constexpr (CALLER_HANDLES_RETURN)
      return SH_CORE::runShardsHashed2(_shards, context, input, output, outHash);
    else
      return SH_CORE::runShardsHashed(_shards, context, input, output, outHash);
  }

  operator bool() const { return _shardsArray.size() > 0; }

  const Shards &shards() const { return _shards; }
};

template <class SH_CORE> struct TOwnedVar : public SHVar {
  TOwnedVar() : SHVar() {}
  TOwnedVar(TOwnedVar &&source) : SHVar() { *this = source; }
  TOwnedVar(const TOwnedVar &source) : SHVar() { SH_CORE::cloneVar(*this, source); }
  TOwnedVar(const SHVar &source) : SHVar() { SH_CORE::cloneVar(*this, source); }
  TOwnedVar &operator=(const SHVar &other) {
    SH_CORE::cloneVar(*this, other);
    return *this;
  }
  TOwnedVar &operator=(const TOwnedVar &other) {
    SH_CORE::cloneVar(*this, other);
    return *this;
  }
  TOwnedVar &operator=(TOwnedVar &&other) {
    SH_CORE::destroyVar(*this);
    *this = other;
    return *this;
  }
  ~TOwnedVar() { SH_CORE::destroyVar(*this); }
};

// helper to create structured data tables
// see XR's GamePadButtonTable for an example
template <class SH_CORE> struct TTableVar : public SHVar {
  TTableVar() : SHVar() {
    valueType = SHType::Table;
    payload.tableValue = SH_CORE::tableNew();
  }

  TTableVar(const TTableVar &other) : SHVar() { SH_CORE::cloneVar(*this, other); }

  TTableVar &operator=(const TTableVar &other) {
    SH_CORE::cloneVar(*this, other);
    return *this;
  }

  TTableVar(TTableVar &&other) : SHVar() { std::swap(*this, other); }

  TTableVar(std::initializer_list<std::pair<std::string_view, SHVar>> pairs) : TTableVar() {
    for (auto &kv : pairs) {
      auto &rdst = (*this)[kv.first];
      SH_CORE::cloneVar(rdst, kv.second);
    }
  }

  TTableVar(const TTableVar &others, std::initializer_list<std::pair<std::string_view, SHVar>> pairs) : TTableVar() {
    const auto &table = others.payload.tableValue;
    ForEach(table, [&](auto &key, auto &val) {
      auto &rdst = (*this)[key];
      SH_CORE::cloneVar(rdst, val);
    });

    for (auto &kv : pairs) {
      auto &rdst = (*this)[kv.first];
      SH_CORE::cloneVar(rdst, kv.second);
    }
  }

  TTableVar &operator=(TTableVar &&other) {
    std::swap(*this, other);
    memset(&other, 0x0, sizeof(SHVar));
    return *this;
  }

  ~TTableVar() { SH_CORE::destroyVar(*this); }

  SHVar &operator[](std::string_view key) {
    auto vp = payload.tableValue.api->tableAt(payload.tableValue, key.data());
    return *vp;
  }

  template <typename T> T &get(std::string_view key) {
    static_assert(sizeof(T) == sizeof(SHVar), "Invalid T size, should be sizeof(SHVar)");
    auto vp = payload.tableValue.api->tableAt(payload.tableValue, key.data());
    if (vp->valueType == SHType::None) {
      // try initialize in this case
      new (vp) T();
    }
    return (T &)*vp;
  }
};

template <class SH_CORE> struct TSeqVar : public SHVar {
  TSeqVar() : SHVar() { valueType = SHType::Seq; }

  TSeqVar(const TSeqVar &other) : SHVar() { SH_CORE::cloneVar(*this, other); }

  TSeqVar &operator=(const TSeqVar &other) {
    SH_CORE::cloneVar(*this, other);
    return *this;
  }

  TSeqVar(TSeqVar &&other) : SHVar() { std::swap(*this, other); }

  TSeqVar &operator=(TSeqVar &&other) {
    std::swap(*this, other);
    memset(&other, 0x0, sizeof(SHVar));
    return *this;
  }

  ~TSeqVar() { SH_CORE::destroyVar(*this); }

  SHVar &operator[](int index) { return payload.seqValue.elements[index]; }
  const SHVar &operator[](int index) const { return payload.seqValue.elements[index]; }

  SHVar *data() { return payload.seqValue.len == 0 ? nullptr : &payload.seqValue.elements[0]; }

  size_t size() const { return payload.seqValue.len; }

  void resize(size_t nsize) { SH_CORE::seqResize(&payload.seqValue, nsize); }

  void push_back(const SHVar &value) { SH_CORE::seqPush(&payload.seqValue, &value); }
  void push_back(SHVar &&value) {
    SH_CORE::seqPush(&payload.seqValue, &value);
    value.valueType = SHType::None;
  }

  void clear() { SH_CORE::seqResize(payload.seqValue, 0); }

  template <typename T> T &get(int index) {
    static_assert(sizeof(T) == sizeof(SHVar), "Invalid T size, should be sizeof(SHVar)");
    auto vp = &payload.seqValue.elements[index];
    if (vp->valueType == SHType::None) {
      // try initialize in this case
      new (vp) T();
    }
    return (T &)*vp;
  }
};

// https://godbolt.org/z/I72ctd
template <class Function> struct Defer {
  Function _f;
  Defer(Function &&f) : _f(f) {}
  ~Defer() { _f(); }
};

#define DEFER_NAME(uniq) _defer##uniq
#define DEFER_DEF(uniq, body) ::shards::Defer DEFER_NAME(uniq)([&]() { body; })
#define DEFER(body) DEFER_DEF(__LINE__, body)

template <typename T, size_t N> struct __attribute__((aligned(16))) aligned_array : public std::array<T, N> {};
}; // namespace shards

#endif
