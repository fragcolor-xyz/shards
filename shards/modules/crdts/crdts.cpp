#include "crdts.hpp"

namespace shards {
namespace crdts {

inline SHVar uuid2Var(const boost::uuids::uuid &uuid) {
  SHVar vUuid{};
  vUuid.valueType = SHType::Int16;
  memcpy(&vUuid.payload.int16Value, uuid.data, 16);
  return vUuid;
}

inline boost::uuids::uuid var2Uuid(const SHVar &v) {
  shassert(v.valueType == SHType::Int16 || v == Var::Empty);
  boost::uuids::uuid uuid;
  memcpy(uuid.data, &v.payload.int16Value, 16);
  return uuid;
}

struct ShardsCRDT : CRDT<boost::uuids::uuid, OwnedVar> {
  ShardsCRDT() : CRDT<boost::uuids::uuid, OwnedVar>(boost::uuids::nil_uuid()) {}

  void init(boost::uuids::uuid id, int64_t preallocate) {
    shassert(node_id_ == boost::uuids::nil_uuid() && "CRDT already initialized");
    node_id_ = id;
    data_.reserve(preallocate);
  }
};

struct CRDTTypes {
  SHVAR_OBJECT_DECL('crdt', "CRDT", CRDT, ShardsCRDT);

  static inline std::array<SHVar, 8> ChangesTableKeys{
      Var("col-name"),     //
      Var("col-name-key"), //
      Var("col-version"),  //
      Var("db-version"),   //
      Var("flags"),        //
      Var("node-id"),      //
      Var("record-id"),    //
      Var("value"),        //
  };
  static inline Types ChangesTableTypes{
      CoreInfo::AnyType,   //
      CoreInfo::AnyType,   //
      CoreInfo::IntType,   //
      CoreInfo::IntType,   //
      CoreInfo::IntType,   //
      CoreInfo::Int16Type, //
      CoreInfo::Int16Type, //
      CoreInfo::AnyType,   //
  };
  static inline Type ChangesTableType = Type::TableOf(ChangesTableTypes, ChangesTableKeys, true);
  static inline Type ChangesTableVarType = Type::VariableOf(ChangesTableType);
  static inline Type ChangesTableSeqType = Type::SeqOf(ChangesTableType);
};

struct CRDTNew {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CRDTTypes::CRDT; }
  static SHOptionalString help() { return SHCCSTR("Creates a new crdt"); }

  CRDTNew() : _preallocate(Var(10000)) {}

  PARAM_PARAMVAR(_id, "ID", "The current client's node id", {CoreInfo::Int16Type, CoreInfo::Int16VarType});
  PARAM_VAR(_preallocate, "Preallocate", "The number of records to preallocate", {CoreInfo::IntType});
  PARAM_IMPL(PARAM_IMPL_FOR(_id), PARAM_IMPL_FOR(_preallocate));

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

    _crdt->init(var2Uuid(_id.get()), _preallocate.payload.intValue);

    return CRDTTypes::CRDTObjectVar.Get(_crdt);
  }
};

inline void intoVar(Change<boost::uuids::uuid, OwnedVar> &&change, ChangesFixedTable &output) {
  output.record_id() = uuid2Var(change.record_id);
  if (change.col_name) {
    output.col_name() = shards::Var(change.col_name->name);
    output.col_name_key() = change.col_name->key ? shards::Var(std::move(*change.col_name->key)) : Var::Empty;
  } else {
    output.col_name() = Var::Empty;
    output.col_name_key() = Var::Empty;
  }
  output.value() = change.value.has_value() ? std::move(change.value.value()) : Var::Empty;
  output.col_version() = Var(static_cast<int64_t>(change.col_version));
  output.db_version() = Var(static_cast<int64_t>(change.db_version));
  output.node_id() = uuid2Var(change.node_id);
  output.flags() = Var((int64_t)change.flags);
}

