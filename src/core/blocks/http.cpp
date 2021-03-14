/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2021 Giovanni Petrantoni */

#ifndef CB_NO_HTTP_BLOCKS

#ifndef __EMSCRIPTEN__
#define BOOST_ERROR_CODE_HEADER_ONLY
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>

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
#else
#include <boost/algorithm/string.hpp>
#include <emscripten/fetch.h>
#endif

#include "chainblocks.hpp"
#include "shared.hpp"
#include <filesystem>

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
#ifdef __EMSCRIPTEN__
// those blocks for non emscripten platforms are implemented in Rust

struct Base {
  static inline Types FullStrOutputTypes{
      {CoreInfo::IntType, CoreInfo::StringTableType, CoreInfo::StringType}};
  static inline Types FullBytesOutputTypes{
      {CoreInfo::IntType, CoreInfo::StringTableType, CoreInfo::BytesType}};
  static inline std::array<CBString, 3> FullOutputKeys{"status", "headers",
                                                       "body"};
  static inline Type FullStrOutputType =
      Type::TableOf(FullStrOutputTypes, FullOutputKeys);
  static inline Type FullBytesOutputType =
      Type::TableOf(FullBytesOutputTypes, FullOutputKeys);

  static inline Parameters params{
      {"URL",
       CBCCSTR("The url to request to"),
       {CoreInfo::StringType, CoreInfo::StringVarType}},
      {"Headers",
       CBCCSTR("The headers to use for the request."),
       {CoreInfo::NoneType, CoreInfo::StringTableType,
        CoreInfo::StringVarTableType}},
      {"Timeout",
       CBCCSTR("How many seconds to wait for the request to complete."),
       {CoreInfo::IntType}},
      {"Bytes",
       CBCCSTR("If instead of a string the block should outout bytes."),
       {CoreInfo::BoolType}},
      {"FullResponse",
       CBCCSTR("If the output should be a table with the full response, "
               "including headers and status."),
       {CoreInfo::BoolType}}};
  static CBParametersInfo parameters() { return params; }

  CBTypesInfo outputTypes() {
    if (fullResponse) {
      if (asBytes) {
        return FullBytesOutputType;
      } else {
        return FullStrOutputType;
      }
    } else {
      if (asBytes) {
        return CoreInfo::BytesType;
      } else {
        return CoreInfo::StringType;
      }
    }
  }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      url = value;
      break;
    case 1:
      headers = value;
      break;
    case 2:
      timeout = int(value.payload.intValue);
      break;
    case 3:
      asBytes = value.payload.boolValue;
      break;
    case 4:
      fullResponse = value.payload.boolValue;
      if (fullResponse) {
        // also init lazily the map
        CBMap m;
        CBVar empty{};
        empty.valueType = CBType::Table;
        empty.payload.tableValue.opaque = &m;
        empty.payload.tableValue.api = &Globals::TableInterface;
        outMap["headers"] = empty;
      }
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return url;
    case 1:
      return headers;
    case 2:
      return Var(timeout);
    case 3:
      return Var(asBytes);
    case 4:
      return Var(fullResponse);
    default:
      throw InvalidParameterIndex();
    }
  }

  void warmup(CBContext *context) {
    url.warmup(context);
    headers.warmup(context);
  }

  void cleanup() {
    url.cleanup();
    headers.cleanup();
  }

  static void fetchSucceeded(emscripten_fetch_t *fetch) {
    auto self = reinterpret_cast<Base *>(fetch->userData);
    if (self->fullResponse) {
      const auto len = emscripten_fetch_get_response_headers_length(fetch);
      self->hbuffer.resize(len + 1); // well I think the + 1 is not needed
      emscripten_fetch_get_response_headers(fetch, self->hbuffer.data(),
                                            len + 1);
      auto &mvar = self->outMap["headers"];
      auto m = reinterpret_cast<CBMap *>(mvar.payload.tableValue.opaque);
      std::istringstream resp(self->hbuffer);
      std::string header;
      std::string::size_type index;
      while (std::getline(resp, header) && header != "\r") {
        index = header.find(':', 0);
        if (index != std::string::npos) {
          auto key = boost::algorithm::trim_copy(header.substr(0, index));
          boost::algorithm::to_lower(key);
          auto val = boost::algorithm::trim_copy(header.substr(index + 1));
          m->emplace(key, Var(val));
        }
      }
      self->status = fetch->status;
    }
    self->buffer.assign(fetch->data, fetch->numBytes);
    self->state = 1;
    emscripten_fetch_close(fetch);
  }

  static void fetchFailed(emscripten_fetch_t *fetch) {
    auto self = reinterpret_cast<Base *>(fetch->userData);
    self->buffer.assign(fetch->statusText);
    self->state = -1;
    emscripten_fetch_close(fetch); // Also free data on failure.
  }

  bool asBytes{false};
  bool fullResponse{false};
  uint16_t status;
  int state{0};
  int timeout{10};
  std::string buffer;
  std::string hbuffer;
  std::string vars;
  std::vector<const char *> headersCArray;
  CBMap outMap;
  ParamVar url{Var("")};
  ParamVar headers{};
};

