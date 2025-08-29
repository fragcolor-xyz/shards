#include "memoize.hpp"

SHARDS_REGISTER_FN(memoize) {
  REGISTER_SHARD("Memoize", shards::Memoize);
  REGISTER_SHARD("Track", shards::Track);
  REGISTER_SHARD("Trigger", shards::Trigger);
}
