/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

#ifndef CHAINBLOCKS_NO_HTTP_BLOCKS
#define BOOST_ERROR_CODE_HEADER_ONLY

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

#include "shared.hpp"

namespace chainblocks {
namespace WS {
constexpr uint32_t WebSocketCC = 'webs';

struct Common {
  static inline Type WebSocket{
      {CBType::Object,
       {.object = {.vendorId = CoreCC, .typeId = WebSocketCC}}}};

  static inline Type WebSocketVar{
      {CBType::ContextVar, {.contextVarTypes = WebSocket}}};
};

struct Socket {
  Socket(bool secure) : _secure(secure) {}

  net::io_context ioCtx{};
  ssl::context secureCtx{ssl::context::tlsv12_client};
  websocket::stream<beast::ssl_stream<tcp::socket>> _ssl_socket{ioCtx,
                                                                secureCtx};
  websocket::stream<tcp::socket> _socket{ioCtx};

  void close() {
    if (_socket.is_open()) {
      CBLOG_DEBUG("Closing WebSocket");
      try {
        _socket.close(websocket::close_code::normal);
      } catch (const std::exception &ex) {
        CBLOG_WARNING("Ignored an exception during WebSocket close: {}",
                      ex.what());
      }
    }
    if (_ssl_socket.is_open()) {
      CBLOG_DEBUG("Closing WebSocket");
      try {
        _ssl_socket.close(websocket::close_code::normal);
      } catch (const std::exception &ex) {
        CBLOG_WARNING("Ignored an exception during WebSocket close: {}",
                      ex.what());
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
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static CBParametersInfo parameters() {
    static Parameters params{{"Name",
                              CBCCSTR("The name of this websocket instance."),
                              {CoreInfo::StringType}},
                             {"Host",
                              CBCCSTR("The remote host address or IP."),
                              {CoreInfo::StringType, CoreInfo::StringVarType}},
                             {"Target",
                              CBCCSTR("The remote host target path."),
                              {CoreInfo::StringType, CoreInfo::StringVarType}},
                             {"Port",
                              CBCCSTR("The remote host port."),
                              {CoreInfo::IntType, CoreInfo::IntVarType}},
                             {"Secure",
                              CBCCSTR("If the connection should be secured."),
                              {CoreInfo::BoolType}}};
    return params;
  }

  void setParam(int index, const CBVar &value) {
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

  CBVar getParam(int index) {
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

  void connect(CBContext *context) {
    try {
      boost::asio::io_context ioc;
      tcp::resolver resolver{ioc};
      await(
          context,
          [&] {
            boost::system::error_code error;
            auto resolved = resolver.resolve(
                host.get().payload.stringValue,
                std::to_string(port.get().payload.intValue), error);

            if (error) {
              throw ActivationError("Failed to resolve host");
            }

            CBLOG_TRACE("Websocket resolved remote host");

            // Make the connection on the IP address we get from a lookup
            auto ep =
                ssl ? net::connect(get_lowest_layer(ws->get_secure()), resolved)
                    : net::connect(ws->get_unsecure().next_layer(), resolved);
            std::string h = host.get().payload.stringValue;
            h += ':' + std::to_string(ep.port());

            CBLOG_TRACE("Websocket connected with the remote host");

            // Set a decorator to change the User-Agent of the handshake
            if (ssl) {
              ws->get_secure().set_option(websocket::stream_base::decorator(
                  [](websocket::request_type &req) {
                    req.set(http::field::user_agent,
                            std::string(BOOST_BEAST_VERSION_STRING) +
                                " websocket-client-coro");
                  }));
            } else {
              ws->get_unsecure().set_option(websocket::stream_base::decorator(
                  [](websocket::request_type &req) {
                    req.set(http::field::user_agent,
                            std::string(BOOST_BEAST_VERSION_STRING) +
                                " websocket-client-coro");
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
              CBLOG_TRACE("Websocket performed SSL handshake");
            }

            CBLOG_DEBUG("WebSocket handshake with: {}", h);

            // Perform the websocket handshake
            if (ssl)
              ws->get_secure().handshake(h, target.get().payload.stringValue);
            else
              ws->get_unsecure().handshake(h, target.get().payload.stringValue);

            CBLOG_TRACE("Websocket performed handshake");

            connected = true;
          },
          [&] {
            CBLOG_DEBUG("Cancelling WebSocket connection");
            resolver.cancel();
            ws->close();
          });
    } catch (const std::exception &ex) {
      // TODO some exceptions could be left unhandled
      // or anyway should be fatal
      CBLOG_WARNING("WebSocket connection failed: {}", ex.what());
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

  void warmup(CBContext *ctx) {
    port.warmup(ctx);
    host.warmup(ctx);
    target.warmup(ctx);

    ws = std::make_shared<Socket>(ssl);
    socket = referenceVariable(ctx, name.c_str());
  }

  CBExposedTypesInfo exposedVariables() {
    _expInfo = CBExposedTypeInfo{
        name.c_str(), CBCCSTR("The exposed websocket."), Common::WebSocket};
    _expInfo.isProtected = true;
    return CBExposedTypesInfo{&_expInfo, 1, 0};
  }

  CBVar activate(CBContext *context, const CBVar &input) {
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

  CBExposedTypeInfo _expInfo{};

  bool connected = false;
  CBVar *socket;

  // These objects perform our I/O
  std::shared_ptr<Socket> ws{nullptr};
};

struct User {
  std::shared_ptr<Socket> _ws;
  ParamVar _wsVar{};
  CBExposedTypeInfo _expInfo{};

  static CBParametersInfo parameters() {
    static Parameters params{{"Socket",
                              CBCCSTR("The websocket instance variable."),
                              {Common::WebSocketVar}}};
    return params;
  }

  void setParam(int index, const CBVar &value) { _wsVar = value; }

  CBVar getParam(int index) { return _wsVar; }

  void cleanup() {
    _wsVar.cleanup();
    _ws = nullptr;
  }

  void warmup(CBContext *context) { _wsVar.warmup(context); }

  void ensureSocket() {
    if (_ws == nullptr)
      _ws = *reinterpret_cast<std::shared_ptr<Socket> *>(
          _wsVar.get().payload.objectValue);
  }

  CBExposedTypesInfo requiredVariables() {
    if (_wsVar.isVariable()) {
      _expInfo = CBExposedTypeInfo{_wsVar.variableName(),
                                   CBCCSTR("The required websocket."),
                                   Common::WebSocket};
    } else {
      throw ComposeError("No websocket specified.");
    }
    return CBExposedTypesInfo{&_expInfo, 1, 0};
  }
};

struct WriteString : public User {
  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    ensureSocket();

    std::string_view payload{};

    if (input.payload.stringLen > 0) {
      payload =
          std::string_view(input.payload.stringValue, input.payload.stringLen);
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
      // TODO some exceptions could be left unhandled
      // or anyway should be fatal
      CBLOG_WARNING("WebSocket write failed: {}", ex.what());
      throw ActivationError("WebSocket write failed.");
    }

    return input;
  }
};

struct ReadString : public User {
  beast::flat_buffer _buffer;
  std::string _output;

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    // TODO refactor, we are not awaiting here...
    // either refactor Send of await as well

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
      CBLOG_WARNING("WebSocket read failed: {}", ex.what());
      throw ActivationError("WebSocket read failed.");
    }

    _output.clear();
    const auto data = _buffer.data();
    _output.reserve(data.size());
    _output.append(static_cast<char const *>(data.data()), data.size());
    return Var(_output);
  }
};

void registerBlocks() {
  REGISTER_CBLOCK("WebSocket.Client", Client);
  REGISTER_CBLOCK("WebSocket.WriteString", WriteString);
  REGISTER_CBLOCK("WebSocket.ReadString", ReadString);
}
} // namespace WS
} // namespace chainblocks
#else
namespace chainblocks {
namespace WS {
void registerBlocks() {}
} // namespace WS
} // namespace chainblocks
#endif
