/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

#ifndef SHARDS_NO_HTTP_SHARDS
#define BOOST_ERROR_CODE_HEADER_ONLY

#include "shared.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace shards {
namespace WS {
constexpr uint32_t WebSocketCC = 'webs';

struct Common {
  static inline Type WebSocket{{SHType::Object, {.object = {.vendorId = CoreCC, .typeId = WebSocketCC}}}};

  static inline Type WebSocketVar{{SHType::ContextVar, {.contextVarTypes = WebSocket}}};
};

struct Socket {
  Socket(bool secure) : _secure(secure) {}

  net::io_context ioCtx{};
  ssl::context secureCtx{ssl::context::tlsv12_client};
  websocket::stream<beast::ssl_stream<tcp::socket>> _ssl_socket{ioCtx, secureCtx};
  websocket::stream<tcp::socket> _socket{ioCtx};

  void close() {
    if (_socket.is_open()) {
      SHLOG_DEBUG("Closing WebSocket");
      try {
        _socket.close(websocket::close_code::normal);
      } catch (const std::exception &ex) {
        SHLOG_WARNING("Ignored an exception during WebSocket close: {}", ex.what());
      }
    }
    if (_ssl_socket.is_open()) {
      SHLOG_DEBUG("Closing WebSocket");
      try {
        _ssl_socket.close(websocket::close_code::normal);
      } catch (const std::exception &ex) {
        SHLOG_WARNING("Ignored an exception during WebSocket close: {}", ex.what());
      }
    }
  }

  ~Socket() { close(); }

  websocket::stream<beast::ssl_stream<tcp::socket>> &get_secure() {
    assert(_secure);
    return _ssl_socket;
  }

  websocket::stream<tcp::socket> &get_unsecure() {
    assert(!_secure);
    return _socket;
  }

  constexpr bool secure() const { return _secure; }

  bool _secure;
};

struct Client {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static SHParametersInfo parameters() {
    static Parameters params{
        {"Name", SHCCSTR("The name of this websocket instance."), {CoreInfo::StringType}},
        {"Host", SHCCSTR("The remote host address or IP."), {CoreInfo::StringType, CoreInfo::StringVarType}},
        {"Target", SHCCSTR("The remote host target path."), {CoreInfo::StringType, CoreInfo::StringVarType}},
        {"Port", SHCCSTR("The remote host port."), {CoreInfo::IntType, CoreInfo::IntVarType}},
        {"Secure", SHCCSTR("If the connection should be secured."), {CoreInfo::BoolType}},
    };
    return params;
  }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      name = value.payload.stringValue;
      break;
    case 1:
      host = value;
      break;
    case 2:
      target = value;
      break;
    case 3:
      port = value;
      break;
    case 4:
      ssl = value.payload.boolValue;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(name);
    case 1:
      return host;
    case 2:
      return target;
    case 3:
      return port;
    case 4:
      return Var(ssl);
    default:
      return {};
    }
  }

  void connect(SHContext *context) {
    try {
      boost::asio::io_context ioc;
      tcp::resolver resolver{ioc};
      await(
          context,
          [&] {
            boost::system::error_code error;
            auto resolved = resolver.resolve(host.get().payload.stringValue, std::to_string(port.get().payload.intValue), error);

            if (error) {
              throw ActivationError("Failed to resolve host");
            }

            SHLOG_TRACE("Websocket resolved remote host");

            // Make the connection on the IP address we get from a lookup
            auto ep = ssl ? net::connect(get_lowest_layer(ws->get_secure()), resolved)
                          : net::connect(ws->get_unsecure().next_layer(), resolved);
            std::string h = host.get().payload.stringValue;
            h += ':' + std::to_string(ep.port());

            SHLOG_TRACE("Websocket connected with the remote host");

            // Set a decorator to change the User-Agent of the handshake
            if (ssl) {
              ws->get_secure().set_option(websocket::stream_base::decorator([](websocket::request_type &req) {
                req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-coro");
              }));
            } else {
              ws->get_unsecure().set_option(websocket::stream_base::decorator([](websocket::request_type &req) {
                req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-coro");
              }));
            }

            websocket::stream_base::timeout timeouts{
                std::chrono::seconds(30), // handshake timeout
                std::chrono::seconds(30), // idle timeout
                true                      // send ping at half idle timeout
            };

            if (ssl)
              ws->get_secure().set_option(timeouts);
            else
              ws->get_unsecure().set_option(timeouts);

            if (ssl) {
              // Perform the SSL handshake
              ws->get_secure().next_layer().handshake(ssl::stream_base::client);
              SHLOG_TRACE("Websocket performed SSL handshake");
            }

            SHLOG_DEBUG("WebSocket handshake with: {}", h);

            // Perform the websocket handshake
            if (ssl)
              ws->get_secure().handshake(h, target.get().payload.stringValue);
            else
              ws->get_unsecure().handshake(h, target.get().payload.stringValue);

            SHLOG_TRACE("Websocket performed handshake");

            connected = true;
          },
          [&] {
            SHLOG_DEBUG("Cancelling WebSocket connection");
            resolver.cancel();
            ws->close();
          });
    } catch (const std::exception &ex) {
      SHLOG_WARNING("WebSocket connection failed: {}", ex.what());
      throw ActivationError("WebSocket connection failed.");
    }
  }

  void cleanup() {
    port.cleanup();
    host.cleanup();
    target.cleanup();

    if (socket) {
      releaseVariable(socket);
      ws = nullptr;
      socket = nullptr;
      connected = false;
    }
  }

  void warmup(SHContext *ctx) {
    port.warmup(ctx);
    host.warmup(ctx);
    target.warmup(ctx);

    ws = std::make_shared<Socket>(ssl);
    socket = referenceVariable(ctx, name.c_str());
  }

  SHExposedTypesInfo exposedVariables() {
    _expInfo = SHExposedTypeInfo{name.c_str(), SHCCSTR("The exposed websocket."), Common::WebSocket};
    _expInfo.isProtected = true;
    return SHExposedTypesInfo{&_expInfo, 1, 0};
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (!connected) {
      connect(context);
      socket->payload.objectVendorId = CoreCC;
      socket->payload.objectTypeId = WebSocketCC;
      socket->payload.objectValue = &ws;
    }

    return input;
  }

