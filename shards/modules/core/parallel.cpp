#include <shards/core/module.hpp>
#include <shards/core/params.hpp>
#include <shards/core/foundation.hpp>
#include <shards/common_types.hpp>

namespace shards {

struct ProcessorCount {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::IntType; }

  SHVar activate(SHContext *context, const SHVar &input) { return Var(std::thread::hardware_concurrency()); }
};

SHARDS_REGISTER_FN(parallel) { REGISTER_SHARD("_ProcessorCount", ProcessorCount); }
} // namespace shards