inline void intoChange(const ChangesFixedTable &input, Change<boost::uuids::uuid, OwnedVar> &output) {
  output.record_id = var2Uuid(input.record_id());
  if (input.col_name().valueType == SHType::String) {
    if (input.col_name_key().valueType != SHType::None) {
      output.col_name = std::make_optional(CrdtKey(SHSTRVIEW(input.col_name()), input.col_name_key()));
    } else {
      output.col_name = std::make_optional(CrdtKey(SHSTRVIEW(input.col_name())));
    }
  } else {
    output.col_name = std::nullopt;
  }
  output.value = input.value();
  output.col_version = input.col_version().payload.intValue;
  output.db_version = input.db_version().payload.intValue;
  output.node_id = var2Uuid(input.node_id());
  output.flags = input.flags().payload.intValue;
}

inline void intoChange(const SHVar *input, Change<boost::uuids::uuid, OwnedVar> &output) {
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
  PARAM_PARAMVAR(_recordId, "Record", "The id of the record to insert or update", {CoreInfo::Int16Type, CoreInfo::Int16VarType});
  PARAM_PARAMVAR(_keys, "Keys",
                 "A single key or a sequence of keys to insert or update the record with, when it is a sequence, the input must "
                 "be a sequence of the corresponding values",
                 {CoreInfo::AnyType, CoreInfo::AnySeqType, CoreInfo::AnyVarType, CoreInfo::AnyVarSeqType});
  PARAM_IMPL(PARAM_IMPL_FOR(_crdt), PARAM_IMPL_FOR(_recordId), PARAM_IMPL_FOR(_keys));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo composeV2(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    if (_recordId.isNone()) {
      throw ComposeError("Record ID is required");
    }
    if (_keys.isNone()) {
      throw ComposeError("Keys are required");
    }

    shassert(data.privateContext && "Private context should be valid");
    auto inherited = reinterpret_cast<CompositionContext *>(data.privateContext);
    bool isMany = false;
    if (_keys.isVariable()) {
      auto info = findExposedVariablePtr(inherited->inherited, _keys.variableName());
      if (info->exposedType.basicType == SHType::Seq) {
        isMany = true;
      } else {
        isMany = false;
      }
    } else {
      isMany = _keys->valueType == SHType::Seq;
    }

    if (isMany) {
      if (data.inputType.basicType != SHType::Seq) {
        throw ComposeError("Input must be a sequence if keys are a sequence");
      }
      OVERRIDE_ACTIVATE2(data, activateMany);
      return CRDTTypes::ChangesTableSeqType;
    } else {
      OVERRIDE_ACTIVATE2(data, activate);
      return CRDTTypes::ChangesTableType;
    }
  }

  ChangesFixedTable _changeCache;
  SeqVar _output;
  CrdtVector<Change<boost::uuids::uuid, OwnedVar>> _changes;
  CrdtVector<std::pair<CrdtKey, OwnedVar>> _pairs;

  SHVar &activateMany(SHContext *shContext, const SHVar &input) {
    auto &crdt = varAsObjectChecked<ShardsCRDT>(_crdt.get(), CRDTTypes::CRDT);
    _changes.clear();
    _output.clear();
    _pairs.clear();

    auto &keys = asSeq(_keys.get());
    auto &values = asSeq(input);
    if (keys.size() != values.size()) {
      throw ActivationError("Keys and values must have the same size");
    }

    for (size_t i = 0; i < keys.size(); ++i) {
      auto &crdtKey = keys[i];
      if (crdtKey.valueType == SHType::String) {
        _pairs.emplace_back(CrdtKey(SHSTRVIEW(crdtKey)), values[i]);
      } else if (crdtKey.valueType == SHType::Seq) {
        auto &keys = asSeq(crdtKey);
        if (keys.size() != 2 || keys[0].valueType != SHType::String) {
          throw ActivationError("Keys must be a pair of string and anything");
        }
        _pairs.emplace_back(CrdtKey(SHSTRVIEW(keys[0]), keys[1]), values[i]);
      } else {
        // empty string + anything
        _pairs.emplace_back(CrdtKey("", crdtKey), values[i]);
      }
    }

    crdt.insert_or_update_from_container(var2Uuid(_recordId.get()), _pairs, _changes);

    for (auto &change : _changes) {
      intoVar(std::move(change), _changeCache);
      _output.push_back(_changeCache);
    }

    return _output;
  }

  SHVar &activate(SHContext *shContext, const SHVar &input) {
    auto &crdt = varAsObjectChecked<ShardsCRDT>(_crdt.get(), CRDTTypes::CRDT);

    _changes.clear();

    auto &crdtKey = _keys.get();
    if (crdtKey.valueType == SHType::String) {
      crdt.insert_or_update(var2Uuid(_recordId.get()), _changes, std::make_pair(CrdtKey(SHSTRVIEW(crdtKey)), input));
    } else if (crdtKey.valueType == SHType::Seq) {
      auto &keys = asSeq(crdtKey);
      if (keys.size() != 2 || keys[0].valueType != SHType::String) {
        throw ActivationError("Keys must be a pair of string and anything");
      }
      crdt.insert_or_update(var2Uuid(_recordId.get()), _changes, std::make_pair(CrdtKey(SHSTRVIEW(keys[0]), keys[1]), input));
    } else {
      // empty string + anything
      crdt.insert_or_update(var2Uuid(_recordId.get()), _changes, std::make_pair(CrdtKey("", crdtKey), input));
    }

    shassert(_changes.size() == 1 && "Expected single change");

    intoVar(std::move(_changes[0]), _changeCache);

    return _changeCache;
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

    CrdtVector<Change<boost::uuids::uuid, OwnedVar>> crdtChanges;

    for (auto change : changes) {
      Change<boost::uuids::uuid, OwnedVar> crdtChange;
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
  static SHTypesInfo inputTypes() { return CoreInfo::Int16Type; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR("Gets a record from the crdt"); }

  PARAM_PARAMVAR(_crdt, "CRDT", "The crdt to get the record from", {CRDTTypes::CRDT, Type::VariableOf(CRDTTypes::CRDT)});
  PARAM_PARAMVAR(_keys, "Keys", "The field's keys to get from the record", {CoreInfo::AnySeqType, CoreInfo::AnyVarSeqType});
  PARAM_IMPL(PARAM_IMPL_FOR(_crdt), PARAM_IMPL_FOR(_keys));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    if (_keys.isNone()) {
      throw ComposeError("Keys are required");
    }

    return CoreInfo::AnyType;
  }

  SeqVar _output;

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &crdt = varAsObjectChecked<ShardsCRDT>(_crdt.get(), CRDTTypes::CRDT);
    auto recordId = OwnedVar::Foreign(input); // avoid copy, this makes it CoW
    auto &keys = asSeq(_keys.get());
    auto record = crdt.get_record(var2Uuid(recordId));
    _output.clear();
    if (record) {
      for (auto &key : keys) {
        auto crdtKey = [&]() {
          if (key.valueType == SHType::String) {
            return CrdtKey(SHSTRVIEW(key));
          } else if (key.valueType == SHType::Seq) {
            auto &keys = asSeq(key);
            if (keys.size() != 2 || keys[0].valueType != SHType::String) {
              throw ActivationError("Keys must be a pair of string and anything");
            }
            return CrdtKey(SHSTRVIEW(keys[0]), keys[1]);
          } else {
            // empty string + anything
            return CrdtKey("", key);
          }
        }();
        auto it = record->fields.find(crdtKey);
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

struct CRDTDelete {
  static SHTypesInfo inputTypes() { return CoreInfo::Int16Type; }
  static SHTypesInfo outputTypes() { return CRDTTypes::ChangesTableType; }
  static SHOptionalString help() { return SHCCSTR("Deletes a record from the crdt"); }

  PARAM_PARAMVAR(_crdt, "CRDT", "The crdt to delete the record from", {CRDTTypes::CRDT, Type::VariableOf(CRDTTypes::CRDT)});
  PARAM_IMPL(PARAM_IMPL_FOR(_crdt));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return CRDTTypes::ChangesTableType;
  }

  ChangesFixedTable _changeCache;

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &crdt = varAsObjectChecked<ShardsCRDT>(_crdt.get(), CRDTTypes::CRDT);
    auto recordId = OwnedVar::Foreign(input); // avoid copy, this makes it CoW
    CrdtVector<Change<boost::uuids::uuid, OwnedVar>> changes;
    crdt.delete_record(var2Uuid(recordId), changes);
    shassert(changes.size() == 1 && "Expected single change");
    intoVar(std::move(changes[0]), _changeCache);
    return _changeCache;
  }
};

struct CRDTGetVersion {
  static SHTypesInfo inputTypes() { return CRDTTypes::CRDT; }
  static SHTypesInfo outputTypes() { return CoreInfo::IntType; }
  static SHOptionalString help() {
    return SHCCSTR("Gets the version of the crdt, notice that internally it's a uint64, but shards Int is signed, "
                   "so it might go negative if it's over 2^63, that does not mean the value is not correct.");
  }

  PARAM_IMPL();

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &crdt = varAsObjectChecked<ShardsCRDT>(input, CRDTTypes::CRDT);
    auto version = crdt.get_clock().current_time();
    return Var(static_cast<int64_t>(version));
  }
};

struct CRDTChangesSince {
  static SHTypesInfo inputTypes() { return CoreInfo::IntType; }
  static SHTypesInfo outputTypes() { return CRDTTypes::ChangesTableSeqType; }
  static SHOptionalString help() { return SHCCSTR("Gets the changes since the given version"); }

  PARAM_PARAMVAR(_crdt, "CRDT", "The crdt to get the changes from", {CRDTTypes::CRDT, Type::VariableOf(CRDTTypes::CRDT)});
  PARAM_IMPL(PARAM_IMPL_FOR(_crdt));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return CRDTTypes::ChangesTableSeqType;
  }

  SeqVar _output;
  ChangesFixedTable _changeCache;

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &crdt = varAsObjectChecked<ShardsCRDT>(_crdt.get(), CRDTTypes::CRDT);
    auto changes = crdt.get_changes_since(input.payload.intValue);
    _output.clear();
    for (auto &change : changes) {
      intoVar(std::move(change), _changeCache);
      _output.push_back(_changeCache);
    }
    return _output;
  }
};

