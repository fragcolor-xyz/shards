#include "shards/shards.h"
#include "shards/shards.hpp"
#include <cstdint>
#include <shards/core/module.hpp>
#include <shards/core/foundation.hpp>
#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/utility.hpp>
#include <functional>
#include <optional>
#include <shards/core/async.hpp>
#include <boost/filesystem.hpp>
#include <shared_mutex>
using namespace boost::filesystem;

#pragma clang attribute push(__attribute__((no_sanitize("undefined"))), apply_to = function)
#include "sqlite3.h"
#pragma clang attribute pop

extern "C" {
int sqlite3_crsqlite_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi);
}

namespace shards {
namespace DB {
struct Connection {
  sqlite3 *db;
  std::mutex mutex;
  std::mutex transactionMutex;
  static inline std::shared_mutex globalMutex;

  Connection(const char *path, bool readOnly) {
    std::unique_lock<std::shared_mutex> l(globalMutex); // WRITE LOCK this

    if (readOnly) {
      if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        throw ActivationError(sqlite3_errmsg(db));
      }
    } else {
      if (sqlite3_open(path, &db) != SQLITE_OK) {
        throw ActivationError(sqlite3_errmsg(db));
      }
    }

    uint32_t res;
    if (sqlite3_db_config(db, SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION, 2, &res) != SQLITE_OK) {
      throw ActivationError(sqlite3_errmsg(db));
    }
  }

  Connection(const Connection &) = delete;

  Connection(Connection &&other) {
    db = other.db;
    other.db = nullptr;
  }

  Connection &operator=(const Connection &other) = delete;

  ~Connection() {
    if (db) {
      sqlite3_close(db);
    }
  }

  sqlite3 *get() { return db; }

  void loadExtension(const std::string &path_) {
    if (path_ == "crsqlite") {
      loadCRSQlite();
      return;
    }

    char *errorMsg = nullptr;
    DEFER({
      if (errorMsg)
        sqlite3_free(errorMsg);
    });
    if (sqlite3_load_extension(db, path_.c_str(), nullptr, &errorMsg) != SQLITE_OK) {
      auto exeDirPath = path(GetGlobals().ExePath.c_str()).parent_path();
      auto relPath = exeDirPath / path_;
      auto str = relPath.string();
      if (sqlite3_load_extension(db, str.c_str(), nullptr, &errorMsg) != SQLITE_OK) {
        throw ActivationError(errorMsg);
      }
    }
  }

  void loadCRSQlite() {
    char *errorMsg = nullptr;
    DEFER({
      if (errorMsg)
        sqlite3_free(errorMsg);
    });
    if (sqlite3_crsqlite_init(db, &errorMsg, nullptr) != SQLITE_OK) {
      throw ActivationError(errorMsg);
    }
  }
};

struct Statement {
  sqlite3_stmt *stmt;

  Statement(sqlite3 *db, const char *query) {
    if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) != SQLITE_OK) {
      throw ActivationError(sqlite3_errmsg(db));
    }
  }

  Statement(const Statement &) = delete;

  Statement(Statement &&other) {
    stmt = other.stmt;
    other.stmt = nullptr;
  }

  ~Statement() { sqlite3_finalize(stmt); }

  sqlite3_stmt *get() { return stmt; }
};

struct Base {
  AnyStorage<Connection> _connection;
  OwnedVar _dbNameStr{Var("shards.db")};
  bool ready = false; // mesh is the owner so we don't need cleanup

  bool _withinTransaction{false};

  void compose(SHInstanceData &data) {
    if (data.onWorkerThread)
      throw ComposeError("Can not run on worker thread");

    _withinTransaction = false;
    if (data.shared.len > 0) {
      for (uint32_t i = data.shared.len; i > 0; i--) {
        auto idx = i - 1;
        auto &item = data.shared.elements[idx];
        if (strcmp(item.name, "DB.Transaction.Cookie") == 0) {
          _withinTransaction = true;
          break;
        }
      }
    }
  }

  void _ensureDb(SHContext *context, bool readOnly) {
    if (!ready) {
      auto mesh = context->main->mesh.lock();
      auto dbName = SHSTRVIEW(_dbNameStr);
      if (readOnly) {
        auto storageKey = fmt::format("DB.Connection_{}", dbName);
        _connection = getOrCreateAnyStorage(mesh.get(), storageKey, [&]() { return Connection(dbName.data(), true); });
      } else {
        auto storageKey = fmt::format("DB.RWConnection_{}", dbName);
        _connection = getOrCreateAnyStorage(mesh.get(), storageKey, [&]() { return Connection(dbName.data(), false); });
      }
      ready = true;
    }
  }
};