protected:
  std::string name;
  ParamVar port{Var(443)};
  ParamVar host{Var("echo.websocket.org")};
  ParamVar target{Var("/")};
  bool ssl = true;

  SHExposedTypeInfo _expInfo{};

  bool connected = false;
  SHVar *socket;

  // These objects perform our I/O
  std::shared_ptr<Socket> ws{nullptr};
};

struct User {
  std::shared_ptr<Socket> _ws;
  ParamVar _wsVar{};
  SHExposedTypeInfo _expInfo{};

  static SHParametersInfo parameters() {
    static Parameters params{{"Socket", SHCCSTR("The websocket instance variable."), {Common::WebSocketVar}}};
    return params;
  }

  void setParam(int index, const SHVar &value) { _wsVar = value; }

  SHVar getParam(int index) { return _wsVar; }

  void cleanup() {
    _wsVar.cleanup();
    _ws = nullptr;
  }

  void warmup(SHContext *context) { _wsVar.warmup(context); }

  void ensureSocket() {
    if (_ws == nullptr)
      _ws = *reinterpret_cast<std::shared_ptr<Socket> *>(_wsVar.get().payload.objectValue);
  }

  SHExposedTypesInfo requiredVariables() {
    if (_wsVar.isVariable()) {
      _expInfo = SHExposedTypeInfo{_wsVar.variableName(), SHCCSTR("The required websocket."), Common::WebSocket};
    } else {
      throw ComposeError("No websocket specified.");
    }
    return SHExposedTypesInfo{&_expInfo, 1, 0};
  }
};

struct WriteString : public User {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    ensureSocket();

    std::string_view payload{};

    if (input.payload.stringLen > 0) {
      payload = std::string_view(input.payload.stringValue, input.payload.stringLen);
    } else {
      payload = std::string_view(input.payload.stringValue);
    }

    try {
      await(
          context,
          [&]() {
            if (_ws->secure())
              _ws->get_secure().write(net::buffer(payload));
            else
              _ws->get_unsecure().write(net::buffer(payload));
          },
          [] {});
    } catch (const std::exception &ex) {
      SHLOG_WARNING("WebSocket write failed: {}", ex.what());
      throw ActivationError("WebSocket write failed.");
    }

    return input;
  }
};

struct ReadString : public User {
  beast::flat_buffer _buffer;
  std::string _output;

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    ensureSocket();

    _buffer.clear();

    try {
      await(
          context,
          [&]() {
            if (_ws->secure())
              _ws->get_secure().read(_buffer);
            else
              _ws->get_unsecure().read(_buffer);
          },
          [] {});
    } catch (const std::exception &ex) {
      SHLOG_WARNING("WebSocket read failed: {}", ex.what());
      throw ActivationError("WebSocket read failed.");
    }

    _output.clear();
    const auto data = _buffer.data();
    _output.reserve(data.size());
    _output.append(static_cast<char const *>(data.data()), data.size());
    return Var(_output);
  }
};

void registerShards() {
  REGISTER_SHARD("WebSocket.Client", Client);
  REGISTER_SHARD("WebSocket.WriteString", WriteString);
  REGISTER_SHARD("WebSocket.ReadString", ReadString);
}
} // namespace WS
} // namespace shards
#else
namespace shards {
namespace WS {
void registerShards() {}
} // namespace WS
} // namespace shards
#endif
