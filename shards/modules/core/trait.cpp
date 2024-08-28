#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/core/hash.inl>
#include <shards/common_types.hpp>

#define XXH_INLINE_ALL
#include <xxhash.h>

namespace shards {
struct MakeTrait {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR("This shard creates a trait with the specified name and types and makes it available for use in the wire."); }

  static SHOptionalString inputHelp() { return DefaultHelpText::InputHelpIgnored; }
  static SHOptionalString outputHelp() { return SHCCSTR("The trait object created."); }

  static inline shards::Types TypeTableTypes{{
      shards::CoreInfo::TypeType,
  }};
  static inline shards::Type TypeTable = shards::Type::TableOf(TypeTableTypes);

  PARAM_VAR(_name, "Name", "The trait name", {shards::CoreInfo::StringType});
  PARAM_VAR(_types, "Types", "The trait types", {TypeTable});
  PARAM_IMPL(PARAM_IMPL_FOR(_name), PARAM_IMPL_FOR(_types));

  SHTraitVariables _tmpVariables;
  SHTrait _tmpTrait;
  HashState<XXH128_hash_t> _hashState;

  ~MakeTrait() { arrayFree(_tmpVariables); }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    SPDLOG_TRACE("MakeTrait: {} {}", _name, _types);

    _hashState.reset();

    SHVar output{
        .payload =
            SHVarPayload{
                .traitValue = &_tmpTrait,
            },
        .valueType = SHType::Trait,
    };

    XXH3_state_t hashState;
    XXH3_128bits_reset(&hashState);

    arrayResize(_tmpVariables, 0);
    for (auto &[k, v] : _types.payload.tableValue) {
      if (k.valueType != SHType::String)
        throw std::runtime_error(fmt::format("MakeTrait: variable name must be a string ({})", k));
      auto &fieldType = *v.payload.typeValue;

      SHTraitVariable var{
          .name =
              SHStringWithLen{
                  .string = k.payload.stringValue,
                  .len = k.payload.stringLen,
              },
          .type = fieldType,
      };
      arrayPush(_tmpVariables, var);
    }

    std::sort(begin(_tmpVariables), end(_tmpVariables),
              [](const SHTraitVariable &a, const SHTraitVariable &b) { return toStringView(a.name) < toStringView(b.name); });

    for (auto &var : _tmpVariables) {
      XXH3_128bits_update(&hashState, var.name.string, var.name.len);
      _hashState.updateTypeHash(var.type, &hashState);
    }

    auto digest = XXH3_128bits_digest(&hashState);
    memcpy(_tmpTrait.id, &digest, sizeof(SHTrait::id));
    _tmpTrait.name =
        SHStringWithLen{
            .string = _name.payload.stringValue,
            .len = _name.payload.stringLen,
        },
    _tmpTrait.variables = _tmpVariables;
    return output;
  }
};

struct TraitId {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::Int2Type; }
  static SHOptionalString help() { return SHCCSTR("Retrieves the hash id of the given trait"); }

  PARAM_VAR(_trait, "Trait", "The trait", {CoreInfo::TraitType});
  PARAM_IMPL(PARAM_IMPL_FOR(_trait));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    if (_trait->isNone())
      throw std::runtime_error("TraitId: trait is required");

    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    shassert(_trait->payload.traitValue);
    Int2VarPayload i2{};
    i2.int2Value[0] = _trait->payload.traitValue->id[0];
    i2.int2Value[1] = _trait->payload.traitValue->id[1];
    return Var(i2);
  }
};
} // namespace shards

SHARDS_REGISTER_FN(trait) {
  REGISTER_SHARD("MakeTrait", shards::MakeTrait);
  REGISTER_SHARD("TraitId", shards::TraitId);
}