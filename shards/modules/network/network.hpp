#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <memory>

#ifdef __clang__
#pragma clang attribute push(__attribute__((no_sanitize("undefined"))), apply_to = function)
#endif

// ASIO must go first!!
#include <boost/asio.hpp>

#ifdef __clang__
#pragma clang attribute pop
#endif

#include <shards/shards.h>

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