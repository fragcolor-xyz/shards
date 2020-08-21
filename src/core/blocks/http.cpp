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
#include <deque>
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

template <http::verb VERB> struct PostLike final : public Client {
  static inline Types PostInTypes{CoreInfo::NoneType, CoreInfo::StringTableType,
                                  CoreInfo::StringType};

  static CBTypesInfo inputTypes() { return PostInTypes; }

  static const char *help() {
    return "If the input is a table it will default to "
           "application/x-www-form-urlencoded, if it's a string will be "
           "application/json instead";
  }

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
        } else if (input.valueType == String) {
          vars.append(input.payload.stringValue);
        }

        buffer.clear();
        res.clear();
        res.body().clear();

        // Set up an HTTP GET request message
        http::request<http::string_body> req{
            VERB, target.get().payload.stringValue, version};
        req.set(http::field::host, host.get().payload.stringValue);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        if (input.valueType == Table) {
          req.set(http::field::content_type,
                  "application/x-www-form-urlencoded");
        } else if (input.valueType == String) {
          req.set(http::field::content_type, "application/json");
        }

        // add custom headers
        if (headers.get().valueType == Table) {
          auto htab = headers.get().payload.tableValue;
          ForEach(htab, [&](auto key, auto &value) {
            req.set(key, value.payload.stringValue);
          });
        }

        // add the body of the post
        req.body() = vars;
        req.prepare_payload();

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

using Post = PostLike<http::verb::post>;
using Put = PostLike<http::verb::put>;

struct Peer : public std::enable_shared_from_this<Peer> {
  static constexpr uint32_t PeerCC = 'httP';
  static inline Type Info{
      {CBType::Object, {.object = {.vendorId = CoreCC, .typeId = PeerCC}}}};

  std::shared_ptr<CBChain> chain;
  std::shared_ptr<tcp::socket> socket;
};

struct PeerError {
  beast::error_code ec;
  Peer *peer;
};

struct Server {
  static inline Parameters params{
      {"Handler",
       "The chain that will be spawned and handle a remote request.",
       {CoreInfo::ChainType}},
      {"Endpoint",
       "The URL from where your service can be accessed by a client.",
       {CoreInfo::StringType}},
      {"Port", "The port this service will use.", {CoreInfo::IntType}}};

  static CBParametersInfo parameters() { return params; }

  // bypass
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  void setParam(int idx, CBVar val) {
    switch (idx) {
    case 0: {
      _handlerMaster = val;
      _pool.reset(
          new ChainDoppelgangerPool<Peer>(_handlerMaster.payload.chainValue));
    } break;
    case 1:
      _endpoint = val.payload.stringValue;
      break;
    case 2:
      _port = uint16_t(val.payload.intValue);
      break;
    default:
      break;
    }
  }

  CBVar getParam(int idx) {
    switch (idx) {
    case 0:
      return _handlerMaster;
    case 1:
      return Var(_endpoint);
    case 2:
      return Var(int(_port));
    default:
      return Var::Empty;
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    const IterableExposedInfo shared(data.shared);
    // copy shared
    _sharedCopy = shared;
    return data.inputType;
  }

  // "Loop" forever accepting new connections.
  void accept_once(CBContext *context) {
    auto peer = _pool->acquire(_composer);
    peer->socket.reset(new tcp::socket(*_ioc));
    _acceptor->async_accept(
        *peer->socket, [context, peer, this](beast::error_code ec) {
          if (!ec) {
            auto node = context->main->node.lock();
            if (node) {
              peer->chain->variables["Http.Server.Socket"] =
                  Var::Object(peer.get(), CoreCC, Peer::PeerCC);
              node->schedule(peer->chain, Var::Empty, false);
            } else {
              _pool->release(peer);
            }
          } else {
            _pool->release(peer);
          }
          // continue accepting the next
          accept_once(context);
        });
  }

  void warmup(CBContext *context) {
    if (!_pool) {
      throw ComposeError("Peer chains pool not valid!");
    }

    _ioc.reset(new net::io_context());
    auto addr = net::ip::make_address(_endpoint);
    _acceptor.reset(new tcp::acceptor(*_ioc, {addr, _port}));
    _composer.context = context;
    // start accepting
    accept_once(context);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    try {
      _ioc->poll();
    } catch (PeerError pe) {
      LOG(INFO) << "Http request error: " << pe.ec.message();
      stop(pe.peer->chain.get());
      _pool->release(pe.peer->shared_from_this());
    }
    return input;
  }

  struct Composer {
    Server &server;
    CBContext *context;

    void compose(CBChain *chain) {
      CBInstanceData data{};
      data.inputType = CoreInfo::StringType;
      data.shared = server._sharedCopy;
      data.chain = context->chainStack.back();
      chain->node = context->main->node;
      auto res = composeChain(
          chain,
          [](const struct CBlock *errorBlock, const char *errorTxt,
             CBBool nonfatalWarning, void *userData) {
            if (!nonfatalWarning) {
              LOG(ERROR) << errorTxt;
              throw ActivationError("Http.Server handler chain compose failed");
            } else {
              LOG(WARNING) << errorTxt;
            }
          },
          nullptr, data);
      arrayFree(res.exposedInfo);
      arrayFree(res.requiredInfo);
    }
  };

  uint16_t _port{7070};
  std::string _endpoint{"0.0.0.0"};
  OwnedVar _handlerMaster{};
  std::unique_ptr<ChainDoppelgangerPool<Peer>> _pool;
  IterableExposedInfo _sharedCopy;
  Composer _composer{*this};

  // The io_context is required for all I/O
  std::unique_ptr<net::io_context> _ioc;
  std::deque<Peer> _peers;
  std::unique_ptr<tcp::acceptor> _acceptor;
};

struct Read {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringTableType; }

