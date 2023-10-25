#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>
#include <tracy/Wrapper.hpp>

namespace shards::Tracy {
const uint32_t &toTracyColor(const SHVar &v) { return *(uint32_t *)&v.payload.colorValue; }
struct Message {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::StringType; }
  static SHOptionalString help() { return SHCCSTR(""); }

  PARAM_PARAMVAR(_color, "Color", "", {shards::CoreInfo::NoneType, shards::CoreInfo::ColorType});
  PARAM_IMPL(PARAM_IMPL_FOR(_color));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup() { PARAM_CLEANUP(); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    if (_color.isNone())
      TracyMessage(input.payload.stringValue, input.payload.stringLen);
    else
      TracyMessageC(input.payload.stringValue, input.payload.stringLen, toTracyColor(_color));
    return input;
  }
};
} // namespace shards::Tracy

namespace shards {
SHARDS_REGISTER_FN(tracy) {
  using namespace Tracy;
  REGISTER_SHARD("Tracy.Message", Message);
}
} // namespace shards