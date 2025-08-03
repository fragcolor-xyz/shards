#include <shards/shards.hpp>
#include <shards/utility.hpp>
#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <boost/container/small_vector.hpp>

using CrdtKey = shards::OwnedVar;
using CrdtNodeId = SHVar; // Int16/uuid
template <typename T> using CrdtVector = boost::container::small_vector<T, 4>;
template <typename K, typename V> using CrdtMap = boost::unordered_flat_map<K, V, std::hash<K>>;
template <typename K> using CrdtSet = boost::unordered_flat_set<K, std::hash<K>>;
template <typename T, typename Comparator> using CrdtSortedSet = boost::container::flat_set<T, Comparator>;
template <typename K, typename V> using CrdtTombstoneMap = boost::unordered_flat_map<K, V, std::hash<K>>;

#define CRDT_COLLECTIONS_DEFINED
#include "crdt.hpp"

namespace shards {
namespace crdts {

struct ShardsCRDT : CRDT<OwnedVar, OwnedVar> {
  ShardsCRDT() : CRDT<OwnedVar, OwnedVar>(Var::Empty) {}

  void init(SHVar id) {
    shassert(node_id_.valueType == SHType::None && "CRDT already initialized");
    node_id_ = id;
  }
};

struct CRDTTypes {
  SHVAR_OBJECT_DECL('crdt', "CRDT", CRDT, ShardsCRDT);

  static inline std::array<SHVar, 7> ChangesTableKeys{
      Var("col-name"),    //
      Var("col-version"), //
      Var("db-version"),  //
      Var("flags"),       //
      Var("node-id"),     //
      Var("record-id"),   //
      Var("value"),       //
  };
  static inline Types ChangesTableTypes{
      CoreInfo::AnyType,   //
      CoreInfo::IntType,   //
      CoreInfo::IntType,   //
      CoreInfo::IntType,   //
      CoreInfo::Int16Type, //
      CoreInfo::AnyType,   //
      CoreInfo::AnyType,   //
  };
  static inline Type ChangesTableType = Type::TableOf(ChangesTableTypes, ChangesTableKeys);
  static inline Type ChangesTableVarType = Type::VariableOf(ChangesTableType);
  static inline Type ChangesTableSeqType = Type::SeqOf(ChangesTableType);
};

struct CRDTNew {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
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

struct ChangesFixedTable : TableVar {
  using MapType = ShardsAlignedMap<OwnedVar, OwnedVar>;

  constexpr MapType &map() { return *static_cast<MapType *>(payload.tableValue.opaque); }
  constexpr const MapType &map() const { return *static_cast<const MapType *>(payload.tableValue.opaque); }

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

  OwnedVar &col_name() { return map().tree().nth(0)->second; }

  const OwnedVar &col_name() const { return map().tree().nth(0)->second; }

  OwnedVar &col_version() { return map().tree().nth(1)->second; }

  const OwnedVar &col_version() const { return map().tree().nth(1)->second; }

  OwnedVar &db_version() { return map().tree().nth(2)->second; }

  const OwnedVar &db_version() const { return map().tree().nth(2)->second; }

  OwnedVar &flags() { return map().tree().nth(3)->second; }

  const OwnedVar &flags() const { return map().tree().nth(3)->second; }

  OwnedVar &node_id() { return map().tree().nth(4)->second; }

  const OwnedVar &node_id() const { return map().tree().nth(4)->second; }

  OwnedVar &record_id() { return map().tree().nth(5)->second; }

  const OwnedVar &record_id() const { return map().tree().nth(5)->second; }

  OwnedVar &value() { return map().tree().nth(6)->second; }

  const OwnedVar &value() const { return map().tree().nth(6)->second; }
};

inline void intoVar(Change<OwnedVar, OwnedVar> &&change, ChangesFixedTable &output) {
  output.record_id() = std::move(change.record_id);
  output.col_name() = change.col_name.has_value() ? std::move(change.col_name.value()) : Var::Empty;
  output.value() = change.value.has_value() ? std::move(change.value.value()) : Var::Empty;
  output.col_version() = Var(static_cast<int64_t>(change.col_version));
  output.db_version() = Var(static_cast<int64_t>(change.db_version));
  output.node_id() = change.node_id;
  output.flags() = Var((int64_t)change.flags);
}

inline void intoChange(const ChangesFixedTable &input, Change<OwnedVar, OwnedVar> &output) {
  output.record_id = input.record_id();
  output.col_name = input.col_name();
  output.value = input.value();
  output.col_version = input.col_version().payload.intValue;
  output.db_version = input.db_version().payload.intValue;
  output.node_id = input.node_id();
  output.flags = input.flags().payload.intValue;
}

inline void intoChange(const SHVar *input, Change<OwnedVar, OwnedVar> &output) {
  auto *changeTable = reinterpret_cast<const ChangesFixedTable *>(input);
  intoChange(*changeTable, output);
}

struct CRDTSet {
  static inline Types InputTypes{CoreInfo::AnyType, CoreInfo::AnySeqType};
  static inline Types OutputTypes{CRDTTypes::ChangesTableType, CRDTTypes::ChangesTableSeqType};

  static SHTypesInfo inputTypes() { return InputTypes; }
  static SHTypesInfo outputTypes() { return OutputTypes; }
  static SHOptionalString help() { return SHCCSTR("Inserts or updates a record in the crdt"); }

