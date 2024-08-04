#include "shards/shards.h"
#include "shards/shards.hpp"
#include <shards/log/log.hpp>
#include <cstdint>
#include <shards/core/platform.hpp>
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
using namespace std::chrono_literals;

#ifdef __clang__
#pragma clang attribute push(__attribute__((no_sanitize("undefined"))), apply_to = function)
#endif

#include "sqlite3.h"

#ifdef __clang__
#pragma clang attribute pop
#endif

extern "C" {
int sqlite3_crsqlite_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi);
}

static const char *vfs = //
#if SH_EMSCRIPTEN
    "shards-memory-locked";
#else
    nullptr;
#endif

#ifndef SH_SQLITE_DEBUG_LOGS
#define SH_SQLITE_DEBUG_LOGS 0
#endif
#if SH_SQLITE_DEBUG_LOGS
#define SH_SQLITE_DEBUG_LOG(...) SPDLOG_LOGGER_DEBUG(logger, __VA_ARGS__)
#else
#define SH_SQLITE_DEBUG_LOG(...)
#endif

static auto logger = shards::logging::getOrCreate("sqlite");

namespace shards {
namespace DB {

#define SQLITE_NO_LOCK 0
#define SQLITE_SHARED_LOCK 1
#define SQLITE_RESERVED_LOCK 2
#define SQLITE_PENDING_LOCK 3
#define SQLITE_EXCLUSIVE_LOCK 4

struct MemoryLockedVfs : sqlite3_vfs {
  static inline const char *backendVfs = //
#if SH_EMSCRIPTEN
      "unix";
#else
      nullptr;
#endif

  sqlite3_vfs *fallback{};
  sqlite3_io_methods ioMethods{};
  const sqlite3_io_methods *fallbackIoMethods{};

  enum Lock { None, Shared, Unique };
  // These are shared betweent threads and the mutex is held possibly by multiple
  struct SharedFileNode {
    std::shared_timed_mutex mutex;
    std::atomic_int refCount = 0;
  };
  // These are only controlled from the same thread an keep the lock state owned by this node
  struct UniqueFileNode {
    std::shared_ptr<SharedFileNode> shared;
    Lock currentLock{};
  };
  std::shared_mutex setMutex;
  std::unordered_map<std::string, std::shared_ptr<SharedFileNode>> fileNodes;
  std::unordered_map<sqlite3_file *, std::shared_ptr<UniqueFileNode>> fileNodeMap;

  void initIoMethods(const sqlite3_io_methods *base) {
    memcpy(&ioMethods, base, sizeof(sqlite3_io_methods));
    fallbackIoMethods = base;
    ioMethods.xClose = [](sqlite3_file *pFile) -> int {
      auto &self = MemoryLockedVfs::instance();
      return self.closeFile(pFile);
    };
    ioMethods.xLock = [](sqlite3_file *pFile, int lockType) -> int {
      auto &self = MemoryLockedVfs::instance();
      return self.lockFile(pFile, lockType);
    };
    ioMethods.xUnlock = [](sqlite3_file *pFile, int lockType) -> int {
      auto &self = MemoryLockedVfs::instance();
      return self.unlockFile(pFile, lockType);
    };
    ioMethods.xCheckReservedLock = [](sqlite3_file *pFile, int *pResOut) -> int {
      *pResOut = 0;
      return SQLITE_OK;
    };
    ioMethods.xShmBarrier = nullptr;
    ioMethods.xShmUnmap = nullptr;
    ioMethods.xShmLock = nullptr;
    ioMethods.xShmMap = nullptr;
  }

