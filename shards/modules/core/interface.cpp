#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>

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
    throw std::logic_error("MakeInterface is not implemented");
  }
};
} // namespace shards

#ifdef interface
#undef interface
#endif
SHARDS_REGISTER_FN(interface) { REGISTER_SHARD("MakeInterface", shards::MakeInterface); }