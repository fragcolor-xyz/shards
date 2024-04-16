#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/core/hash.inl>
#include <shards/common_types.hpp>

#define XXH_INLINE_ALL
#include <xxhash.h>

namespace shards {
struct MakeInterface {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR(""); }

  static inline shards::Types TypeTableTypes{{
      shards::CoreInfo::TypeType,
  }};
  static inline shards::Type TypeTable = shards::Type::TableOf(TypeTableTypes);

  PARAM_VAR(_name, "Name", "The interface name", {shards::CoreInfo::StringType});
  PARAM_VAR(_types, "Types", "The interface types", {TypeTable});
  PARAM_IMPL(PARAM_IMPL_FOR(_name), PARAM_IMPL_FOR(_types));

  SHInterfaceVariables _tmpVariables;
  SHInterface _tmpInterface;
  HashState<XXH128_hash_t> _hashState;

  ~MakeInterface() { arrayFree(_tmpVariables); }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    SPDLOG_INFO("MakeInterface: {}", _name);
    SPDLOG_INFO("Interface types: {}", TypeTableTypes);

    SHVar output{
        .payload =
            SHVarPayload{
                .interfaceValue = &_tmpInterface,
            },
        .valueType = SHType::Interface,
    };

    XXH3_state_t hashState;
    XXH3_128bits_reset(&hashState);

    arrayResize(_tmpVariables, 0);
    for (auto &[k, v] : _types.payload.tableValue) {
      if (k.valueType != SHType::String)
        throw std::runtime_error(fmt::format("MakeInterface: variable name must be a string ({})", k));
      auto &fieldType = *v.payload.typeValue;

      SHInterfaceVariable var{
          .name = k.payload.stringValue,
          .type = fieldType,
      };
      arrayPush(_tmpVariables, var);
    }

    std::sort(begin(_tmpVariables), end(_tmpVariables), [](const SHInterfaceVariable &a, const SHInterfaceVariable &b) {
      return std::string_view(a.name) < std::string_view(b.name);
    });

    for (auto &var : _tmpVariables) {
      XXH3_128bits_update(&hashState, var.name, strlen(var.name));
      _hashState.updateTypeHash(var.type, &hashState);
    }

    auto digest = XXH3_128bits_digest(&hashState);
    memcpy(_tmpInterface.id, &digest, sizeof(SHInterface::id));
    _tmpInterface.name = _name.payload.stringValue;
    _tmpInterface.variables = _tmpVariables;
    return output;
  }
};
} // namespace shards

#ifdef interface
#undef interface
#endif
SHARDS_REGISTER_FN(interface) { REGISTER_SHARD("MakeInterface", shards::MakeInterface); }