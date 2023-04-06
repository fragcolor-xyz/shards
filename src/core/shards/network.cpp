/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

// ASIO must go first!!
#include <boost/asio.hpp>

#include "../runtime.hpp"
#include "shards.hpp"
#include "shardwrapper.hpp"
#include "shared.hpp"
#include "utility.hpp"
#include <boost/lockfree/queue.hpp>
#include <functional>
#include <memory>
#include <thread>
#include <utility>

using boost::asio::ip::udp;

namespace shards {
namespace Network {
constexpr uint32_t SocketCC = 'netS';

struct SocketData {
  udp::socket *socket;
  udp::endpoint *endpoint;
};

struct NetworkContext {
  boost::asio::io_context _io_context;
  std::thread _io_context_thread;

  NetworkContext() {
    _io_context_thread = std::thread([this] {
      // Force run to run even without work
      boost::asio::executor_work_guard<boost::asio::io_context::executor_type> g = boost::asio::make_work_guard(_io_context);
      try {
        SHLOG_DEBUG("Boost asio context running...");
        _io_context.run();
      } catch (...) {
        SHLOG_ERROR("Boost asio context run failed.");
      }
      SHLOG_DEBUG("Boost asio context exiting...");
    });
  }

  ~NetworkContext() {
    boost::asio::post(_io_context, [this]() {
      // allow end/thread exit
      _io_context.stop();
    });
    _io_context_thread.join();
  }
};

struct NetworkBase {
  ShardsVar _blks{};

  static inline Type SocketInfo{{SHType::Object, {.object = {.vendorId = CoreCC, .typeId = SocketCC}}}};

  ParamVar _addr{Var("localhost")};
  ParamVar _port{Var(9191)};

  // Every server/client will share same context, so sharing the same recv
  // buffer is possible and nice!
  ThreadShared<std::array<char, 0xFFFF>> _recv_buffer;

  SHVar *_socketVar = nullptr;
  SocketData _socket{};

  static inline ParamsInfo params = ParamsInfo(
      ParamsInfo::Param("Address", SHCCSTR("The local bind address or the remote address."), CoreInfo::StringOrStringVar),
      ParamsInfo::Param("Port", SHCCSTR("The port to bind if server or to connect to if client."), CoreInfo::IntOrIntVar),
      ParamsInfo::Param("Receive", SHCCSTR("The flow to execute when a packet is received."), CoreInfo::ShardsOrNone));

  static SHParametersInfo parameters() { return SHParametersInfo(params); }

  std::shared_ptr<entt::any> _contextStorage;
  NetworkContext *_context = nullptr;

  void cleanup() {
    if (_context) {
      auto &io_context = _context->_io_context;

      // defer all in the context or we will crash!
      if (_socket.socket) {
        auto socket = _socket.socket;
        boost::asio::post(io_context, [socket]() {
          if (socket) {
            socket->close();
            delete socket;
          }
        });

        _socket.socket = nullptr;
        _socket.endpoint = nullptr;
      }
    }

    _contextStorage.reset();
    _context = nullptr;

    // clean context vars
    if (_socketVar) {
      releaseVariable(_socketVar);
      _socketVar = nullptr;
    }

    _blks.cleanup();
    _addr.cleanup();
    _port.cleanup();
  }

  void warmup(SHContext *context) {
    auto networkContext = context->anyStorage["Network.Context"].lock();
    if (!networkContext) {
      _contextStorage = std::make_shared<entt::any>(std::in_place_type_t<NetworkContext>{});
      context->anyStorage["Network.Context"] = _contextStorage;
      auto anyPtr = _contextStorage.get();
      _context = &entt::any_cast<NetworkContext &>(*anyPtr);
    } else {
      _contextStorage = networkContext;
      _context = entt::any_cast<NetworkContext *>(*networkContext);
    }

    _addr.warmup(context);
    _port.warmup(context);
    _blks.warmup(context);
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  SHTypeInfo compose(SHInstanceData &data) {
    // inject our special context vars
    auto endpointInfo = ExposedInfo::Variable("Network.Socket", SHCCSTR("The active socket."), SHTypeInfo(SocketInfo));
    shards::arrayPush(data.shared, endpointInfo);
    _blks.compose(data);
    return data.inputType;
  }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _addr = value;
      break;
    case 1:
      _port = value;
      break;
    case 2:
      _blks = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _addr;
    case 1:
      return _port;
    case 2:
      return _blks;
    default:
      return Var::Empty;
    }
  }