  int lockFile(sqlite3_file *pFile, int lockType) {
    std::shared_lock<std::shared_mutex> l(setMutex);
    auto node = fileNodeMap[pFile];
    l.unlock();

    if (lockType >= SQLITE_PENDING_LOCK) {
      if (node->currentLock == Lock::None) {
        if (!node->shared->mutex.try_lock_for(10ms))
          return SQLITE_BUSY;
      } else {
        if (node->currentLock != Lock::Unique) {
          if (node->currentLock == Lock::Shared) {
            node->shared->mutex.unlock_shared();
            node->currentLock = Lock::None;
          }
        }
        // This might leave lock in a different state than before, but maybe it's fine?
        if (!node->shared->mutex.try_lock_for(100ms))
          return SQLITE_BUSY;
      }
      node->currentLock = Lock::Unique;
    } else {
      if (node->currentLock == Lock::None) {
        if (!node->shared->mutex.try_lock_shared_for(10ms))
          return SQLITE_BUSY;
        node->currentLock = Lock::Shared;
      }
    }
    return SQLITE_OK;
  }

  int unlockFile(sqlite3_file *pFile, int lockType) {
    std::shared_lock<std::shared_mutex> l(setMutex);
    auto node = fileNodeMap[pFile];
    l.unlock();

    bool wantSharedLock = lockType > SQLITE_NO_LOCK && lockType < SQLITE_PENDING_LOCK;
    if (wantSharedLock && node->currentLock == Lock::Shared) {
      return SQLITE_OK;
    }

    if (node->currentLock == Lock::Unique) {
      node->shared->mutex.unlock();
      node->currentLock = Lock::None;
    } else if (node->currentLock == Lock::Shared) {
      node->shared->mutex.unlock_shared();
      node->currentLock = Lock::None;
    }

    if (wantSharedLock) {
      // Happens in case of downgrading
      // This might leave lock in a different state than before, but maybe it's fine?
      if (!node->shared->mutex.try_lock_shared_for(100ms))
        return SQLITE_BUSY;
      node->currentLock = Lock::Shared;
    }

    return SQLITE_OK;
  }

  int closeFile(sqlite3_file *pFile) {
    std::unique_lock<std::shared_mutex> l(setMutex);
    auto node = fileNodeMap[pFile];
    node->shared->refCount--;
    fileNodeMap.erase(pFile);
    return fallbackIoMethods->xClose(pFile);
  }

  int openFile(sqlite3_vfs *pVfs, const char *zName, sqlite3_file *pFile, int flags, int *pOutFlags) {
    std::unique_lock<std::shared_mutex> l(setMutex);
    int result = fallback->xOpen(fallback, zName, pFile, flags, pOutFlags);
    if (result == SQLITE_OK) {
      if (zName == nullptr)
        return result; // Ignore open on nullptr
      if (!ioMethods.xFileSize)
        initIoMethods(pFile->pMethods);
      pFile->pMethods = &ioMethods;

      auto normalizedPath = boost::filesystem::absolute(boost::filesystem::path(zName)).normalize().string();
      auto it = fileNodes.find(normalizedPath);
      auto uniqueNode = std::make_shared<UniqueFileNode>();
      std::shared_ptr<SharedFileNode> node;
      if (it == fileNodes.end()) {
        node = std::make_shared<SharedFileNode>();
        fileNodes[normalizedPath] = node;
        uniqueNode->shared = node;
      } else {
        node = it->second;
      }
      node->refCount++;
      uniqueNode->shared = node;
      fileNodeMap[pFile] = uniqueNode;
    }
    return result;
  }

  MemoryLockedVfs() {
    sqlite3_vfs_register(this, 0);
    fallback = sqlite3_vfs_find(backendVfs);
    memcpy(this, fallback, sizeof(sqlite3_vfs));
    this->zName = "shards-memory-locked";

    xOpen = [](sqlite3_vfs *pVfs, const char *zName, sqlite3_file *pFile, int flags, int *pOutFlags) -> int {
      auto &self = *(MemoryLockedVfs *)pVfs;
      return self.openFile(pVfs, zName, pFile, flags, pOutFlags);
    };
  }
  static MemoryLockedVfs &instance() {
    static MemoryLockedVfs vfs;
    return vfs;
  }
};

struct Connection {
  sqlite3 *db{};
  std::mutex mutex;
  std::mutex transactionMutex;
  static inline std::shared_mutex globalMutex;