  void warmup(CBContext *context) {
    _peerVar = referenceVariable(context, "Http.Server.Socket");
  }

  void cleanup() {
    releaseVariable(_peerVar);
    _peerVar = nullptr;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto peer = reinterpret_cast<Peer *>(_peerVar->payload.objectValue);

    bool done = false;
    request.clear();
    buffer.clear();
    http::async_read(*peer->socket, buffer, request,
                     [&](beast::error_code ec, std::size_t nbytes) {
                       if (ec) {
                         throw PeerError{ec, peer};
                       }
                       done = true;
                     });

    while (!done) {
      CB_SUSPEND(context, 0.0);
    }

    switch (request.method()) {
    case http::verb::get:
      _output["method"] = Var("GET", 3);
      break;
    case http::verb::post:
      _output["method"] = Var("POST", 4);
      break;
    case http::verb::put:
      _output["method"] = Var("PUT", 3);
      break;
    case http::verb::delete_:
      _output["method"] = Var("DELETE", 6);
      break;
    default:
      throw ActivationError("Unsupported HTTP method.");
    }

    auto target = request.target();
    _output["target"] = Var(target.data(), target.size());

    _output["body"] = Var(request.body());

    auto res = CBVar();
    res.valueType = Table;
    res.payload.tableValue.opaque = &_output;
    res.payload.tableValue.api = &Globals::TableInterface;
    return res;
  }

  CBVar *_peerVar{nullptr};
  CBMap _output;
  beast::flat_buffer buffer{8192};
  http::request<http::string_body> request;
};

struct Response {
  static inline Types PostInTypes{CoreInfo::StringType};

  static CBTypesInfo inputTypes() { return PostInTypes; }
  static CBTypesInfo outputTypes() { return PostInTypes; }

  static inline Parameters params{
      {"Status", "The HTTP status code to return.", {CoreInfo::IntType}},
      {"Headers",
       "The headers to attach to this request.",
       {CoreInfo::StringTableType, CoreInfo::StringVarTableType,
        CoreInfo::NoneType}}};

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, CBVar value) {
    if (index == 0)
      _status = http::status(value.payload.intValue);
    else
      _headers = value;
  }

  CBVar getParam(int index) {
    if (index == 0)
      return Var(int64_t(_status));
    else
      return _headers;
  }

  void warmup(CBContext *context) {
    _headers.warmup(context);
    _peerVar = referenceVariable(context, "Http.Server.Socket");
  }

  void cleanup() {
    _headers.cleanup();
    releaseVariable(_peerVar);
    _peerVar = nullptr;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto peer = reinterpret_cast<Peer *>(_peerVar->payload.objectValue);
    _response.clear();

    _response.result(_status);
    _response.set(http::field::content_type, "application/json");
    _response.body() = input.payload.stringValue;

    // add custom headers
    if (_headers.get().valueType == Table) {
      auto htab = _headers.get().payload.tableValue;
      ForEach(htab, [&](auto key, auto &value) {
        _response.set(key, value.payload.stringValue);
      });
    }

    _response.prepare_payload();

    http::async_write(*peer->socket, _response,
                      [&](beast::error_code ec, std::size_t nbytes) {
                        if (ec) {
                          throw PeerError{ec, peer};
                        }
                      });
    return input;
  }

  http::status _status{200};
  CBVar *_peerVar{nullptr};
  ParamVar _headers{};
  http::response<http::string_body> _response;
};

void registerBlocks() {
  REGISTER_CBLOCK("Http.Get", Get);
  REGISTER_CBLOCK("Http.Post", Post);
  REGISTER_CBLOCK("Http.Put", Put);
  REGISTER_CBLOCK("Http.Server", Server);
  REGISTER_CBLOCK("Http.Read", Read);
  REGISTER_CBLOCK("Http.Response", Response);
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
