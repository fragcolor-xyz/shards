#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <memory>

// ASIO must go first!!
#pragma clang attribute push(__attribute__((no_sanitize("undefined"))), apply_to = function)
#include <boost/asio.hpp>
#pragma clang attribute pop

#include <shards/shards.h>

using boost::asio::ip::tcp;

namespace shards {
namespace Network {

struct OnPeerConnected {
  tcp::endpoint endpoint;
  std::weak_ptr<SHWire> wire;
};

struct OnPeerDisconnected {
  tcp::endpoint endpoint;
  std::weak_ptr<SHWire> wire;
};

} // namespace Network
} // namespace shards

#endif