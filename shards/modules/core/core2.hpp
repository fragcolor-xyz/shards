#ifndef F94041FD_F0BF_4193_844E_7B91ED7E8636
#define F94041FD_F0BF_4193_844E_7B91ED7E8636

#include <shards/core/params.hpp>
#include <shards/core/compose.hpp>
#include <shards/common_types.hpp>

namespace shards::new_core {

struct Set {
  PARAM_PARAMVAR(_name, "Name", "The name of the variable to set.", {CoreInfo::StringOrAnyVar});
  PARAM_VAR(_global, "Global", "If the variable is global.", {CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_name), PARAM_IMPL_FOR(_global));

  static SHTypesInfo inputTypes();
  static SHTypesInfo outputTypes();

  ExposedInfo _exposed;
  SHVar *_slot{};

  Set();
  SHExposedTypesInfo exposedVariables();
  SHTypeInfo compose(SHInstanceData &data);
  void warmup(SHContext *ctx);
  void cleanup(SHContext *ctx);
  SHVar activate(SHContext *ctx, const SHVar &input);
};

struct Ref {
  PARAM_PARAMVAR(_name, "Name", "The name of the variable to set.", {CoreInfo::StringOrAnyVar});
  PARAM_VAR(_global, "Global", "If the variable is global.", {CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_name), PARAM_IMPL_FOR(_global));

  static SHTypesInfo inputTypes();
  static SHTypesInfo outputTypes();

  ExposedInfo _exposed;
  SHVar *_slot{};

  Ref();
  SHExposedTypesInfo exposedVariables();
  SHTypeInfo compose(SHInstanceData &data);
  SHVar activate(SHContext *ctx, const SHVar &input);
};

struct Update {
  PARAM_PARAMVAR(_name, "Name", "The name of the variable to update.", {CoreInfo::StringOrAnyVar});
  PARAM_VAR(_global, "Global", "If the variable is global.", {CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_name), PARAM_IMPL_FOR(_global));

  static SHTypesInfo inputTypes();
  static SHTypesInfo outputTypes();

  SHVar *_slot{};
  bool _existingDeclaredAsGlobal{};

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data);
  void warmup(SHContext *ctx);
  void cleanup(SHContext *ctx);
  SHVar activate(SHContext *ctx, const SHVar &input);
};

struct Get {
  PARAM_PARAMVAR(_name, "Name", "The name of the variable to get.", {CoreInfo::StringOrAnyVar});
  PARAM_IMPL(PARAM_IMPL_FOR(_name));

  SHVar *_slot{};

  static SHTypesInfo inputTypes();
  static SHTypesInfo outputTypes();

  void warmup(SHContext *ctx);
  void cleanup(SHContext *ctx);
  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo composeV2(SHInstanceData &data);
  SHVar activate(SHContext *ctx, const SHVar &input);
};

void registerCore2();

} // namespace shards

#endif /* F94041FD_F0BF_4193_844E_7B91ED7E8636 */