  Connection(const char *path, bool readOnly) {
    std::unique_lock<std::shared_mutex> l(globalMutex); // WRITE LOCK this

    if (readOnly) {
      SH_SQLITE_DEBUG_LOG(logger, "sqlite open read-only {}", path);
      if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY, vfs) != SQLITE_OK) {
        throw ActivationError(sqlite3_errmsg(db));
      }
    } else {
      SH_SQLITE_DEBUG_LOG(logger, "sqlite open read-write {}", path);
      if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, vfs) != SQLITE_OK) {
        throw ActivationError(sqlite3_errmsg(db));
      }
    }
    SH_SQLITE_DEBUG_LOG(logger, "sqlite opened {}", (void *)db);

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
      SH_SQLITE_DEBUG_LOG(logger, "sqlite close {}", (void *)db);
      std::unique_lock<std::shared_mutex> l(globalMutex); // WRITE LOCK this
      sqlite3_close(db);
    }
  }

  sqlite3 *get() { return db; }

  void loadExtension(const std::string &path_, const std::optional<std::string> &entryPoint) {
    if (path_ == "crsqlite") {
      loadCRSQlite();
      return;
    }

    char *errorMsg = nullptr;
    DEFER({
      if (errorMsg)
        sqlite3_free(errorMsg);
    });

    const char *entryPointStr = entryPoint.has_value() ? entryPoint.value().c_str() : nullptr;

    if (sqlite3_load_extension(db, path_.c_str(), entryPointStr, &errorMsg) != SQLITE_OK) {
      auto exeDirPath = path(GetGlobals().ExePath.c_str()).parent_path();
      auto relPath = exeDirPath / path_;
      auto str = relPath.string();
      if (sqlite3_load_extension(db, str.c_str(), entryPointStr, &errorMsg) != SQLITE_OK) {
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
    try {
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
    } catch (std::exception &e) {
      throw ActivationError(fmt::format("Failed to connect: {}", e.what()));
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
    _asRows = Var(false);
    _retry = Var(false);
  }

  PARAM_PARAMVAR(_query, "Query", "The database query to execute every activation.",
                 {CoreInfo::StringType, CoreInfo::StringVarType});
  PARAM_PARAMVAR(_dbName, "Database", "The optional sqlite database filename.",
                 {CoreInfo::NoneType, CoreInfo::StringType, CoreInfo::StringVarType});
  PARAM_VAR(_asRows, "AsRows", "Return the result as rows.", {CoreInfo::BoolType});
  PARAM_VAR(_retry, "Retry", "Retry the query if the database was locked.", {CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_query), PARAM_IMPL_FOR(_dbName), PARAM_IMPL_FOR(_asRows), PARAM_IMPL_FOR(_retry));

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
    case SQLITE_TEXT: {
      int text_len = sqlite3_column_bytes(prepared->get(), index);
      return Var((const char *)sqlite3_column_text(prepared->get(), index), text_len);
    }
    case SQLITE_BLOB:
      return Var((const uint8_t *)sqlite3_column_blob(prepared->get(), index),
                 (uint32_t)sqlite3_column_bytes(prepared->get(), index));
    case SQLITE_NULL:
      return Var();
    default:
      throw ActivationError("Unsupported Var type for sqlite");
    }
  }

  SHVar getOutputRows(SHContext *context, OutputType &output_) {
    RowOutput *ptr = std::get_if<RowOutput>(&output_);
    if (!ptr)
      ptr = &output_.emplace<RowOutput>();
    RowOutput &output = *ptr;

    output.keys.clear();
    output.output.clear();

    bool empty = true;
    int rc;
    bool retry = _retry.payload.boolValue;
    do {
      rc = sqlite3_step(prepared->get());
      if (rc == SQLITE_ROW) {
        auto numCols = sqlite3_column_count(prepared->get());
        if (numCols == 0) {
          continue;
        }

        empty = false;

        // fill column cache on first row
        if (output.keys.empty()) {
          for (int i = 0; i < numCols; i++) {
            auto colName = sqlite3_column_name(prepared->get(), i);
            output.keys.push_back(Var(colName, 0)); // we need to strlen, so we pass 0 explicit, sqlite has no way to know ahead
          }
        }

        auto &outTable = (TableVar &)output.output.emplace_back_fast();
        if (outTable.valueType != SHType::Table) {
          outTable = TableVar();
        }
        for (auto i = 0; i < numCols; i++) {
          outTable.insert(output.keys[i], columnToVar(i));
        }
      } else if (rc == SQLITE_BUSY) {
        if (retry) {
          // Try again after yield or next frame
          shards::suspend(context, 0.0, true);
        } else {
          throw ActivationError("SQLite database is busy and retry is disabled");
        }
      }
    } while (rc == SQLITE_ROW || (rc == SQLITE_BUSY && retry));

    if (rc != SQLITE_DONE) {
      throw ActivationError(sqlite3_errmsg(_connection->get()));
    }

    return empty ? emptySeqOutput : output.output;
  }

  SHVar getOutputCols(SHContext *context, OutputType &output_) {
    ColOutput *ptr = std::get_if<ColOutput>(&output_);
    if (!ptr)
      ptr = &output_.emplace<ColOutput>();
    ColOutput &output = *ptr;

    output.columns.clear();
    bool empty = true;
    int rc;
    bool retry = _retry.payload.boolValue;
    do {
      rc = sqlite3_step(prepared->get());
      if (rc == SQLITE_ROW) {
        auto numCols = sqlite3_column_count(prepared->get());
        if (numCols == 0) {
          continue;
        }

        empty = false;

        // fill column cache on first row
        if (output.columns.empty()) {
          for (int i = 0; i < numCols; i++) {
            auto colName = sqlite3_column_name(prepared->get(), i);
            auto colNameVar = Var(colName, 0); // pass 0 to call strlen as sqlite has no way to know ahead
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
      } else if (rc == SQLITE_BUSY) {
        if (retry) {
          // Try again after yield or next frame
          shards::suspend(context, 0.0, true);
        } else {
          throw ActivationError("SQLite database is busy and retry is disabled");
        }
      }
    } while (rc == SQLITE_ROW || (rc == SQLITE_BUSY && retry));

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

    return maybeAwaitne(
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

          SH_SQLITE_DEBUG_LOG(logger, "sqlite query, db: {}, {}", (void *)_connection->db, _query.get().payload.stringValue);
          return _returnCols ? getOutputCols(context, output) : getOutputRows(context, output);
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

    maybeAwait(
        context,
        [&] {
          std::shared_lock<std::shared_mutex> l1(_connection->globalMutex); // READ LOCK this
          std::scoped_lock<std::mutex> l2(_connection->mutex);

          SH_SQLITE_DEBUG_LOG(logger, "Transaction begin, db: {}", (void *)_connection->db);
          auto rc = sqlite3_exec(_connection->get(), "BEGIN;", nullptr, nullptr, nullptr);
          if (rc != SQLITE_OK) {
            throw ActivationError(sqlite3_errmsg(_connection->get()));
          }
        },
        []() {});

    SHVar output{};
    auto state = _queries.activate(context, input, output);

    maybeAwait(
        context,
        [&] {
          std::shared_lock<std::shared_mutex> l1(_connection->globalMutex); // READ LOCK this
          std::scoped_lock<std::mutex> l2(_connection->mutex);

          if (state != SHWireState::Continue) {
            // likely something went wrong! lets rollback.
            SH_SQLITE_DEBUG_LOG(logger, "Transaction rollback, db: {}", (void *)_connection->db);
            auto rc = sqlite3_exec(_connection->get(), "ROLLBACK;", nullptr, nullptr, nullptr);
            if (rc != SQLITE_OK) {
              throw ActivationError(sqlite3_errmsg(_connection->get()));
            }
          } else {
            // commit
            SH_SQLITE_DEBUG_LOG(logger, "Transaction commit, db: {}", (void *)_connection->db);
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

  LoadExtension() {
    _extPath = Var("my-extension");
    _dbName = Var("shards.db");
  }

  PARAM_PARAMVAR(_extPath, "Path", "The path to the extension to load.", {CoreInfo::StringType, CoreInfo::StringVarType});
  PARAM_PARAMVAR(_dbName, "Database", "The optional sqlite database filename.",
                 {CoreInfo::NoneType, CoreInfo::StringType, CoreInfo::StringVarType});
  PARAM_PARAMVAR(_entryPoint, "EntryPoint", "The entry point of the extension.",
                 {CoreInfo::StringType, CoreInfo::StringVarType, CoreInfo::NoneType});
  PARAM_IMPL(PARAM_IMPL_FOR(_extPath), PARAM_IMPL_FOR(_dbName), PARAM_IMPL_FOR(_entryPoint));

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

          auto &entryPoint = _entryPoint.get();
          if (entryPoint.valueType == SHType::None) {
            _connection->loadExtension(extPath, std::nullopt);
          } else {
            auto entryPointStr = SHSTRING_PREFER_SHSTRVIEW(entryPoint);
            _connection->loadExtension(extPath, entryPointStr);
          }

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

    return maybeAwaitne(
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
          SH_SQLITE_DEBUG_LOG(logger, "Raw query db: {}, {}", (void *)_connection->db, query);
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
    _fast = Var(true);
    _pages = Var(200);
  }

  PARAM_PARAMVAR(_dest, "Destination", "The destination database filename.", {CoreInfo::StringType, CoreInfo::StringVarType});
  PARAM_PARAMVAR(_dbName, "Database", "The optional sqlite database filename.",
                 {CoreInfo::NoneType, CoreInfo::StringType, CoreInfo::StringVarType});
  PARAM_PARAMVAR(_pages, "Pages", "The number of pages to copy at once.", {CoreInfo::IntType, CoreInfo::IntVarType});
  PARAM_VAR(_fast, "Unthrottled", "If true, the backup will not be throttled and it might lock the DB while copying.",
            {CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_dest), PARAM_IMPL_FOR(_dbName), PARAM_IMPL_FOR(_pages), PARAM_IMPL_FOR(_fast));

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

    return maybeAwaitne(
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
          auto rc = sqlite3_open_v2(destPath.data(), &pDest, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, vfs);
          if (rc != SQLITE_OK) {
            throw ActivationError(sqlite3_errmsg(pDest));
          }
          DEFER({ sqlite3_close(pDest); });

          auto pBackup = sqlite3_backup_init(pDest, "main", _connection->get(), "main");
          if (!pBackup) {
            throw ActivationError(sqlite3_errmsg(pDest));
          }
          DEFER({
            auto rc = sqlite3_backup_finish(pBackup);
            if (rc != SQLITE_OK) {
              throw ActivationError(sqlite3_errmsg(pDest));
            }
          });

          int pages = (int)_pages.get().payload.intValue;

          // do 200 pages, unlock, yield a bit, repeat
          do {
            rc = sqlite3_backup_step(pBackup, pages);
            if (!_fast.payload.boolValue && (rc == SQLITE_OK || rc == SQLITE_BUSY || rc == SQLITE_LOCKED)) {
              // unlock
              l2.unlock();
              // thread wait here 100 ms
              std::this_thread::sleep_for(std::chrono::milliseconds(100));
              // lock again
              l2.lock();
            }
            // check for errors!
            if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_BUSY && rc != SQLITE_LOCKED) {
              throw ActivationError(sqlite3_errmsg(pDest));
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
  MemoryLockedVfs::instance();
  REGISTER_SHARD("DB.Query", Query);
  REGISTER_SHARD("DB.Transaction", Transaction);
  REGISTER_SHARD("DB.LoadExtension", LoadExtension);
  REGISTER_SHARD("DB.RawQuery", RawQuery);
  REGISTER_SHARD("DB.Backup", Backup);
}
