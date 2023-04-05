#include "shared.hpp"
#include "params.hpp"
#include "utility.hpp"
#include <optional>
#include <sqlite3.h>

namespace shards {
namespace DB {
struct Connection {
  sqlite3 *db;

  Connection(const char *path) {
    if (sqlite3_open(path, &db) != SQLITE_OK) {
      throw ActivationError(sqlite3_errmsg(db));
    }
  }

  ~Connection() { sqlite3_close(db); }

  sqlite3 *get() { return db; }
};

struct Statement {
  sqlite3_stmt *stmt;

  Statement(sqlite3 *db, const char *query) {
    if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) != SQLITE_OK) {
      throw ActivationError(sqlite3_errmsg(db));
    }
  }

  ~Statement() { sqlite3_finalize(stmt); }

  sqlite3_stmt *get() { return stmt; }
};

struct Base {
  std::shared_ptr<Connection> _connection;

  void warmup(SHContext *context) {
    auto &dbCtx = context->anyStorage["DBConnection"];
    if (dbCtx.type() != entt::type_id<std::shared_ptr<Connection>>()) {
      // create a new context
      dbCtx = entt::any{std::in_place_type_t<std::shared_ptr<Connection>>{}};
    }
    auto &ctx = entt::any_cast<std::shared_ptr<Connection> &>(dbCtx);
    if (!ctx) {
      ctx.reset(new Connection("shards.db"));
    }
    _connection = ctx;
  }

  void cleanup() { _connection.reset(); }
};

struct Query : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::AnySeqType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyTableType; }

  PARAM_VAR(_query, "Query", "The database query to execute every activation.", {CoreInfo::StringType});
  PARAM_IMPL(Query, PARAM_IMPL_FOR(_query));

  void cleanup() {
    PARAM_CLEANUP();

    prepared.reset();

    Base::cleanup();
  }

  std::unique_ptr<Statement> prepared;

  void warmup(SHContext *context) {
    Base::warmup(context);

    PARAM_WARMUP(context);
  }

  TableVar output;
  std::vector<SeqVar *> colSeqs;

  SHVar activate(SHContext *context, const SHVar &input) {
    if (!prepared) {
      prepared.reset(new Statement(_connection->get(), _query.payload.stringValue));
    }

    sqlite3_reset(prepared->get());
    sqlite3_clear_bindings(prepared->get());

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
      case SHType::String:
        rc = sqlite3_bind_text(prepared->get(), idx, value.payload.stringValue, value.payload.stringLen, SQLITE_STATIC);
        break;
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

    colSeqs.clear();
    while ((rc = sqlite3_step(prepared->get())) == SQLITE_ROW) {
      auto count = sqlite3_column_count(prepared->get());
      // fill column cache on first row
      if (colSeqs.empty()) {
        for (int i = 0; i < count; i++) {
          auto colName = sqlite3_column_name(prepared->get(), i);
          auto &col = output[colName];
          if (col.valueType == SHType::None) {
            SeqVar seq;
            output.insert(colName, std::move(seq));
            colSeqs.push_back(&asSeq(output[colName]));
          } else {
            auto &seq = asSeq(col);
            seq.clear();
            colSeqs.push_back(&seq);
          }
        }
      }

      for (auto i = 0; i < count; i++) {
        auto &col = *colSeqs[i];
        auto type = sqlite3_column_type(prepared->get(), i);
        switch (type) {
        case SQLITE_INTEGER:
          col.push_back(Var((int64_t)sqlite3_column_int64(prepared->get(), i)));
          break;
        case SQLITE_FLOAT:
          col.push_back(Var(sqlite3_column_double(prepared->get(), i)));
          break;
        case SQLITE_TEXT:
          col.push_back(Var((const char *)sqlite3_column_text(prepared->get(), i)));
          break;
        case SQLITE_BLOB:
          col.push_back(
              Var((const uint8_t *)sqlite3_column_blob(prepared->get(), i), (uint32_t)sqlite3_column_bytes(prepared->get(), i)));
          break;
        case SQLITE_NULL:
          col.push_back(Var::Empty);
          break;
        default:
          throw ActivationError("Unsupported Var type for sqlite");
        }
      }
    }

    if (rc != SQLITE_DONE) {
      throw ActivationError(sqlite3_errmsg(_connection->get()));
    }

    return output;
  }
};

void registerShards() {
  REGISTER_SHARD("DB.Query", Query);
}
} // namespace DB
} // namespace shards