#define ENSURE_DB(__ctx__, __ro__)   \
  if (_dbName.get() != _dbNameStr) { \
    _dbNameStr = _dbName.get();      \
    ready = false;                   \
  }                                  \
  _ensureDb(__ctx__, __ro__)

struct Query : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::AnySeqType; }
  static SHTypesInfo outputTypes() {
    static Types types{CoreInfo::AnyTableType, CoreInfo::AnySeqType};
    return types;
  }

  void setup() {
    _query = Var("SELECT * FROM test WHERE id = ?");
    _dbName = Var("shards.db");
  }

  PARAM_PARAMVAR(_query, "Query", "The database query to execute every activation.",
                 {CoreInfo::StringType, CoreInfo::StringVarType});
  PARAM_PARAMVAR(_dbName, "Database", "The optional sqlite database filename.",
                 {CoreInfo::NoneType, CoreInfo::StringType, CoreInfo::StringVarType});
  PARAM_VAR(_asRows, "AsRows", "Return the result as rows.", {CoreInfo::NoneType, CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_query), PARAM_IMPL_FOR(_dbName), PARAM_IMPL_FOR(_asRows));

  PARAM_REQUIRED_VARIABLES();

  std::unique_ptr<Statement> prepared;

  struct ColOutput {
    TableVar output;
    std::vector<SeqVar *> columns;
  };

  struct RowOutput {
    SeqVar output;
    SeqVar keys;
  };

  // this is a trick to avoid clearing the output on every activation if empty
  static inline TableVar emptyTableOutput;
  static inline SeqVar emptySeqOutput;

  bool _returnCols{};
  using OutputType = std::variant<std::monostate, ColOutput, RowOutput>;

  OutputType _output[2]; // dual buffered due to awaiting
  size_t _outputCount;

  Var columnToVar(size_t index) {
    auto type = sqlite3_column_type(prepared->get(), index);
    switch (type) {
    case SQLITE_INTEGER:
      return Var((int64_t)sqlite3_column_int64(prepared->get(), index));
    case SQLITE_FLOAT:
      return Var(sqlite3_column_double(prepared->get(), index));
    case SQLITE_TEXT:
      return Var((const char *)sqlite3_column_text(prepared->get(), index));
    case SQLITE_BLOB:
      return Var((const uint8_t *)sqlite3_column_blob(prepared->get(), index),
                 (uint32_t)sqlite3_column_bytes(prepared->get(), index));
    case SQLITE_NULL:
      return Var();
    default:
      throw ActivationError("Unsupported Var type for sqlite");
    }
  }

  SHVar getOutputRows(OutputType &output_) {
    RowOutput *ptr = std::get_if<RowOutput>(&output_);
    if (!ptr)
      ptr = &output_.emplace<RowOutput>();
    RowOutput &output = *ptr;

    output.keys.clear();
    output.output.clear();

    bool empty = true;
    int rc;
    while ((rc = sqlite3_step(prepared->get())) == SQLITE_ROW) {
      auto numCols = sqlite3_column_count(prepared->get());
      if (numCols == 0) {
        continue;
      }

      empty = false;

      // fill column cache on first row
      if (output.keys.empty()) {
        for (int i = 0; i < numCols; i++) {
          auto colName = sqlite3_column_name(prepared->get(), i);
          output.keys.push_back(Var(colName));
        }
      }

      auto &outTable = (TableVar &)output.output.emplace_back_fast();
      if (outTable.valueType != SHType::Table) {
        outTable = TableVar();
      }
      for (auto i = 0; i < numCols; i++) {
        outTable.insert(output.keys[i], columnToVar(i));
      }
    }

    if (rc != SQLITE_DONE) {
      throw ActivationError(sqlite3_errmsg(_connection->get()));
    }

    return empty ? emptySeqOutput : output.output;
  }

  SHVar getOutputCols(OutputType &output_) {
    ColOutput *ptr = std::get_if<ColOutput>(&output_);
    if (!ptr)
      ptr = &output_.emplace<ColOutput>();
    ColOutput &output = *ptr;

    output.columns.clear();
    bool empty = true;
    int rc;
    while ((rc = sqlite3_step(prepared->get())) == SQLITE_ROW) {
      auto numCols = sqlite3_column_count(prepared->get());
      if (numCols == 0) {
        continue;
      }

      empty = false;

      // fill column cache on first row
      if (output.columns.empty()) {
        for (int i = 0; i < numCols; i++) {
          auto colName = sqlite3_column_name(prepared->get(), i);
          auto colNameVar = Var(colName);
          auto &col = output.output[colNameVar];
          if (col.valueType == SHType::None) {
            SeqVar seq;
            output.output.insert(colNameVar, std::move(seq));
            output.columns.push_back(&asSeq(output.output[colNameVar]));
          } else {
            auto &seq = asSeq(col);
            seq.clear();
            output.columns.push_back(&seq);
          }
        }
      }

      for (auto i = 0; i < numCols; i++) {
        auto &col = *output.columns[i];
        col.push_back(columnToVar(i));
      }
    }

    if (rc != SQLITE_DONE) {
      throw ActivationError(sqlite3_errmsg(_connection->get()));
    }

    return empty ? emptyTableOutput : output.output;
  }

  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    Base::compose(data);
    if (!_asRows->isNone() && (bool)*_asRows) {
      _returnCols = false;
      return CoreInfo::SeqOfAnyTableType;
    } else {
      _returnCols = true;
      return CoreInfo::AnyTableType;
    }
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);

    if (_connection) {
      std::scoped_lock<std::mutex> l(_connection->mutex);
      prepared.reset();
    } else {
      assert(!prepared && "prepared should be null if _connection is null");
    }
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (_dbName.get() != _dbNameStr) {
      _dbNameStr = _dbName.get();
      ready = false;
      prepared.reset();
    }
    _ensureDb(context, false);

    // prevent data race on output, as await code might run in parallel with regular mesh!
    OutputType &output = _output[_outputCount++ % 2];

    return awaitne(
        context,
        [&]() -> SHVar {
          // ok if we are not within a transaction, we need to check if transaction lock is locked!
          std::optional<std::unique_lock<std::mutex>> transactionLock;
          if (!_withinTransaction) {
            std::unique_lock<std::mutex> lock(_connection->transactionMutex);
            transactionLock = std::move(lock);
          }
          std::shared_lock<std::shared_mutex> l1(_connection->globalMutex); // READ LOCK this
          std::scoped_lock<std::mutex> l2(_connection->mutex);

          if (!prepared) {
            prepared.reset(
                new Statement(_connection->get(), _query.get().payload.stringValue)); // _query is full terminated cos cloned
          }

          sqlite3_reset(prepared->get());
          sqlite3_clear_bindings(prepared->get());
          int expectedNumParameters = sqlite3_bind_parameter_count(prepared->get());

          int rc;

          int idx = 0;
          for (auto value : input) {
            idx++; // starting from 1 with sqlite
            switch (value.valueType) {
            case SHType::Bool:
              rc = sqlite3_bind_int(prepared->get(), idx, int(value.payload.boolValue));
              break;
            case SHType::Int:
              rc = sqlite3_bind_int64(prepared->get(), idx, value.payload.intValue);
              break;
            case SHType::Float:
              rc = sqlite3_bind_double(prepared->get(), idx, value.payload.floatValue);
              break;
            case SHType::String: {
              auto sv = SHSTRVIEW(value);
              rc = sqlite3_bind_text(prepared->get(), idx, sv.data(), sv.size(), SQLITE_STATIC);
            } break;
            case SHType::Bytes:
              rc = sqlite3_bind_blob(prepared->get(), idx, value.payload.bytesValue, value.payload.bytesSize, SQLITE_STATIC);
              break;
            case SHType::None:
              rc = sqlite3_bind_null(prepared->get(), idx);
              break;
            default:
              throw ActivationError("Unsupported Var type for sqlite");
            }
            if (rc != SQLITE_OK) {
              throw ActivationError(sqlite3_errmsg(_connection->get()));
            }
          }

          if (idx < expectedNumParameters)
            throw ActivationError("Not enough parameters for query");

          return _returnCols ? getOutputCols(output) : getOutputRows(output);
        },
        [] {});
  }
};