template <const string_view &METHOD> struct GetLike : public Base {
  static inline Types InputTypes{
      {CoreInfo::NoneType, CoreInfo::StringTableType}};
  static CBTypesInfo inputTypes() { return InputTypes; }

  CBVar activate(CBContext *context, const CBVar &input) {
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, METHOD.data());
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = fetchSucceeded;
    attr.onerror = fetchFailed;
    attr.userData = this;
    attr.timeoutMSecs = timeout * 1000;

    vars.clear();
    vars.append(url.get().payload.stringValue);
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

    // add custom headers
    headersCArray.clear();
    if (headers.get().valueType == Table) {
      auto htab = headers.get().payload.tableValue;
      ForEach(htab, [&](auto key, auto &value) {
        headersCArray.emplace_back(key);
        headersCArray.emplace_back(value.payload.stringValue);
      });
    }
    headersCArray.emplace_back(nullptr);
    attr.requestHeaders = headersCArray.data();

    state = 0;
    buffer.clear();
    emscripten_fetch(&attr, vars.c_str());

    while (state == 0) {
      CB_SUSPEND(context, 0.0);
    }

    if (state == 1) {
      if (unlikely(fullResponse)) {
        outMap.emplace("status", Var(status));
        if (asBytes) {
          outMap.emplace("body", Var((uint8_t *)buffer.data(), buffer.size()));
        } else {
          outMap.emplace("body", Var(buffer));
        }
        CBVar res{};
        res.valueType = CBType::Table;
        res.payload.tableValue.opaque = &outMap;
        res.payload.tableValue.api = &Globals::TableInterface;
        return res;
      } else {
        if (asBytes) {
          return Var((uint8_t *)buffer.data(), buffer.size());
        } else {
          return Var(buffer);
        }
      }
    } else {
      LOG(ERROR) << "Http request failed with status: " << buffer;
      throw ActivationError("Http request failed");
    }
  }
};

constexpr string_view GET = "GET";
using Get = GetLike<GET>;
constexpr string_view HEAD = "HEAD";
using Head = GetLike<HEAD>;

