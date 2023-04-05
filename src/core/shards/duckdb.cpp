#include "duckdb/common/types.hpp"
#include "duckdb/main/connection.hpp"
#include "duckdb/main/database.hpp"
#include "foundation.hpp"
#include "shards.h"
#include "shards.hpp"
#include "shared.hpp"
#include <duckdb.hpp>
#include "params.hpp"
#include "utility.hpp"

namespace shards {
namespace DB {
duckdb::Value toDuckDBValue(const SHVar &var) {
  switch (var.valueType) {
  case SHType::Bool:
    return duckdb::Value::BOOLEAN(var.payload.boolValue);
  case SHType::Int:
    return duckdb::Value::BIGINT(var.payload.intValue);
  case SHType::Float:
    return duckdb::Value::DOUBLE(var.payload.floatValue);
  case SHType::String: {
    auto sv = SHSTRVIEW(var);
    static thread_local std::string scratch;
    scratch.clear();
    scratch.append(sv.data(), sv.size());
    return duckdb::Value(scratch);
  }
  case SHType::Bytes: {
    auto sv = SHSTRVIEW(var);
    return duckdb::Value::BLOB(var.payload.bytesValue, var.payload.bytesSize);
  }
  case SHType::None:
    return duckdb::Value();
  default:
    throw ActivationError("Unsupported Var type for DuckDB");
  }
}

void fromDuckDBValue(const duckdb::Value &value, SHVar &var) {
  if(value.IsNull())
    return;

  switch (value.type().id()) {
  case duckdb::LogicalTypeId::BOOLEAN:
    var.payload.boolValue = value.GetValue<bool>();
    var.valueType = SHType::Bool;
    break;
  case duckdb::LogicalTypeId::BIGINT:
    var.payload.intValue = value.GetValue<int64_t>();
    var.valueType = SHType::Int;
    break;
  case duckdb::LogicalTypeId::DOUBLE:
    var.payload.floatValue = value.GetValue<double>();
    var.valueType = SHType::Float;
    break;
  case duckdb::LogicalTypeId::VARCHAR:
    var.payload.stringValue = value.GetValue<std::string>().c_str();
    var.payload.stringLen = value.GetValue<std::string>().size();
    var.valueType = SHType::String;
    break;
  case duckdb::LogicalTypeId::BLOB:
    var.payload.bytesValue = (uint8_t *)value.GetValue<std::string>().c_str();
    var.payload.bytesSize = value.GetValue<std::string>().size();
    var.valueType = SHType::Bytes;
    break;
  default:
    throw ActivationError("Unsupported Var type for DuckDB");
  }
}

duckdb::DuckDB &getDatabase() {
  static duckdb::DuckDB db("shards-db");
  return db;
}

duckdb::Connection &getConnection() {
  static thread_local duckdb::Connection con(getDatabase());
  return con;
}

struct Query {
  static SHTypesInfo inputTypes() { return CoreInfo::AnySeqType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyTableType; }

  PARAM_VAR(_query, "Query", "The database query to execute every activation.", {CoreInfo::StringType});
  PARAM_IMPL(Query, PARAM_IMPL_FOR(_query));

  void cleanup() {
    PARAM_CLEANUP();
    prepared.reset();
    lastResult.reset();
  }

  std::unique_ptr<duckdb::PreparedStatement> prepared;
  std::unique_ptr<duckdb::QueryResult> lastResult;

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  std::vector<duckdb::Value> inputs;
  TableVar output;
  std::vector<SeqVar *> colSeqs;

  SHVar activate(SHContext *context, const SHVar &input) {
    if (!prepared) {
      auto queryView = SHSTRVIEW(_query);
      std::string query(queryView);
      prepared = getConnection().Prepare(query);
      if (prepared->HasError()) {
        throw ActivationError(prepared->GetError());
      }
    }

    inputs.clear();
    for (auto value : input) {
      inputs.push_back(toDuckDBValue(value));
    }

    lastResult = prepared->Execute(inputs);
    if(lastResult->HasError()) {
      throw ActivationError(lastResult->GetError());
    }

    auto cols = lastResult->ColumnCount();

    colSeqs.clear();
    for (size_t i = 0; i < cols; i++) {
      auto &col = output[lastResult->ColumnName(i)];
      if (col.valueType == SHType::None) {
        SeqVar seq;
        output.insert(lastResult->ColumnName(i), std::move(seq));
        colSeqs.push_back(&asSeq(output[lastResult->ColumnName(i)]));
      } else {
        auto &seq = asSeq(col);
        seq.clear();
        colSeqs.push_back(&seq);
      }
    }

    for (auto &res : *lastResult) {
      for (size_t i = 0; i < cols; i++) {
        auto &seq = *colSeqs[i];
        SHVar var{};
        fromDuckDBValue(res.GetValue<duckdb::Value>(i), var);
        seq.push_back(var);
      }
    }

    return output;
  }
};

void registerShards() { REGISTER_SHARD("DB.Query", Query); }
} // namespace DB
} // namespace shards
