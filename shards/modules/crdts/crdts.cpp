#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>

using CrdtKey = shards::OwnedVar;
using CrdtNodeId = SHVar; // Int16/uuid
template <typename T> using CrdtVector = std::vector<T>;
template <typename K, typename V> using CrdtMap = std::unordered_map<K, V>;
template <typename K> using CrdtSet = std::unordered_set<K>;
template <typename T, typename Comparator> using CrdtSortedSet = boost::container::flat_set<T, Comparator>;
template <typename K, typename V> using CrdtTombstoneMap = std::unordered_map<K, V>;

#define CRDT_COLLECTIONS_DEFINED
#include "crdt.hpp"

namespace shards {
namespace crdts {

struct ShardsCRDT : CRDT<shards::OwnedVar, shards::OwnedVar> {
  ShardsCRDT() : CRDT<shards::OwnedVar, shards::OwnedVar>(Var::Empty) {}

  void init(SHVar id) {
    shassert(node_id_.valueType == SHType::None && "CRDT already initialized");
    node_id_ = id;
  }
};

struct CRDTTypes {
  SHVAR_OBJECT_DECL('crdt', "CRDT", CRDT, ShardsCRDT);
};

struct CRDTNew {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CRDTTypes::CRDT; }
  static SHOptionalString help() { return SHCCSTR("Creates a new crdt"); }

  PARAM_PARAMVAR(_id, "ID", "The current client's node id", {CoreInfo::Int16Type});
  PARAM_IMPL(PARAM_IMPL_FOR(_id));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    if (_crdt) {
      CRDTTypes::CRDTObjectVar.Release(_crdt);
      _crdt = nullptr;
    }
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    if (_id.isNone()) {
      throw ComposeError("ID is required");
    }

    return CRDTTypes::CRDT;
  }

  ShardsCRDT *_crdt = nullptr;

  SHVar activate(SHContext *shContext, const SHVar &input) {
    if (_crdt) {
      CRDTTypes::CRDTObjectVar.Release(_crdt);
    }
    _crdt = CRDTTypes::CRDTObjectVar.New();

    _crdt->init(_id.get());

    return CRDTTypes::CRDTObjectVar.Get(_crdt);
  }
};

struct ChangesFixedTable : shards::TableVar {
  using MapType = ShardsAlignedMap<OwnedVar, OwnedVar>;
  MapType *map = static_cast<MapType *>(payload.tableValue.opaque);

  ChangesFixedTable() {
    //! Lexigraphically sorted at compile time!!
    insert("col-name", Var::Empty);
    insert("col-version", Var::Empty);
    insert("db-version", Var::Empty);
    insert("flags", Var::Empty);
    insert("node-id", Var::Empty);
    insert("record-id", Var::Empty);
    insert("value", Var::Empty);
  }

  OwnedVar &col_name() { return map->tree().nth(0)->second; }

  OwnedVar &col_version() { return map->tree().nth(1)->second; }

  OwnedVar &db_version() { return map->tree().nth(2)->second; }

  OwnedVar &flags() { return map->tree().nth(3)->second; }

  OwnedVar &node_id() { return map->tree().nth(4)->second; }

  OwnedVar &record_id() { return map->tree().nth(5)->second; }

  OwnedVar &value() { return map->tree().nth(6)->second; }
};

inline void intoVar(Change<shards::OwnedVar, shards::OwnedVar> &&change, ChangesFixedTable &output) {
  output.record_id() = std::move(change.record_id);
  output.col_name() = change.col_name.has_value() ? std::move(change.col_name.value()) : Var::Empty;
  output.value() = change.value.has_value() ? std::move(change.value.value()) : Var::Empty;
  output.col_version() = Var(static_cast<int64_t>(change.col_version));
  output.db_version() = Var(static_cast<int64_t>(change.db_version));
  output.node_id() = change.node_id;
  output.flags() = Var((int64_t)change.flags);
}

inline void intoChange(ChangesFixedTable &input, Change<shards::OwnedVar, shards::OwnedVar> &output) {
  output.record_id = input.record_id();
  output.col_name = input.col_name();
  output.value = input.value();
  output.col_version = input.col_version().payload.intValue;
  output.db_version = input.db_version().payload.intValue;
  output.node_id = input.node_id();
  output.flags = input.flags().payload.intValue;
}

struct CRDTSet {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR("Inserts or updates a record in the crdt"); }

  PARAM_PARAMVAR(_crdt, "CRDT", "The crdt to insert or update the record in",
                 {CRDTTypes::CRDT, Type::VariableOf(CRDTTypes::CRDT)});
  PARAM_PARAMVAR(_recordId, "Record", "The id of the record to insert or update", {CoreInfo::AnyType});
  PARAM_PARAMVAR(_key, "Key", "The field's key to insert or update the record with", {CoreInfo::AnyType});
  PARAM_IMPL(PARAM_IMPL_FOR(_crdt), PARAM_IMPL_FOR(_recordId), PARAM_IMPL_FOR(_key));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return shards::CoreInfo::AnyType;
  }

  ChangesFixedTable _changeCache;

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &crdt = varAsObjectChecked<ShardsCRDT>(_crdt.get(), CRDTTypes::CRDT);
    boost::container::small_vector<Change<shards::OwnedVar, shards::OwnedVar>, 1> changes;
    crdt.insert_or_update(_recordId.get(), changes, std::make_pair(_key.get(), input));
    shassert(changes.size() == 1 && "Expected single change");

    intoVar(std::move(changes[0]), _changeCache);

    return _changeCache;
  }
};

struct CRDTApply {};
} // namespace crdts

SHARDS_REGISTER_FN(crdts) {
  using namespace crdts;
  REGISTER_SHARD("CRDT.New", CRDTNew);
  REGISTER_SHARD("CRDT.Set", CRDTSet);
}
} // namespace shards