  struct Reader {
    char *buffer;
    size_t offset;
    size_t max;

    Reader(char *buf, size_t size) : buffer(buf), offset(0), max(size) {}

    void operator()(uint8_t *buf, size_t size) {
      auto newOffset = offset + size;
      if (newOffset > max) {
        throw ActivationError("Overflow requested");
      }
      memcpy(buf, buffer + offset, size);
      offset = newOffset;
    }
  };

  struct Writer {
    char *buffer;
    size_t offset;
    size_t max;

    Writer(char *buf, size_t size) : buffer(buf), offset(0), max(size) {}

    void operator()(const uint8_t *buf, size_t size) {
      auto newOffset = offset + size;
      if (newOffset > max) {
        throw ActivationError("Overflow requested");
      }
      memcpy(buffer + offset, buf, size);
      offset = newOffset;
    }
  };

  void setSocket(SHContext *context) {
    if (!_socketVar) {
      _socketVar = referenceVariable(context, "Network.Socket");
    }
    auto rc = _socketVar->refcount;
    auto flags = _socketVar->flags;
    *_socketVar = Var::Object(&_socket, CoreCC, SocketCC);
    _socketVar->refcount = rc;
    _socketVar->flags = flags;
  }
};

struct Server : public NetworkBase {
  struct ClientPkt {
    udp::endpoint *remote;
    SHVar payload;
  };

  boost::lockfree::queue<ClientPkt> _queue{16};
  boost::lockfree::queue<ClientPkt> _empty_queue{16};
  udp::endpoint _sender;

  void destroy() {
    // drain queues first
    while (!_queue.empty()) {
      ClientPkt pkt;
      if (_queue.pop(pkt)) {
        delete pkt.remote;
        destroyVar(pkt.payload);
      }
    }

    while (!_empty_queue.empty()) {
      ClientPkt pkt;
      if (_empty_queue.pop(pkt)) {
        delete pkt.remote;
        destroyVar(pkt.payload);
      }
    }
  }

  Serialization deserial;

  void do_receive() {
    _socket.socket->async_receive_from(boost::asio::buffer(&_recv_buffer().front(), _recv_buffer().size()), _sender,
                                       [this](boost::system::error_code ec, std::size_t bytes_recvd) {
                                         if (!ec && bytes_recvd > 0) {
                                           ClientPkt pkt{};

                                           // try reuse vars internal memory smartly
                                           if (!_empty_queue.empty()) {
                                             _empty_queue.pop(pkt);
                                             *pkt.remote = _sender;
                                           } else {
                                             pkt.remote = new udp::endpoint(_sender);
                                           }

                                           // deserialize from buffer
                                           Reader r(&_recv_buffer().front(), bytes_recvd);
                                           deserial.reset();
                                           deserial.deserialize(r, pkt.payload);

                                           // add ready packet to queue
                                           _queue.push(pkt);

                                           // keep receiving
                                           do_receive();
                                         }
                                       });
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    assert(_context);
    auto &io_context = _context->_io_context;

    if (!_socket.socket) {
      // first activation, let's init
      _socket.socket = new udp::socket(io_context, udp::endpoint(udp::v4(), _port.get().payload.intValue));

      // start receiving
      boost::asio::post(io_context, [this]() { do_receive(); });
    }

    setSocket(context);

    // receive from ring-buffer and run wires
    while (!_queue.empty()) {
      ClientPkt pkt;
      if (_queue.pop(pkt)) {
        SHVar output{};
        // update remote as pops in context variable
        _socket.endpoint = pkt.remote;
        activateShards(SHVar(_blks).payload.seqValue, context, pkt.payload, output);
        // release the var once done
        // will recycle internal buffers
        _empty_queue.push(pkt);
      }
    }

    return input;
  }
};

struct Client : public NetworkBase {
  ExposedInfo _exposedInfo{};

  boost::lockfree::queue<SHVar> _queue{16};
  boost::lockfree::queue<SHVar> _empty_queue{16};

