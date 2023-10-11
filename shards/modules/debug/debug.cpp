#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>

namespace shards::Debug {
struct DebugNoop {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR("A shard that can be set to break in the debugger at a specific location"); }

  PARAM_PARAMVAR(_tag, "Tag", "Any tag to identify this debug shard", {shards::CoreInfo::NoneType, shards::CoreInfo::AnyType});
  PARAM_PARAMVAR(_inspect, "Inspect", "Anything to visualize", {shards::CoreInfo::NoneType, shards::CoreInfo::AnyType});
  PARAM_IMPL(PARAM_IMPL_FOR(_tag), PARAM_IMPL_FOR(_inspect));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext* context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    SHWire* wire = shContext->currentWire();
    SHMesh *mesh = shContext->currentWire()->mesh.lock().get();
    if (!_tag.isNone()) {
      SHVar &tagValue = _tag.get();
      SHLOG_TRACE("Triggered debug noop ({})", tagValue);
    } else {
      SHLOG_TRACE("Triggered debug noop");
    }
    return input;
  }
};
} // namespace shards::Debug

namespace shards {
SHARDS_REGISTER_FN(debug) {
  using namespace Debug;
  REGISTER_SHARD("Debug.Noop", DebugNoop);
}
} // namespace shards