template <const string_view &METHOD> struct PostLike : public Base {
  static inline Types InputTypes{{CoreInfo::NoneType, CoreInfo::StringTableType,
                                  CoreInfo::BytesType, CoreInfo::StringType}};
  static CBTypesInfo inputTypes() { return InputTypes; }

  CBVar activate(CBContext *context, const CBVar &input) {
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, METHOD.data());
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = fetchSucceeded;
    attr.onerror = fetchFailed;
    attr.userData = this;
    attr.timeoutMSecs = timeout * 1000;

    // add custom headers
    bool hasContentType = false;
    headersCArray.clear();
    if (headers.get().valueType == Table) {
      auto htab = headers.get().payload.tableValue;
      ForEach(htab, [&](auto key, auto &value) {
        std::string data(key);
        std::transform(data.begin(), data.end(), data.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (data == "content-type") {
          hasContentType = true;
        }
        headersCArray.emplace_back(key);
        headersCArray.emplace_back(value.payload.stringValue);
      });
    }

    if (input.valueType == CBType::String) {
      if (!hasContentType) {
        headersCArray.emplace_back("content-type");
        headersCArray.emplace_back("application/json");
      }
      attr.overriddenMimeType = "application/json";
      attr.requestData = input.payload.stringValue;
      attr.requestDataSize = CBSTRLEN(input);
    } else if (input.valueType == CBType::Bytes) {
      if (!hasContentType) {
        headersCArray.emplace_back("content-type");
        headersCArray.emplace_back("application/octet-stream");
      }
      attr.requestData = (const char *)input.payload.bytesValue;
      attr.requestDataSize = input.payload.bytesSize;
    } else if (input.valueType == CBType::Table) {
      if (!hasContentType) {
        headersCArray.emplace_back("content-type");
        headersCArray.emplace_back("application/x-www-form-urlencoded");
      }
      vars.clear();
      ForEach(input.payload.tableValue, [&](auto key, auto &value) {
        vars.append(url_encode(key));
        vars.append("=");
        vars.append(url_encode(value.payload.stringValue));
        vars.append("&");
      });
      vars.resize(vars.size() - 1);
      attr.requestData = vars.c_str();
      attr.requestDataSize = vars.size();
    }

    headersCArray.emplace_back(nullptr);
    attr.requestHeaders = headersCArray.data();

    state = 0;
    buffer.clear();
    emscripten_fetch(&attr, url.get().payload.stringValue);

    while (state == 0) {
      CB_SUSPEND(context, 0.0);
    }

    if (state == 1) {
      if (unlikely(fullResponse)) {
        outMap.emplace("status", Var(status));
        if (asBytes) {
          outMap.emplace("body", Var((uint8_t *)buffer.data(), buffer.size()));
        } else {
          outMap.emplace("body", Var(buffer));
        }
        CBVar res{};
        res.valueType = CBType::Table;
        res.payload.tableValue.opaque = &outMap;
        res.payload.tableValue.api = &Globals::TableInterface;
        return res;
      } else {
        if (asBytes) {
          return Var((uint8_t *)buffer.data(), buffer.size());
        } else {
          return Var(buffer);
        }
      }
    } else {
      LOG(ERROR) << "Http request failed with status: " << buffer;
      throw ActivationError("Http request failed");
    }
  }
};

constexpr string_view POST = "POST";
using Post = PostLike<POST>;
constexpr string_view PUT = "PUT";
using Put = PostLike<PUT>;
constexpr string_view PATCH = "PATCH";
using Patch = PostLike<PATCH>;
constexpr string_view DELETE = "DELETE";
using Delete = PostLike<DELETE>;

