#ifndef CB_LMDB_HPP
#define CB_LMDB_HPP

#include "shared.hpp"
#include <filesystem>
#include <lmdb.h>

namespace chainblocks {
namespace LMDB {

constexpr auto maxReaders = 1024;

struct Env {
#define CHECKED(__call__)                                                      \
  {                                                                            \
    auto err = __call__;                                                       \
    if (err != 0) {                                                            \
      LOG(ERROR) << "LMDB error: " << mdb_strerror(err);                       \
      throw CBException("LMDB error detected.");                               \
    }                                                                          \
  }

  void init() {
    if (_init)
      return;
    _init = true;

    CHECKED(mdb_env_create(&_env));

    CHECKED(mdb_env_set_maxreaders(_env, maxReaders));

    unsigned int flags = MDB_NOTLS | MDB_NOSYNC | MDB_NOSUBDIR;

    std::filesystem::path dbPath("cbdb.bin");
    if (Globals::RootPath.size() > 0) {
      std::filesystem::path cbpath(Globals::RootPath);
      auto abspath = std::filesystem::absolute(cbpath / dbPath);
      abspath.make_preferred();
      CHECKED(mdb_env_open(_env, abspath.string().c_str(), flags, 0664));
    } else {
      auto abspath =
          std::filesystem::absolute(std::filesystem::current_path() / dbPath);
      abspath.make_preferred();
      CHECKED(mdb_env_open(_env, abspath.string().c_str(), flags, 0664));
    }

    // we sync every global "sleep" only!
    registerRunLoopCallback("cb.lmdb.sync", []() {
      static const Env &env{Singleton<Env>::value};
      CHECKED(mdb_env_sync(env._env, 1));
    });
  }

  operator MDB_env *() { return _env; }

private:
  bool _init = false;
  MDB_env *_env;
};

struct Base {
  CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  Env &env{Singleton<Env>::value};

  void setup() { env.init(); };

  CBParametersInfo parameters() { return _params; }

  void setParam(int index, CBVar value) { _key = value; }

  CBVar getParam(int index) { return _key; }

  void warmup(CBContext *ctx) { _key.warmup(ctx); }

  void cleanup() { _key.cleanup(); }

protected:
  ParamVar _key;
  static inline Parameters _params{
      {"Key",
       "The key to get from the database.",
       {CoreInfo::StringType, CoreInfo::StringVarType}}};
};

struct Get : public Base {
  CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  void cleanup() {
    if (_txn) {
      mdb_txn_abort(_txn);
      _txn = nullptr;
      ;
    }
    Base::cleanup();
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    const CBVar &cbkey = _key.get();

    // We open a transaction and keep it open until we get called again
    // this allows us to have no need at all for a copy
    // at the cost of a read lock which should be cheap!
    if (_txn) {
      mdb_txn_reset(_txn);
      CHECKED(mdb_txn_renew(_txn));
    } else {
      CHECKED(mdb_txn_begin(env, nullptr, MDB_RDONLY, &_txn));
    }

    MDB_dbi dbi;
    CHECKED(mdb_dbi_open(_txn, nullptr, 0, &dbi));

    MDB_val key{cbkey.payload.stringLen > 0 ? cbkey.payload.stringLen
                                            : strlen(cbkey.payload.stringValue),
                (void *)cbkey.payload.stringValue};

    MDB_val val;
    CHECKED(mdb_get(_txn, dbi, &key, &val));

    assert(val.mv_size >= sizeof(CBVar));

    CBVar res;
    memcpy(&res, val.mv_data, sizeof(CBVar));

    // Fix non blittable types
    if (res.valueType > CBType::EndOfBlittableTypes) {
      switch (res.valueType) {
      case CBType::String: {
        res.payload.stringValue =
            (CBString)((uint8_t *)val.mv_data + sizeof(CBVar));
      } break;
      case CBType::Seq: {
        res.payload.seqValue.elements =
            (CBVar *)((uint8_t *)val.mv_data + sizeof(CBVar));
      } break;
      default: {
        throw CBException("Case not handled and variable is not blittable!");
      }
      }
    }

    return res;
  }

private:
  MDB_txn *_txn = nullptr;
};

template <unsigned int PUT_FLAGS> struct PutBase : public Base {
  // TODO compose: avoid non flat sequences, figure what to do with tables
  // etc
  CBVar activate(CBContext *context, const CBVar &input) {
    const CBVar &cbkey = _key.get();

    MDB_txn *txn;
    CHECKED(mdb_txn_begin(env, nullptr, 0, &txn));

    MDB_dbi dbi;
    CHECKED(mdb_dbi_open(txn, nullptr, 0, &dbi));

    MDB_val key{cbkey.payload.stringLen > 0 ? cbkey.payload.stringLen
                                            : strlen(cbkey.payload.stringValue),
                (void *)cbkey.payload.stringValue};

    if (input.valueType < CBType::EndOfBlittableTypes) {
      // just a regular 32 bytes push
      MDB_val val{sizeof(CBVar), (void *)&input};
      CHECKED(mdb_put(txn, dbi, &key, &val, PUT_FLAGS));
    } else {
      // use MDD_RESERVE to get 32 + data len allocated
      // and directly write to it
      switch (input.valueType) {
      case CBType::String: {
        size_t len = input.payload.stringLen > 0
                         ? input.payload.stringLen
                         : strlen(input.payload.stringValue);
        // make sure to add a 0 terminator!
        size_t size = sizeof(CBVar) + len + 1;
        MDB_val extraVal{size, NULL};
        CHECKED(mdb_put(txn, dbi, &key, &extraVal, PUT_FLAGS | MDB_RESERVE));
        memcpy((uint8_t *)extraVal.mv_data, &input, sizeof(CBVar));
        memcpy((uint8_t *)extraVal.mv_data + sizeof(CBVar),
               (void *)input.payload.stringValue, len);
        ((uint8_t *)extraVal.mv_data + sizeof(CBVar))[len] = 0;
      } break;
      case CBType::Seq: {
        size_t arraySize = input.payload.seqValue.len * sizeof(CBVar);
        size_t size = sizeof(CBVar) + arraySize;
        MDB_val extraVal{size, NULL};
        CHECKED(mdb_put(txn, dbi, &key, &extraVal, PUT_FLAGS | MDB_RESERVE));
        memcpy((uint8_t *)extraVal.mv_data, &input, sizeof(CBVar));
        memcpy((uint8_t *)extraVal.mv_data + sizeof(CBVar),
               (void *)&input.payload.seqValue.elements[0], arraySize);
      } break;
      default: {
        throw CBException("Case not handled and variable is not blittable!");
      }
      }
    }

    CHECKED(mdb_txn_commit(txn));

    return input;
  }
};

using Set = PutBase<MDB_NOOVERWRITE>;
using Update = PutBase<0>;

struct Iterate : public Base {};

struct Delete : public Base {};

void registerBlocks() {
  REGISTER_CBLOCK("DB.Get", Get);
  REGISTER_CBLOCK("DB.Set", Set);
  REGISTER_CBLOCK("DB.Update", Update);
}
} // namespace LMDB
} // namespace chainblocks

#endif /* CB_LMDB_HPP */