  PARAM_PARAMVAR(_crdt, "CRDT", "The crdt to insert or update the record in",
                 {CRDTTypes::CRDT, Type::VariableOf(CRDTTypes::CRDT)});
  PARAM_PARAMVAR(_recordId, "Record", "The id of the record to insert or update", {CoreInfo::AnyType});
  PARAM_PARAMVAR(_keys, "Keys",
                 "A single key or a sequence of keys to insert or update the record with, when it is a sequence, the input must "
                 "be a sequence of the corresponding values",
                 {CoreInfo::AnyType, CoreInfo::AnySeqType});
  PARAM_IMPL(PARAM_IMPL_FOR(_crdt), PARAM_IMPL_FOR(_recordId), PARAM_IMPL_FOR(_keys));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  bool _isMany{false};

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo composeV2(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    shassert(data.privateContext && "Private context should be valid");
    auto inherited = reinterpret_cast<CompositionContext *>(data.privateContext);
    if (_keys.isVariable()) {
      auto info = findExposedVariablePtr(inherited->inherited, _keys.variableName());
      if (info->exposedType.basicType == SHType::Seq) {
        _isMany = true;
      } else {
        _isMany = false;
      }
    } else {
      _isMany = _keys->valueType == SHType::Seq;
    }

    if (_isMany) {
      return CRDTTypes::ChangesTableSeqType;
    } else {
      return CRDTTypes::ChangesTableType;
    }
  }

  ChangesFixedTable _changeCache;
  SeqVar _output;
  CrdtVector<Change<OwnedVar, OwnedVar>> _changes;
  CrdtVector<std::pair<OwnedVar, OwnedVar>> _pairs;

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &crdt = varAsObjectChecked<ShardsCRDT>(_crdt.get(), CRDTTypes::CRDT);
    if (!_isMany) {
      _changes.clear();
      crdt.insert_or_update(_recordId.get(), _changes, std::make_pair(_keys.get(), input));
      shassert(_changes.size() == 1 && "Expected single change");

      intoVar(std::move(_changes[0]), _changeCache);

      return _changeCache;
    } else {
      _changes.clear();
      _output.clear();
      _pairs.clear();

      auto &keys = asSeq(_keys.get());
      auto &values = asSeq(input);
      if (keys.size() != values.size()) {
        throw ActivationError("Keys and values must have the same size");
      }

      for (size_t i = 0; i < keys.size(); ++i) {
        _pairs.emplace_back(keys[i], values[i]);
      }

      crdt.insert_or_update_from_container(_recordId.get(), _pairs, _changes);

      for (auto &change : _changes) {
        intoVar(std::move(change), _changeCache);
        _output.push_back(_changeCache);
      }

      return _output;
    }
  }
};

struct CRDTApply {
  static SHTypesInfo inputTypes() { return CRDTTypes::ChangesTableSeqType; }
  static SHTypesInfo outputTypes() { return CRDTTypes::ChangesTableSeqType; }
  static SHOptionalString help() {
    return SHCCSTR("Applies a list of changes to the crdt, outputs the changes that were applied");
  }

  PARAM_PARAMVAR(_crdt, "CRDT", "The crdt to apply the change to", {CRDTTypes::CRDT, Type::VariableOf(CRDTTypes::CRDT)});
  PARAM_VAR(_outputApplied, "OutputApplied", "If true, outputs the changes that were applied", {CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_crdt), PARAM_IMPL_FOR(_outputApplied));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return CRDTTypes::ChangesTableSeqType;
  }

  SeqVar _appliedChanges;

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &crdt = varAsObjectChecked<ShardsCRDT>(_crdt.get(), CRDTTypes::CRDT);

    auto &changes = asSeq(input);

    CrdtVector<Change<OwnedVar, OwnedVar>> crdtChanges;

    for (auto change : changes) {
      Change<OwnedVar, OwnedVar> crdtChange;
      intoChange(&change, crdtChange);
      crdtChanges.emplace_back(std::move(crdtChange));
    }

    if (_outputApplied.payload.boolValue) {
      _appliedChanges.clear();
      auto appliedChanges = crdt.merge_changes<true>(std::move(crdtChanges));

      for (auto &change : appliedChanges) {
        ChangesFixedTable changeTable;
        intoVar(std::move(change), changeTable);
        _appliedChanges.push_back(changeTable);
      }

      return _appliedChanges;
    } else {
      crdt.merge_changes(std::move(crdtChanges));
      return input;
    }
  }
};

struct CRDTGet {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR("Gets a record from the crdt"); }

  PARAM_PARAMVAR(_crdt, "CRDT", "The crdt to get the record from", {CRDTTypes::CRDT, Type::VariableOf(CRDTTypes::CRDT)});
  PARAM_PARAMVAR(_keys, "Keys", "The field's keys to get from the record", {CoreInfo::AnySeqType});
  PARAM_IMPL(PARAM_IMPL_FOR(_crdt), PARAM_IMPL_FOR(_keys));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return CoreInfo::AnyType;
  }

  SeqVar _output;

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &crdt = varAsObjectChecked<ShardsCRDT>(_crdt.get(), CRDTTypes::CRDT);
    auto recordId = OwnedVar::Foreign(input); // avoid copy, this makes it CoW
    auto &keys = asSeq(_keys.get());
    auto record = crdt.get_record(recordId);
    _output.clear();
    if (record) {
      for (auto &key : keys) {
        auto it = record->fields.find(key);
        if (it != record->fields.end()) {
          _output.push_back(it->second);
        } else {
          _output.push_back(Var::Empty);
        }
      }
    }
    return _output;
  }
};
} // namespace crdts

SHARDS_REGISTER_FN(crdts) {
  using namespace crdts;
  REGISTER_SHARD("CRDT.New", CRDTNew);
  REGISTER_SHARD("CRDT.Set", CRDTSet);
  REGISTER_SHARD("CRDT.Apply", CRDTApply);
  REGISTER_SHARD("CRDT.Get", CRDTGet);
}
} // namespace shards