#else

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
       CBCCSTR("The chain that will be spawned and handle a remote request."),
       {CoreInfo::ChainOrNone}},
      {"Endpoint",
       CBCCSTR("The URL from where your service can be accessed by a client."),
       {CoreInfo::StringType}},
      {"Port",
       CBCCSTR("The port this service will use."),
       {CoreInfo::IntType}}};

  static CBParametersInfo parameters() { return params; }

  // bypass
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  void setParam(int idx, const CBVar &val) {
    switch (idx) {
    case 0: {
      _handlerMaster = val;
      if (_handlerMaster.valueType == CBType::Chain)
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
      {"Status",
       CBCCSTR("The HTTP status code to return."),
       {CoreInfo::IntType}},
      {"Headers",
       CBCCSTR("The headers to attach to this response."),
       {CoreInfo::StringTableType, CoreInfo::StringVarTableType,
        CoreInfo::NoneType}}};

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
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
        return true;
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

struct SendFile {
  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }

  static inline Parameters params{
      {"Headers",
       CBCCSTR("The headers to attach to this response."),
       {CoreInfo::StringTableType, CoreInfo::StringVarTableType,
        CoreInfo::NoneType}}};

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) { _headers = value; }

  CBVar getParam(int index) { return _headers; }

  void warmup(CBContext *context) {
    _headers.warmup(context);
    _peerVar = referenceVariable(context, "Http.Server.Socket");
  }

  void cleanup() {
    _headers.cleanup();
    releaseVariable(_peerVar);
    _peerVar = nullptr;
  }

  static boost::beast::string_view mime_type(boost::beast::string_view path) {
    using boost::beast::iequals;
    auto const ext = [&path] {
      auto const pos = path.rfind(".");
      if (pos == boost::beast::string_view::npos)
        return boost::beast::string_view{};
      return path.substr(pos);
    }();
    if (iequals(ext, ".htm"))
      return "text/html";
    if (iequals(ext, ".html"))
      return "text/html";
    if (iequals(ext, ".php"))
      return "text/html";
    if (iequals(ext, ".css"))
      return "text/css";
    if (iequals(ext, ".txt"))
      return "text/plain";
    if (iequals(ext, ".js"))
      return "application/javascript";
    if (iequals(ext, ".json"))
      return "application/json";
    if (iequals(ext, ".xml"))
      return "application/xml";
    if (iequals(ext, ".swf"))
      return "application/x-shockwave-flash";
    if (iequals(ext, ".flv"))
      return "video/x-flv";
    if (iequals(ext, ".png"))
      return "image/png";
    if (iequals(ext, ".jpe"))
      return "image/jpeg";
    if (iequals(ext, ".jpeg"))
      return "image/jpeg";
    if (iequals(ext, ".jpg"))
      return "image/jpeg";
    if (iequals(ext, ".gif"))
      return "image/gif";
    if (iequals(ext, ".bmp"))
      return "image/bmp";
    if (iequals(ext, ".ico"))
      return "image/vnd.microsoft.icon";
    if (iequals(ext, ".tiff"))
      return "image/tiff";
    if (iequals(ext, ".tif"))
      return "image/tiff";
    if (iequals(ext, ".svg"))
      return "image/svg+xml";
    if (iequals(ext, ".svgz"))
      return "image/svg+xml";
    if (iequals(ext, ".wasm"))
      return "application/wasm";
    return "application/text";
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto peer = reinterpret_cast<Peer *>(_peerVar->payload.objectValue);
    _response.clear();

    std::filesystem::path p{Globals::RootPath};
    p += input.payload.stringValue;

    http::file_body::value_type file;
    boost::beast::error_code ec;
    auto pstr = p.generic_string();
    file.open(pstr.c_str(), boost::beast::file_mode::read, ec);
    if (unlikely(bool(ec))) {
      _404_response.clear();
      _404_response.result(http::status::not_found);
      _404_response.body() = "File not found.";
      _404_response.prepare_payload();

      http::async_write(*peer->socket, _404_response,
                        [&](beast::error_code ec, std::size_t nbytes) {
                          if (ec) {
                            throw PeerError{ec, peer};
                          }
                        });

      return input;
    } else {
      _response.result(http::status::ok);
      _response.set(http::field::content_type,
                    mime_type(input.payload.stringValue));
      _response.body() = std::move(file);

      // add custom headers
      if (_headers.get().valueType == Table) {
        auto htab = _headers.get().payload.tableValue;
        ForEach(htab, [&](auto key, auto &value) {
          _response.set(key, value.payload.stringValue);
          return true;
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
  }

  http::status _status{200};
  CBVar *_peerVar{nullptr};
  ParamVar _headers{};
  http::response<http::file_body> _response;
  http::response<http::string_body> _404_response;
};
#endif

void registerBlocks() {
#ifdef __EMSCRIPTEN__
  REGISTER_CBLOCK("Http.Get", Get);
  REGISTER_CBLOCK("Http.Head", Head);
  REGISTER_CBLOCK("Http.Post", Post);
  REGISTER_CBLOCK("Http.Put", Put);
  REGISTER_CBLOCK("Http.Patch", Patch);
  REGISTER_CBLOCK("Http.Delete", Delete);
#else
  REGISTER_CBLOCK("Http.Server", Server);
  REGISTER_CBLOCK("Http.Read", Read);
  REGISTER_CBLOCK("Http.Response", Response);
  REGISTER_CBLOCK("Http.SendFile", SendFile);
#endif
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
