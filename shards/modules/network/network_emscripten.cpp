
#include <shards/core/shared.hpp>
#include <shards/core/foundation.hpp>
#include <shards/shards.hpp>
#include <shards/shardwrapper.hpp>
#include <shards/core/shared.hpp>
#include <shards/core/wire_doppelganger_pool.hpp>
#include <shards/core/serialization.hpp>
#include <shards/utility.hpp>
#include <optional>
#include <boost/lockfree/queue.hpp>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <stdint.h>
#include <thread>
#include <unordered_map>
#include <utility>
#include "core/async.hpp"

namespace shards {
namespace Network {
constexpr uint32_t PeerCC = 'netP';
constexpr uint32_t ServerCC = 'netS';

SHARDS_REGISTER_FN(network_em) {
  using namespace shards::Network;
  // REGISTER_SHARD("Network.Server", Server);
  // REGISTER_SHARD("Network.Broadcast", Broadcast);
  // REGISTER_SHARD("Network.Client", Client);
  // REGISTER_SHARD("Network.Send", Send);
  // REGISTER_SHARD("Network.PeerID", PeerID);
  // REGISTER_SHARD("Network.Peer", GetPeer);
}

} // namespace Network
} // namespace shards