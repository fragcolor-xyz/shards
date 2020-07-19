/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#ifndef CB_NO_HTTP_BLOCKS
#define BOOST_ERROR_CODE_HEADER_ONLY

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
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

#include "chainblocks.hpp"
#include "shared.hpp"

namespace chainblocks {
namespace WS {
struct Client {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static CBParametersInfo parameters() {
    static Parameters params{{"Host",
                              "The remote host address or IP.",
                              {CoreInfo::StringType, CoreInfo::StringVarType}},
                             {"Target",
                              "The remote host target path.",
                              {CoreInfo::StringType, CoreInfo::StringVarType}},
                             {"Port",
                              "The remote host port.",
                              {CoreInfo::StringType, CoreInfo::StringVarType}},
                             {"Secure",
                              "If the connection should be secured.",
                              {CoreInfo::BoolType}}};
    return params;
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      host = value;
      break;
    case 1:
      target = value;
      break;
    case 2:
      port = value;
      break;
    case 3:
      ssl = value.payload.boolValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return host;
    case 1:
      return target;
    case 2:
      return port;
    case 3:
      return Var(ssl);
    default:
      return {};
    }
  }

  void connect(CBContext *context) {
    AsyncOp<InternalCore> op(context);
    try {
      auto &tasks = _taskManager();
      op.sidechain(tasks, [&]() {
        tcp::resolver resolver{ioc};
        auto resolved = resolver.resolve(host.get().payload.stringValue,
                                         port.get().payload.stringValue);

        // Make the connection on the IP address we get from a lookup
        auto ep = net::connect(get_lowest_layer(ws), resolved);
        std::string h = host.get().payload.stringValue;
        h += ':' + std::to_string(ep.port());

        if (ssl) {
          // Perform the SSL handshake
          ws.next_layer().handshake(ssl::stream_base::client);
        }

        // Set a decorator to change the User-Agent of the handshake
        ws.set_option(
            websocket::stream_base::decorator([](websocket::request_type &req) {
              req.set(http::field::user_agent,
                      std::string(BOOST_BEAST_VERSION_STRING) +
                          " websocket-client-coro");
            }));

        // Perform the websocket handshake
        ws.handshake(h, target.get().payload.stringValue);

        connected = true;
      });
    } catch (std::exception &ex) {
      // TODO some exceptions could be left unhandled
      // or anyway should be fatal
      LOG(WARNING) << "Http connection failed: " << ex.what();
      throw ActivationError("Http connection failed.");
    }
  }

  void cleanup() {
    if (connected)
      ws.close(websocket::close_code::normal);

    port.cleanup();
    host.cleanup();
    target.cleanup();
  }

  void warmup(CBContext *ctx) {
    port.warmup(ctx);
    host.warmup(ctx);
    target.warmup(ctx);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (!connected) {
      connect(context);
    }

    return input;
  }

protected:
  Shared<tf::Executor> _taskManager;

  ParamVar port{Var("443")};
  ParamVar host{Var("www.example.com")};
  ParamVar target{Var("/")};
  bool ssl = true;

  bool connected = false;

  // The io_context is required for all I/O
  net::io_context ioc;

  // The SSL context is required, and holds certificates
  ssl::context ctx{ssl::context::tlsv12_client};

  // These objects perform our I/O
  websocket::stream<beast::ssl_stream<tcp::socket>> ws{ioc, ctx};
};

void registerBlocks() { REGISTER_CBLOCK("WS.Client", Client); }
} // namespace WS
} // namespace chainblocks
#else
namespace chainblocks {
namespace WS {
void registerBlocks() {}
} // namespace WS
} // namespace chainblocks
#endif
