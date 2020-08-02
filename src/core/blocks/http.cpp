/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#ifndef CB_NO_HTTP_BLOCKS
#define BOOST_ERROR_CODE_HEADER_ONLY
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>

#include "blockwrapper.hpp"
#include "chainblocks.hpp"
#include "shared.hpp"
#include <exception>
#include <taskflow/taskflow.hpp>

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>
namespace ssl = net::ssl;       // from <boost/asio/ssl.hpp>
using tcp = net::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>

using namespace std;

inline std::string url_encode(const std::string &value) {
  std::ostringstream escaped;
  escaped.fill('0');
  escaped << hex;

  for (std::string::const_iterator i = value.begin(), n = value.end(); i != n;
       ++i) {
    std::string::value_type c = (*i);

    // Keep alphanumeric and other accepted characters intact
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      escaped << c;
      continue;
    }

    // Any other characters are percent-encoded
    escaped << std::uppercase;
    escaped << '%' << std::setw(2) << int((unsigned char)c);
    escaped << std::nouppercase;
  }

  return escaped.str();
}

namespace chainblocks {
namespace Http {
struct Client {
  constexpr static int version = 11;
  static inline Types InTypes{CoreInfo::NoneType, CoreInfo::StringTableType};
  static inline Parameters params{
      {"Host",
       "The remote host address or IP.",
       {CoreInfo::StringType, CoreInfo::StringVarType}},
      {"Target",
       "The remote host target path.",
       {CoreInfo::StringType, CoreInfo::StringVarType}},
      {"Port",
       "The remote host port.",
       {CoreInfo::IntType, CoreInfo::IntVarType}},
      {"Secure", "If the connection should be secured.", {CoreInfo::BoolType}},
      {"Headers",
       "The headers to attach to this request.",
       {CoreInfo::StringTableType, CoreInfo::StringVarTableType,
        CoreInfo::NoneType}}};

  static CBTypesInfo inputTypes() { return InTypes; }

  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }

  static CBParametersInfo parameters() { return params; }

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
    case 4:
      headers = value;
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
    case 4:
      return headers;
    default:
      return {};
    }
  }

  void connect(CBContext *context) {
    try {
      await(context, [&]() {
        if (ssl) {
          // Set SNI Hostname (many hosts need this to handshake
          // successfully)
          if (!SSL_set_tlsext_host_name(secure_stream.native_handle(),
                                        host.get().payload.stringValue)) {
            beast::error_code ec{static_cast<int>(::ERR_get_error()),
                                 net::error::get_ssl_category()};
            throw beast::system_error{ec};
          }
        }

        tcp::resolver resolver{ioc};
        auto resolved =
            resolver.resolve(host.get().payload.stringValue,
                             std::to_string(port.get().payload.intValue));

        if (ssl) {
          // Make the connection on the IP address we get from a lookup
          beast::get_lowest_layer(secure_stream).connect(resolved);
          // Perform the SSL handshake
          secure_stream.handshake(ssl::stream_base::client);
        } else {
          // Make the connection on the IP address we get from a lookup
          unsecure_stream.connect(resolved);
        }

        connected = true;
      });
    } catch (std::exception &ex) {
      // TODO some exceptions could be left unhandled
      // or anyway should be fatal
      LOG(WARNING) << "Http connection failed: " << ex.what();
      throw ActivationError("Http connection failed.");
    }
  }

  virtual void request(CBContext *context, const CBVar &input) = 0;

  void resetStream() {
    beast::error_code ec;
    secure_stream.shutdown(ec);
    unsecure_stream.close();
    secure_stream = beast::ssl_stream<beast::tcp_stream>(ioc, ctx);
    connected = false;
  }

  void cleanup() {
    resetStream();
    port.cleanup();
    host.cleanup();
    target.cleanup();
    headers.cleanup();
  }

  void warmup(CBContext *ctx) {
    port.warmup(ctx);
    host.warmup(ctx);
    target.warmup(ctx);
    headers.warmup(ctx);

    beast::get_lowest_layer(secure_stream)
        .expires_after(std::chrono::seconds(30));
    unsecure_stream.expires_after(std::chrono::seconds(30));
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (!connected)
      connect(context);

    request(context, input);

    return Var(res.body());
  }