struct Transaction : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  SHComposeResult _composeResult{};

  void setup() { _dbName = Var("shards.db"); }

  PARAM(ShardsVar, _queries, "Queries", "The Shards logic executing various DB queries.", {CoreInfo::ShardsOrNone});
  PARAM_PARAMVAR(_dbName, "Database", "The optional sqlite database filename.",
                 {CoreInfo::NoneType, CoreInfo::StringType, CoreInfo::StringVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_queries), PARAM_IMPL_FOR(_dbName));

  PARAM_REQUIRED_VARIABLES();

  static inline SHExposedTypeInfo _cookie{"DB.Transaction.Cookie"};

  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    Base::compose(data);

    // we need to edit a copy of data
    SHInstanceData dataCopy = data;
    // we need to deep copy it
    dataCopy.shared = {};
    DEFER({ arrayFree(dataCopy.shared); });
    for (uint32_t i = data.shared.len; i > 0; i--) {
      auto idx = i - 1;
      auto &item = data.shared.elements[idx];
      arrayPush(dataCopy.shared, item);
    }
    // add our transaction cookie
    arrayPush(dataCopy.shared, _cookie);
    // the cookie will be used within inner queries to ensure they are part of the transaction
    _composeResult = _queries.compose(dataCopy);
    return data.inputType;
  }

  SHExposedTypesInfo exposedVariables() { return _composeResult.exposedInfo; }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    ENSURE_DB(context, false);

    // avoid transaction nesting
    std::unique_lock<std::mutex> lock(_connection->transactionMutex, std::defer_lock);
    // try to lock, if we can't we suspend
    while (!lock.try_lock()) {
      SH_SUSPEND(context, 0);
    }

    await(
        context,
        [&] {
          std::shared_lock<std::shared_mutex> l1(_connection->globalMutex); // READ LOCK this
          std::scoped_lock<std::mutex> l2(_connection->mutex);

          auto rc = sqlite3_exec(_connection->get(), "BEGIN;", nullptr, nullptr, nullptr);
          if (rc != SQLITE_OK) {
            throw ActivationError(sqlite3_errmsg(_connection->get()));
          }
        },
        []() {});

    SHVar output{};
    auto state = _queries.activate(context, input, output);

    await(
        context,
        [&] {
          std::shared_lock<std::shared_mutex> l1(_connection->globalMutex); // READ LOCK this
          std::scoped_lock<std::mutex> l2(_connection->mutex);

          if (state != SHWireState::Continue) {
            // likely something went wrong! lets rollback.
            auto rc = sqlite3_exec(_connection->get(), "ROLLBACK;", nullptr, nullptr, nullptr);
            if (rc != SQLITE_OK) {
              throw ActivationError(sqlite3_errmsg(_connection->get()));
            }
          } else {
            // commit
            auto rc = sqlite3_exec(_connection->get(), "COMMIT;", nullptr, nullptr, nullptr);
            if (rc != SQLITE_OK) {
              throw ActivationError(sqlite3_errmsg(_connection->get()));
            }
          }
        },
        []() {});

    return input;
  }
};

