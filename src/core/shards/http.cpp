/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

#ifndef SHARDS_NO_HTTP_SHARDS

#ifndef __EMSCRIPTEN__
#define BOOST_ERROR_CODE_HEADER_ONLY
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;
namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>
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

#include "shared.hpp"

using namespace std;

inline std::string url_encode(const std::string_view &value) {
  std::ostringstream escaped;
  escaped.fill('0');
  escaped << hex;

  for (auto i = value.begin(), n = value.end(); i != n; ++i) {
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

namespace shards {
namespace Http {
#ifdef __EMSCRIPTEN__
// those shards for non emscripten platforms are implemented in Rust

struct Base {
  static inline Types FullStrOutputTypes{{CoreInfo::IntType, CoreInfo::StringTableType, CoreInfo::StringType}};
  static inline Types FullBytesOutputTypes{{CoreInfo::IntType, CoreInfo::StringTableType, CoreInfo::BytesType}};
  static inline std::array<SHString, 3> FullOutputKeys{"status", "headers", "body"};
  static inline Type FullStrOutputType = Type::TableOf(FullStrOutputTypes, FullOutputKeys);
  static inline Type FullBytesOutputType = Type::TableOf(FullBytesOutputTypes, FullOutputKeys);

  static inline Parameters params{
      {"URL", SHCCSTR("The url to request to"), {CoreInfo::StringType, CoreInfo::StringVarType}},
      {"Headers",
       SHCCSTR("The headers to use for the request."),
       {CoreInfo::NoneType, CoreInfo::StringTableType, CoreInfo::StringVarTableType}},
      {"Timeout", SHCCSTR("How many seconds to wait for the request to complete."), {CoreInfo::IntType}},
      {"Bytes", SHCCSTR("If instead of a string the shard should outout bytes."), {CoreInfo::BoolType}},
      {"FullResponse",
       SHCCSTR("If the output should be a table with the full response, "
               "including headers and status."),
       {CoreInfo::BoolType}}};
  static SHParametersInfo parameters() { return params; }

  SHTypesInfo outputTypes() {
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

  void setParam(int index, const SHVar &value) {
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
        SHMap m;
        SHVar empty{};
        empty.valueType = SHType::Table;
        empty.payload.tableValue.opaque = &m;
        empty.payload.tableValue.api = &GetGlobals().TableInterface;
        outMap["headers"] = empty;
      }
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
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

  void warmup(SHContext *context) {
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
      emscripten_fetch_get_response_headers(fetch, self->hbuffer.data(), len + 1);
      auto &mvar = self->outMap["headers"];
      auto m = reinterpret_cast<SHMap *>(mvar.payload.tableValue.opaque);
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
  SHMap outMap;
  ParamVar url{Var("")};
  ParamVar headers{};
};

template <const string_view &METHOD> struct GetLike : public Base {
  static inline Types InputTypes{{CoreInfo::NoneType, CoreInfo::StringTableType}};
  static SHTypesInfo inputTypes() { return InputTypes; }

  SHVar activate(SHContext *context, const SHVar &input) {
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
        auto sv_value = SHSTRVIEW(value);
        vars.append(url_encode(key));
        vars.append("=");
        vars.append(url_encode(sv_value));
        vars.append("&");
      });
      vars.resize(vars.size() - 1);
    }

    // add custom headers
    headersCArray.clear();
    if (headers.get().valueType == Table) {
      auto htab = headers.get().payload.tableValue;
      ForEach(htab, [&](auto key, auto &value) {
        auto sv_value = SHSTRVIEW(value);
        headersCArray.emplace_back(key);
        headersCArray.emplace_back(sv_value.data());
      });
    }
    headersCArray.emplace_back(nullptr);
    attr.requestHeaders = headersCArray.data();

    state = 0;
    buffer.clear();
    emscripten_fetch(&attr, vars.c_str());

    while (state == 0) {
      SH_SUSPEND(context, 0.0);
    }

    if (state == 1) {
      if (unlikely(fullResponse)) {
        outMap.emplace("status", Var(status));
        if (asBytes) {
          outMap.emplace("body", Var((uint8_t *)buffer.data(), buffer.size()));
        } else {
          outMap.emplace("body", Var(buffer));
        }
        SHVar res{};
        res.valueType = SHType::Table;
        res.payload.tableValue.opaque = &outMap;
        res.payload.tableValue.api = &GetGlobals().TableInterface;
        return res;
      } else {
        if (asBytes) {
          return Var((uint8_t *)buffer.data(), buffer.size());
        } else {
          return Var(buffer);
        }
      }
    } else {
      SHLOG_ERROR("Http request failed with status: {}", buffer);
      throw ActivationError("Http request failed");
    }
  }
};

constexpr string_view GET = "GET";
using Get = GetLike<GET>;
constexpr string_view HEAD = "HEAD";
using Head = GetLike<HEAD>;

template <const string_view &METHOD> struct PostLike : public Base {
  static inline Types InputTypes{{CoreInfo::NoneType, CoreInfo::StringTableType, CoreInfo::BytesType, CoreInfo::StringType}};
  static SHTypesInfo inputTypes() { return InputTypes; }

  SHVar activate(SHContext *context, const SHVar &input) {
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
        std::transform(data.begin(), data.end(), data.begin(), [](unsigned char c) { return std::tolower(c); });
        if (data == "content-type") {
          hasContentType = true;
        }
        headersCArray.emplace_back(key);
        headersCArray.emplace_back(value.payload.stringValue);
      });
    }

    if (input.valueType == SHType::String) {
      if (!hasContentType) {
        headersCArray.emplace_back("content-type");
        headersCArray.emplace_back("application/json");
      }
      attr.overriddenMimeType = "application/json";
      attr.requestData = input.payload.stringValue;
      attr.requestDataSize = SHSTRLEN(input);
    } else if (input.valueType == SHType::Bytes) {
      if (!hasContentType) {
        headersCArray.emplace_back("content-type");
        headersCArray.emplace_back("application/octet-stream");
      }
      attr.requestData = (const char *)input.payload.bytesValue;
      attr.requestDataSize = input.payload.bytesSize;
    } else if (input.valueType == SHType::Table) {
      if (!hasContentType) {
        headersCArray.emplace_back("content-type");
        headersCArray.emplace_back("application/x-www-form-urlencoded");
      }
      vars.clear();
      ForEach(input.payload.tableValue, [&](auto key, auto &value) {
        auto sv_value = SHSTRVIEW(value);
        vars.append(url_encode(key));
        vars.append("=");
        vars.append(url_encode(sv_value));
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
      SH_SUSPEND(context, 0.0);
    }

    if (state == 1) {
      if (unlikely(fullResponse)) {
        outMap.emplace("status", Var(status));
        if (asBytes) {
          outMap.emplace("body", Var((uint8_t *)buffer.data(), buffer.size()));
        } else {
          outMap.emplace("body", Var(buffer));
        }
        SHVar res{};
        res.valueType = SHType::Table;
        res.payload.tableValue.opaque = &outMap;
        res.payload.tableValue.api = &GetGlobals().TableInterface;
        return res;
      } else {
        if (asBytes) {
          return Var((uint8_t *)buffer.data(), buffer.size());
        } else {
          return Var(buffer);
        }
      }
    } else {
      SHLOG_ERROR("Http request failed with status: {}", buffer);
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
  static inline Type Info{{SHType::Object, {.object = {.vendorId = CoreCC, .typeId = PeerCC}}}};

  std::shared_ptr<SHWire> wire;
  std::shared_ptr<tcp::socket> socket;
};

struct PeerError {
  std::string_view source;
  beast::error_code ec;
  Peer *peer;
};

struct Server {
  static inline Parameters params{
      {"Handler", SHCCSTR("The wire that will be spawned and handle a remote request."), {CoreInfo::WireOrNone}},
      {"Endpoint", SHCCSTR("The URL from where your service can be accessed by a client."), {CoreInfo::StringType}},
      {"Port", SHCCSTR("The port this service will use."), {CoreInfo::IntType}}};

  static SHParametersInfo parameters() { return params; }

  // bypass
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  void setParam(int idx, const SHVar &val) {
    switch (idx) {
    case 0: {
      _handlerMaster = val;
      if (_handlerMaster.valueType == SHType::Wire)
        _pool.reset(new WireDoppelgangerPool<Peer>(_handlerMaster.payload.wireValue));
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

  SHVar getParam(int idx) {
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

  SHTypeInfo compose(const SHInstanceData &data) {
    const IterableExposedInfo shared(data.shared);
    // copy shared
    _sharedCopy = shared;
    return data.inputType;
  }

  // "Loop" forever accepting new connections.
  void accept_once(SHContext *context) {
    auto peer = _pool->acquire(_composer);
    peer->wire->onStop.clear(); // we have a fresh recycled wire here
    std::weak_ptr<Peer> weakPeer(peer);
    peer->wire->onStop.emplace_back([this, weakPeer]() {
      if (auto p = weakPeer.lock())
        _pool->release(p);
    });
    peer->socket.reset(new tcp::socket(*_ioc));
    _acceptor->async_accept(*peer->socket, [context, peer, this](beast::error_code ec) {
      if (!ec) {
        auto mesh = context->main->mesh.lock();
        if (mesh) {
          peer->wire->variables["Http.Server.Socket"] = Var::Object(peer.get(), CoreCC, Peer::PeerCC);
          mesh->schedule(peer->wire, Var::Empty, false);
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

  void warmup(SHContext *context) {
    if (!_pool) {
      throw ComposeError("Peer wires pool not valid!");
    }

    _ioc.reset(new net::io_context());
    auto addr = net::ip::make_address(_endpoint);
    _acceptor.reset(new tcp::acceptor(*_ioc, {addr, _port}));
    _composer.context = context;
    // start accepting
    accept_once(context);
  }

  void cleanup() { _pool->stopAll(); }

  SHVar activate(SHContext *context, const SHVar &input) {
    try {
      _ioc->poll();
    } catch (PeerError pe) {
      SHLOG_DEBUG("Http request error: {} from {} - closing connection.", pe.ec.message(), pe.source);
      stop(pe.peer->wire.get());
    }
    return input;
  }

  struct Composer {
    Server &server;
    SHContext *context;

    void compose(SHWire *wire) {
      SHInstanceData data{};
      data.inputType = CoreInfo::StringType;
      data.shared = server._sharedCopy;
      data.wire = context->wireStack.back();
      wire->mesh = context->main->mesh;
      auto res = composeWire(
          wire,
          [](const struct Shard *errorShard, const char *errorTxt, SHBool nonfatalWarning, void *userData) {
            if (!nonfatalWarning) {
              SHLOG_ERROR(errorTxt);
              throw ActivationError("Http.Server handler wire compose failed");
            } else {
              SHLOG_WARNING(errorTxt);
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
  std::unique_ptr<WireDoppelgangerPool<Peer>> _pool;
  IterableExposedInfo _sharedCopy;
  Composer _composer{*this};

  // The io_context is required for all I/O
  std::unique_ptr<net::io_context> _ioc;
  std::deque<Peer> _peers;
  std::unique_ptr<tcp::acceptor> _acceptor;
};

struct Read {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringTableType; }

  void warmup(SHContext *context) {
    _peerVar = referenceVariable(context, "Http.Server.Socket");
    if (_peerVar->valueType == SHType::None) {
      throw WarmupError("Socket variable not found in wire");
    }
  }

  void cleanup() {
    releaseVariable(_peerVar);
    _peerVar = nullptr;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    assert(_peerVar->valueType == Object);
    assert(_peerVar->payload.objectValue);
    auto peer = reinterpret_cast<Peer *>(_peerVar->payload.objectValue);

    bool done = false;
    request.body().clear();
    request.clear();
    buffer.clear();
    http::async_read(*peer->socket, buffer, request, [&, peer](beast::error_code ec, std::size_t nbytes) {
      if (ec) {
        // notice there is likehood of done not being valid
        // anymore here
        throw PeerError{"Read", ec, peer};
      } else {
        done = true;
      }
    });

    // we suspend here, that's why we captured & above!!
    while (!done) {
      SH_SUSPEND(context, 0.0);
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

    auto res = SHVar();
    res.valueType = Table;
    res.payload.tableValue.opaque = &_output;
    res.payload.tableValue.api = &GetGlobals().TableInterface;
    return res;
  }

  SHVar *_peerVar{nullptr};
  SHMap _output;
  beast::flat_buffer buffer{8192};
  http::request<http::string_body> request;
};

struct Response {
  static inline Types PostInTypes{CoreInfo::StringType};

  static SHTypesInfo inputTypes() { return PostInTypes; }
  static SHTypesInfo outputTypes() { return PostInTypes; }

  static inline Parameters params{{"Status", SHCCSTR("The HTTP status code to return."), {CoreInfo::IntType}},
                                  {"Headers",
                                   SHCCSTR("The headers to attach to this response."),
                                   {CoreInfo::StringTableType, CoreInfo::StringVarTableType, CoreInfo::NoneType}}};

  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) {
    if (index == 0)
      _status = http::status(value.payload.intValue);
    else
      _headers = value;
  }

  SHVar getParam(int index) {
    if (index == 0)
      return Var(int64_t(_status));
    else
      return _headers;
  }

  void warmup(SHContext *context) {
    _headers.warmup(context);
    _peerVar = referenceVariable(context, "Http.Server.Socket");
    if (_peerVar->valueType == SHType::None) {
      throw WarmupError("Socket variable not found in wire");
    }
  }

  void cleanup() {
    _headers.cleanup();
    releaseVariable(_peerVar);
    _peerVar = nullptr;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    assert(_peerVar->valueType == Object);
    assert(_peerVar->payload.objectValue);
    auto peer = reinterpret_cast<Peer *>(_peerVar->payload.objectValue);
    _response.clear();

    _response.result(_status);
    _response.set(http::field::content_type, "application/json");
    auto input_view = SHSTRVIEW(input);
    _response.body() = input_view;

    // add custom headers
    if (_headers.get().valueType == Table) {
      auto htab = _headers.get().payload.tableValue;
      ForEach(htab, [&](auto key, auto &value) {
        _response.set(key, value.payload.stringValue);
        return true;
      });
    }

    _response.prepare_payload();

    bool done = false;
    http::async_write(*peer->socket, _response, [&, peer](beast::error_code ec, std::size_t nbytes) {
      if (ec) {
        throw PeerError{"Response", ec, peer};
      } else {
        SHLOG_TRACE("Response: async_write bytes: {}", nbytes);
        done = true;
      }
    });

    // we suspend here, that's why we captured & above!!
    while (!done) {
      SH_SUSPEND(context, 0.0);
    }

    return input;
  }

  http::status _status{200};
  SHVar *_peerVar{nullptr};
  ParamVar _headers{};
  http::response<http::string_body> _response;
};

struct SendFile {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  static inline Parameters params{{"Headers",
                                   SHCCSTR("The headers to attach to this response."),
                                   {CoreInfo::StringTableType, CoreInfo::StringVarTableType, CoreInfo::NoneType}}};

  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) { _headers = value; }

  SHVar getParam(int index) { return _headers; }

  void warmup(SHContext *context) {
    _headers.warmup(context);
    _peerVar = referenceVariable(context, "Http.Server.Socket");
    if (_peerVar->valueType == SHType::None) {
      throw WarmupError("Socket variable not found in wire");
    }
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

  SHVar activate(SHContext *context, const SHVar &input) {
    assert(_peerVar->valueType == Object);
    assert(_peerVar->payload.objectValue);
    auto peer = reinterpret_cast<Peer *>(_peerVar->payload.objectValue);

    fs::path p{GetGlobals().RootPath};
    p += input.payload.stringValue;

    http::file_body::value_type file;
    boost::beast::error_code ec;
    auto pstr = p.generic_string();
    bool done = false;
    file.open(pstr.c_str(), boost::beast::file_mode::read, ec);
    if (unlikely(bool(ec))) {
      _404_response.clear();
      _404_response.result(http::status::not_found);
      _404_response.body() = "File not found.";
      _404_response.prepare_payload();

      http::async_write(*peer->socket, _404_response, [&, peer](beast::error_code ec, std::size_t nbytes) {
        if (ec) {
          throw PeerError{"SendFile:1", ec, peer};
        } else {
          SHLOG_TRACE("SendFile:1: async_write bytes: {}", nbytes);
          done = true;
        }
      });
    } else {
      _response.clear();
      _response.result(http::status::ok);
      _response.set(http::field::content_type, mime_type(input.payload.stringValue));
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

      http::async_write(*peer->socket, _response, [&, peer](beast::error_code ec, std::size_t nbytes) {
        if (ec) {
          throw PeerError{"SendFile:2", ec, peer};
        } else {
          SHLOG_TRACE("SendFile:2: async_write bytes: {}", nbytes);
          done = true;
        }
      });
    }

    // we suspend here, that's why we captured & above!!
    while (!done) {
      SH_SUSPEND(context, 0.0);
    }

    return input;
  }

  SHVar *_peerVar{nullptr};
  ParamVar _headers{};
  http::response<http::file_body> _response;
  http::response<http::string_body> _404_response;
};
#endif

struct EncodeURI {
  std::string _output;
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }
  SHVar activate(SHContext *context, const SHVar &input) {
    auto str = SHSTRVIEW(input);
    _output = url_encode(str);
    return Var(_output);
  }
};

void registerShards() {
#ifdef __EMSCRIPTEN__
  REGISTER_SHARD("Http.Get", Get);
  REGISTER_SHARD("Http.Head", Head);
  REGISTER_SHARD("Http.Post", Post);
  REGISTER_SHARD("Http.Put", Put);
  REGISTER_SHARD("Http.Patch", Patch);
  REGISTER_SHARD("Http.Delete", Delete);
#else
  REGISTER_SHARD("Http.Server", Server);
  REGISTER_SHARD("Http.Read", Read);
  REGISTER_SHARD("Http.Response", Response);
  REGISTER_SHARD("Http.SendFile", SendFile);
#endif
  REGISTER_SHARD("String.EncodeURI", EncodeURI);
}
} // namespace Http
} // namespace shards
#else
namespace shards {
namespace Http {
void registerShards() {}
} // namespace Http
} // namespace shards
#endif