protected:
  ParamVar port{Var(443)};
  ParamVar host{Var("www.example.com")};
  ParamVar target{Var("/")};
  ParamVar headers{};
  std::string vars;
  bool ssl = true;

  bool connected = false;

  // The io_context is required for all I/O
  net::io_context ioc;

  // The SSL context is required, and holds certificates
  ssl::context ctx{ssl::context::tlsv12_client};

  // These objects perform our I/O
  beast::ssl_stream<beast::tcp_stream> secure_stream{ioc, ctx};
  beast::tcp_stream unsecure_stream{ioc};

  // This buffer is used for reading and must be persisted
  beast::flat_buffer buffer;

  // Declare a container to hold the response
  http::response<http::string_body> res;
};

struct Get final : public Client {
  void request(CBContext *context, const CBVar &input) override {
    try {
      await(context, [&]() {
        vars.clear();
        vars.append(target.get().payload.stringValue);
        if (input.valueType == Table) {
          vars.append("?");
          ForEach(input.payload.tableValue, [&](auto key, auto &value) {
            vars.append(url_encode(key));
            vars.append("=");
            vars.append(url_encode(value.payload.stringValue));
            vars.append("&");
          });
          vars.resize(vars.size() - 1);
        }

        buffer.clear();
        res.clear();
        res.body().clear();

        // Set up an HTTP GET request message
        http::request<http::string_body> req{http::verb::get, vars, version};
        req.set(http::field::host, host.get().payload.stringValue);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        // add custom headers
        if (headers.get().valueType == Table) {
          auto htab = headers.get().payload.tableValue;
          ForEach(htab, [&](auto key, auto &value) {
            req.set(key, value.payload.stringValue);
          });
        }

        if (ssl) {
          // Send the HTTP request to the remote host
          http::write(secure_stream, req);

          // Receive the HTTP response
          http::read(secure_stream, buffer, res);
        } else {
          // Send the HTTP request to the remote host
          http::write(unsecure_stream, req);

          // Receive the HTTP response
          http::read(unsecure_stream, buffer, res);
        }
      });
    } catch (std::exception &ex) {
      // TODO some exceptions could be left unhandled
      // or anyway should be fatal
      LOG(WARNING) << "Http request failed: " << ex.what();
      resetStream();
      throw ActivationError("Http request failed.");
    }
  }
};

struct Post final : public Client {
  void request(CBContext *context, const CBVar &input) override {
    try {
      await(context, [&]() {
        vars.clear();
        if (input.valueType == Table) {
          ForEach(input.payload.tableValue, [&](auto key, auto &value) {
            vars.append(url_encode(key));
            vars.append("=");
            vars.append(url_encode(value.payload.stringValue));
            vars.append("&");
          });
          if (vars.size() > 0)
            vars.resize(vars.size() - 1);
        }

        buffer.clear();
        res.clear();
        res.body().clear();

        // Set up an HTTP GET request message
        http::request<http::string_body> req{
            http::verb::post, target.get().payload.stringValue, version};
        req.set(http::field::host, host.get().payload.stringValue);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req.set(http::field::content_type, "application/x-www-form-urlencoded");

        // add custom headers
        if (headers.get().valueType == Table) {
          auto htab = headers.get().payload.tableValue;
          ForEach(htab, [&](auto key, auto &value) {
            req.set(key, value.payload.stringValue);
          });
        }

        // add the body of the post
        req.body() = vars;

        if (ssl) {
          // Send the HTTP request to the remote host
          http::write(secure_stream, req);

          // Receive the HTTP response
          http::read(secure_stream, buffer, res);
        } else {
          // Send the HTTP request to the remote host
          http::write(unsecure_stream, req);

          // Receive the HTTP response
          http::read(unsecure_stream, buffer, res);
        }
      });
    } catch (std::exception &ex) {
      // TODO some exceptions could be left unhandled
      // or anyway should be fatal
      LOG(ERROR) << "Http request failed: " << ex.what();
      resetStream();
      throw ActivationError("Http request failed.");
    }
  }
};

void registerBlocks() {
  REGISTER_CBLOCK("Http.Get", Get);
  REGISTER_CBLOCK("Http.Post", Post);
}
} // namespace Http
} // namespace chainblocks
#else
namespace chainblocks {
namespace Http {
void registerBlocks() {}
} // namespace Http
} // namespace chainblocks
#endif