struct LoadExtension : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  void setup() {
    _extPath = Var("my-extension");
    _dbName = Var("shards.db");
  }

  PARAM_PARAMVAR(_extPath, "Path", "The path to the extension to load.", {CoreInfo::StringType, CoreInfo::StringVarType});
  PARAM_PARAMVAR(_dbName, "Database", "The optional sqlite database filename.",
                 {CoreInfo::NoneType, CoreInfo::StringType, CoreInfo::StringVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_extPath), PARAM_IMPL_FOR(_dbName));

  PARAM_REQUIRED_VARIABLES();

  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    Base::compose(data);
    return data.inputType;
  }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    ENSURE_DB(context, false);

    return awaitne(
        context,
        [&] {
          std::optional<std::unique_lock<std::mutex>> transactionLock;
          if (!_withinTransaction) {
            std::unique_lock<std::mutex> lock(_connection->transactionMutex);
            transactionLock = std::move(lock);
          }
          std::shared_lock<std::shared_mutex> l1(_connection->globalMutex); // READ LOCK this
          std::scoped_lock<std::mutex> l2(_connection->mutex);

          std::string extPath(_extPath.get().payload.stringValue, _extPath.get().payload.stringLen);
          _connection->loadExtension(extPath);
          return input;
        },
        [] {});
  }
};

