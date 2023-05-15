#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <memory>
#pragma clang attribute push(__attribute__((no_sanitize("undefined"))), apply_to = function)
#include <boost/container/stable_vector.hpp>
#pragma clang attribute pop

// ASIO must go first!!
#include <boost/asio.hpp>

#include "shards.h"

using boost::asio::ip::udp;

namespace shards {
namespace Network {

struct OnPeerConnected {
  udp::endpoint endpoint;
  std::weak_ptr<SHWire> wire;
};

struct OnPeerDisconnected {
  udp::endpoint endpoint;
  std::weak_ptr<SHWire> wire;
};

} // namespace Network
} // namespace shards

#endif