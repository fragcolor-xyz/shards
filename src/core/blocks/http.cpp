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
                                //
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

  static CBTypesInfo inputTypes() {
    static Types t{CoreInfo::NoneType, CoreInfo::StringTableType};
    return t;
  }

  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }

  static CBParametersInfo parameters() {
    static Parameters params{{"Host",
                              "The remote host address or IP.",
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

  void connect(CBContext *context, AsyncOp<InternalCore> &op) {
    try {
      op.sidechain<tf::Taskflow>(Tasks, [&]() {
        if (ssl) {
          // Set SNI Hostname (many hosts need this to handshake
          // successfully)
          if (!SSL_set_tlsext_host_name(stream.native_handle(),
                                        host.get().payload.stringValue)) {
            beast::error_code ec{static_cast<int>(::ERR_get_error()),
                                 net::error::get_ssl_category()};
            throw beast::system_error{ec};
          }
        }

        resolved = resolver.resolve(host.get().payload.stringValue,
                                    port.get().payload.stringValue);

        // Make the connection on the IP address we get from a lookup
        beast::get_lowest_layer(stream).connect(resolved);

        if (ssl) {
          // Perform the SSL handshake
          stream.handshake(ssl::stream_base::client);
        }

        connected = true;
      });
    } catch (ChainCancellation &) {
      throw;
    } catch (std::exception &ex) {
      // TODO some exceptions could be left unhandled
      // or anyway should be fatal
      LOG(ERROR) << "Http connection failed, pausing chain half a second "
                    "before restart, exception: "
                 << ex.what();
      suspend(context, 0.5);
      throw ActivationError("Http connection failed, restarting chain.",
                            CBChainState::Restart, false);
    }
  }

  virtual void request(CBContext *context, AsyncOp<InternalCore> &op,
                       const CBVar &input) = 0;

  void resetStream() {
    beast::error_code ec;
    stream.shutdown(ec);
    stream = beast::ssl_stream<beast::tcp_stream>(ioc, ctx);
    connected = false;
  }

  void cleanup() { resetStream(); }

  CBVar activate(CBContext *context, const CBVar &input) {
    AsyncOp<InternalCore> op(context);

    if (!connected)
      connect(context, op);

    request(context, op, input);

    return Var(res.body());
  }

protected:
#if 1
  tf::Executor &Tasks{Singleton<tf::Executor>::value};
#else
  static inline tf::Executor Tasks{1};
#endif

  ParamVar port{Var("443")};
  ParamVar host{Var("www.example.com")};
  ParamVar target{Var("/")};
  std::string vars;
  bool ssl = true;

  bool connected = false;

  // The io_context is required for all I/O
  net::io_context ioc;

  // The SSL context is required, and holds certificates
  ssl::context ctx{ssl::context::tlsv12_client};

  // These objects perform our I/O
  tcp::resolver resolver{ioc};
  beast::ssl_stream<beast::tcp_stream> stream{ioc, ctx};

  tcp::resolver::results_type resolved;

  // This buffer is used for reading and must be persisted
  beast::flat_buffer buffer;

  // Declare a container to hold the response
  http::response<http::string_body> res;
};

struct Get final : public Client {
  void request(CBContext *context, AsyncOp<InternalCore> &op,
               const CBVar &input) override {
    try {
      op.sidechain<tf::Taskflow>(Tasks, [&]() {
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

        // Send the HTTP request to the remote host
        http::write(stream, req);

        // Receive the HTTP response
        http::read(stream, buffer, res);
      });
    } catch (ChainCancellation &) {
      throw;
    } catch (std::exception &ex) {
      // TODO some exceptions could be left unhandled
      // or anyway should be fatal
      LOG(ERROR) << "Http request failed, pausing chain half a second "
                    "before restart, exception: "
                 << ex.what();

      resetStream();

      suspend(context, 0.5);
      throw ActivationError("Http request failed, restarting chain.",
                            CBChainState::Restart, false);
    }
  }
};

struct Post final : public Client {
  void request(CBContext *context, AsyncOp<InternalCore> &op,
               const CBVar &input) override {
    try {
      op.sidechain<tf::Taskflow>(Tasks, [&]() {
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
        req.body() = vars;

        // Send the HTTP request to the remote host
        http::write(stream, req);

        // Receive the HTTP response
        http::read(stream, buffer, res);
      });
    } catch (ChainCancellation &) {
      throw;
    } catch (std::exception &ex) {
      // TODO some exceptions could be left unhandled
      // or anyway should be fatal
      LOG(ERROR) << "Http request failed, pausing chain half a second "
                    "before restart, exception: "
                 << ex.what();

      resetStream();

      suspend(context, 0.5);
      throw ActivationError("Http request failed, restarting chain.",
                            CBChainState::Restart, false);
    }
  }
};

void registerBlocks() {
  REGISTER_CBLOCK("Http.Get", Get);
  REGISTER_CBLOCK("Http.Post", Post);
}
} // namespace Http
} // namespace chainblocks
