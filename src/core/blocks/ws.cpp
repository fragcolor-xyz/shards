/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#ifndef CB_NO_HTTP_BLOCKS
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
  net::io_context ioCtx{};
  ssl::context secureCtx{ssl::context::tlsv12_client};
  websocket::stream<beast::ssl_stream<tcp::socket>> socket{ioCtx, secureCtx};

  void close() {
    if (socket.is_open()) {
      LOG(DEBUG) << "Closing WebSocket";
      try {
        socket.close(websocket::close_code::normal);
      } catch (const std::exception &ex) {
        LOG(WARNING) << "Ignored an exception during WebSocket close: "
                     << ex.what();
      }
    }
  }

  ~Socket() { close(); }

  websocket::stream<beast::ssl_stream<tcp::socket>> &get() { return socket; }
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
      await(context, [&] {
        boost::asio::io_context ioc;
        tcp::resolver resolver{ioc};
        auto resolved =
            resolver.resolve(host.get().payload.stringValue,
                             std::to_string(port.get().payload.intValue));

        // Make the connection on the IP address we get from a lookup
        auto ep = net::connect(get_lowest_layer(ws->get()), resolved);
        std::string h = host.get().payload.stringValue;
        h += ':' + std::to_string(ep.port());

        if (ssl) {
          // Perform the SSL handshake
          ws->get().next_layer().handshake(ssl::stream_base::client);
        }

        // Set a decorator to change the User-Agent of the handshake
        ws->get().set_option(
            websocket::stream_base::decorator([](websocket::request_type &req) {
              req.set(http::field::user_agent,
                      std::string(BOOST_BEAST_VERSION_STRING) +
                          " websocket-client-coro");
            }));

        websocket::stream_base::timeout timeouts{
            std::chrono::seconds(30), // handshake timeout
            std::chrono::seconds(30), // idle timeout
            true                      // send ping at half idle timeout
        };
        ws->get().set_option(timeouts);

        LOG(DEBUG) << "WebSocket handshake with: " << h;

        // Perform the websocket handshake
        ws->get().handshake(h, target.get().payload.stringValue);

        connected = true;
      });
    } catch (const std::exception &ex) {
      // TODO some exceptions could be left unhandled
      // or anyway should be fatal
      LOG(WARNING) << "WebSocket connection failed: " << ex.what();
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

    ws = std::make_shared<Socket>();
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

    ws->ioCtx.poll();

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
      await(context, [&]() { _ws->get().write(net::buffer(payload)); });
    } catch (const std::exception &ex) {
      // TODO some exceptions could be left unhandled
      // or anyway should be fatal
      LOG(WARNING) << "WebSocket write failed: " << ex.what();
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

    // poll once, if we have data we will be done quick
    // we become the pollers in this case
    _ws->ioCtx.poll();

    bool done = false;
    boost::system::error_code done_err{};
    _ws->get().async_read(_buffer, [&done, &done_err](auto err, auto size) {
      done_err = err;
      done = true;
    });

    while (!done) {
      // we become the pollers in this case
      _ws->ioCtx.poll();
      CB_SUSPEND(context, 0);
    }

    if (done_err.failed()) {
      auto errorMsg = done_err.message();
      throw ActivationError(errorMsg);
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