  void destroy() {
    // drain queues first
    while (!_queue.empty()) {
      SHVar v;
      if (_queue.pop(v)) {
        destroyVar(v);
      }
    }

    while (!_empty_queue.empty()) {
      SHVar v;
      if (_empty_queue.pop(v)) {
        destroyVar(v);
      }
    }
  }

  Serialization deserial;

  void do_receive() {
    _socket.socket->async_receive_from(boost::asio::buffer(&_recv_buffer().front(), _recv_buffer().size()), _server,
                                       [this](boost::system::error_code ec, std::size_t bytes_recvd) {
                                         if (!ec && bytes_recvd > 0) {
                                           SHVar v{};

                                           // try reuse vars internal memory smartly
                                           if (!_empty_queue.empty()) {
                                             _empty_queue.pop(v);
                                           }

                                           // deserialize from buffer
                                           Reader r(&_recv_buffer().front(), bytes_recvd);
                                           deserial.reset();
                                           deserial.deserialize(r, v);

                                           // process a packet
                                           _queue.push(v);

                                           // keep receiving
                                           do_receive();
                                         }
                                       });
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    assert(_context);
    auto &io_context = _context->_io_context;

    if (!_socket.socket) {
      // first activation, let's init
      _socket.socket = new udp::socket(io_context, udp::endpoint(udp::v4(), 0));

      boost::asio::io_service io_service;
      udp::resolver resolver(io_service);
      auto sport = std::to_string(_port.get().payload.intValue);
      udp::resolver::query query(udp::v4(), _addr.get().payload.stringValue, sport);
      _server = *resolver.resolve(query);

      // start receiving
      boost::asio::post(io_context, [this]() { do_receive(); });
    }

    setSocket(context);
    // in the case of client we actually set the remote here
    _socket.endpoint = &_server;

    // receive from ringbuffer and run wires
    while (!_queue.empty()) {
      SHVar v;
      if (_queue.pop(v)) {
        SHVar output{};
        activateShards(SHVar(_blks).payload.seqValue, context, v, output);
        // release the var once done
        // will recycle internal buffers
        _empty_queue.push(v);
      }
    }

    return input;
  }

  udp::endpoint _server;
};

struct Send {
  // Must take an optional seq of SocketData, to be used properly by server
  // This way we get also a easy and nice broadcast

  ThreadShared<std::array<char, 0xFFFF>> _send_buffer;

  SHVar *_socketVar = nullptr;

  std::shared_ptr<entt::any> _contextStorage;
  NetworkContext *_context = nullptr;

  void cleanup() {
    _contextStorage.reset();
    _context = nullptr;

    // clean context vars
    if (_socketVar) {
      releaseVariable(_socketVar);
      _socketVar = nullptr;
    }
  }

  void warmup(SHContext *context) {
    auto networkContext = context->anyStorage["Network.Context"].lock();
    if (!networkContext) {
      throw WarmupError("Network.Context not set");
    } else {
      _contextStorage = networkContext;
      auto anyPtr = networkContext.get();
      _context = &entt::any_cast<NetworkContext &>(*anyPtr);
    }
  }

  SocketData *getSocket(SHContext *context) {
    if (!_socketVar) {
      _socketVar = referenceVariable(context, "Network.Socket");
    }
    assert(_socketVar->payload.objectVendorId == CoreCC);
    assert(_socketVar->payload.objectTypeId == SocketCC);
    return reinterpret_cast<SocketData *>(_socketVar->payload.objectValue);
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  Serialization serializer;

  SHVar activate(SHContext *context, const SHVar &input) {
    auto socket = getSocket(context);
    NetworkBase::Writer w(&_send_buffer().front(), _send_buffer().size());
    serializer.reset();
    auto size = serializer.serialize(input, w);
    socket->socket->async_send_to(boost::asio::buffer(&_send_buffer().front(), size), *socket->endpoint,
                                  [](boost::system::error_code ec, std::size_t bytes_sent) {
                                    if (ec) {
                                      std::cout << "Error sending: " << ec.message() << std::endl;
                                    }
                                  });
    return input;
  }
};
}; // namespace Network

void registerNetworkShards() {
  using namespace Network;
  REGISTER_SHARD("Network.Server", Server);
  REGISTER_SHARD("Network.Client", Client);
  REGISTER_SHARD("Network.Send", Send);
}
}; // namespace shards