struct CRDTCompactTombstones {
  static SHTypesInfo inputTypes() { return CoreInfo::IntType; }
  static SHTypesInfo outputTypes() { return CoreInfo::IntType; }
  static SHOptionalString help() { return SHCCSTR("Reduces memory usage of the CRDT by removing old tombstones"); }

  PARAM_PARAMVAR(_crdt, "CRDT", "The crdt to compact", {CRDTTypes::CRDT, Type::VariableOf(CRDTTypes::CRDT)});
  PARAM_IMPL(PARAM_IMPL_FOR(_crdt));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return CoreInfo::IntType;
  }

  void activate(SHContext *shContext, const SHVar &input) {
    auto &crdt = varAsObjectChecked<ShardsCRDT>(_crdt.get(), CRDTTypes::CRDT);
    crdt.compact_tombstones(input.payload.intValue);
  }
};

struct CRDTTombstones {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::IntType; }
  static SHOptionalString help() { return SHCCSTR("Gets the number of tombstones in the CRDT"); }

  PARAM_PARAMVAR(_crdt, "CRDT", "The crdt to get the tombstones from", {CRDTTypes::CRDT, Type::VariableOf(CRDTTypes::CRDT)});
  PARAM_IMPL(PARAM_IMPL_FOR(_crdt));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return CoreInfo::IntType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &crdt = varAsObjectChecked<ShardsCRDT>(_crdt.get(), CRDTTypes::CRDT);
    return Var(static_cast<int64_t>(crdt.tombstone_count()));
  }
};
} // namespace crdts

SHARDS_REGISTER_FN(crdts) {
  using namespace crdts;
  REGISTER_SHARD("CRDT.New", CRDTNew);
  REGISTER_SHARD("CRDT.Set", CRDTSet);
  REGISTER_SHARD("CRDT.Apply", CRDTApply);
  REGISTER_SHARD("CRDT.Get", CRDTGet);
  REGISTER_SHARD("CRDT.Delete", CRDTDelete);
  REGISTER_SHARD("CRDT.Version", CRDTGetVersion);
  REGISTER_SHARD("CRDT.ChangesSince", CRDTChangesSince);
  REGISTER_SHARD("CRDT.CompactTombstones", CRDTCompactTombstones);
  REGISTER_SHARD("CRDT.Tombstones", CRDTTombstones);
}
} // namespace shards