struct RawQuery : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  void setup() {
    _dbName = Var("shards.db");
    _readOnly = Var(false);
  }

  PARAM_PARAMVAR(_dbName, "Database", "The optional sqlite database filename.",
                 {CoreInfo::NoneType, CoreInfo::StringType, CoreInfo::StringVarType});
  PARAM_PARAMVAR(_readOnly, "ReadOnly", "If true, the database will be opened in read only mode.",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_dbName), PARAM_IMPL_FOR(_readOnly));

  PARAM_REQUIRED_VARIABLES();

  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    Base::compose(data);
    return outputTypes().elements[0];
  }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    ENSURE_DB(context, _readOnly.get().payload.boolValue);

    return awaitne(
        context,
        [&] {
          std::optional<std::unique_lock<std::mutex>> transactionLock;
          if (!_withinTransaction) {
            std::unique_lock<std::mutex> lock(_connection->transactionMutex);
            transactionLock = std::move(lock);
          }
          std::shared_lock<std::shared_mutex> l1(_connection->globalMutex); // READ LOCK this
          std::scoped_lock<std::mutex> l2(_connection->mutex);

          char *errMsg = nullptr;
          std::string query(input.payload.stringValue, input.payload.stringLen); // we need to make sure we are 0 terminated
          int rc = sqlite3_exec(_connection->get(), query.c_str(), nullptr, nullptr, &errMsg);

          if (rc != SQLITE_OK) {
            throw ActivationError(errMsg);
            sqlite3_free(errMsg);
          }

          return input;
        },
        [] {});
  }
};

struct Backup : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  void setup() {
    _dest = Var("backup.db");
    _dbName = Var("shards.db");
  }

  PARAM_PARAMVAR(_dest, "Destination", "The destination database filename.", {CoreInfo::StringType, CoreInfo::StringVarType});
  PARAM_PARAMVAR(_dbName, "Database", "The optional sqlite database filename.",
                 {CoreInfo::NoneType, CoreInfo::StringType, CoreInfo::StringVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_dest), PARAM_IMPL_FOR(_dbName));

  PARAM_REQUIRED_VARIABLES();

  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    Base::compose(data);
    return outputTypes().elements[0];
  }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    ENSURE_DB(context, true);

    return awaitne(
        context,
        [&] {
          std::optional<std::unique_lock<std::mutex>> transactionLock;
          if (!_withinTransaction) {
            std::unique_lock<std::mutex> lock(_connection->transactionMutex);
            transactionLock = std::move(lock);
          }
          std::shared_lock<std::shared_mutex> l1(_connection->globalMutex); // READ LOCK this
          std::unique_lock<std::mutex> l2(_connection->mutex);

          auto destPath = SHSTRVIEW(_dest.get());
          sqlite3 *pDest;
          auto rc = sqlite3_open(destPath.data(), &pDest);
          if (rc != SQLITE_OK) {
            throw ActivationError(sqlite3_errmsg(pDest));
          }
          DEFER({ sqlite3_close(pDest); });

          auto pBackup = sqlite3_backup_init(pDest, "main", _connection->get(), "main");
          if (!pBackup) {
            throw ActivationError(sqlite3_errmsg(pDest));
          }
          DEFER({ sqlite3_backup_finish(pBackup); });

          // do 5 pages, unlock, yield a bit, repeat
          do {
            rc = sqlite3_backup_step(pBackup, 200);
            if (rc == SQLITE_OK || rc == SQLITE_BUSY || rc == SQLITE_LOCKED) {
              // unlock
              l2.unlock();
              // thread wait here 250 ms
              std::this_thread::sleep_for(std::chrono::milliseconds(100));
              // lock again
              l2.lock();
            }
          } while (rc == SQLITE_OK || rc == SQLITE_BUSY || rc == SQLITE_LOCKED);

          return input;
        },
        [] {});
  }
};

} // namespace DB
} // namespace shards
SHARDS_REGISTER_FN(sqlite) {
  using namespace shards::DB;
  REGISTER_SHARD("DB.Query", Query);
  REGISTER_SHARD("DB.Transaction", Transaction);
  REGISTER_SHARD("DB.LoadExtension", LoadExtension);
  REGISTER_SHARD("DB.RawQuery", RawQuery);
  REGISTER_SHARD("DB.Backup", Backup);
}
