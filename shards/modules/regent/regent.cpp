#include "regent.hpp"

namespace shards::regent {
std::atomic_uint64_t Node::UidCounter = 0;
std::atomic_uint64_t Core::UidCounter = 0;
} // namespace shards::regent