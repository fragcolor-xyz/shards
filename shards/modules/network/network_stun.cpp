/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2025 Fragcolor Pte. Ltd. */

#include <boost/asio.hpp>
#include <shards/core/shared.hpp>
#include <shards/core/foundation.hpp>
#include <shards/shards.hpp>
#include <shards/shardwrapper.hpp>
#include <shards/utility.hpp>
#include <memory>
#include <stdint.h>
#include <thread>
#include <random>
#include <array>
#include <atomic>
#include "log.hpp"

namespace shards {
namespace Network {

static inline auto logger = getLogger();
using boost::asio::ip::udp;

// STUN Message Types (RFC 5389)
constexpr uint16_t STUN_BINDING_REQUEST = 0x0001;
constexpr uint16_t STUN_BINDING_RESPONSE = 0x0101;
constexpr uint16_t STUN_BINDING_ERROR_RESPONSE = 0x0111;

// STUN Attributes
constexpr uint16_t STUN_ATTR_MAPPED_ADDRESS = 0x0001;
constexpr uint16_t STUN_ATTR_XOR_MAPPED_ADDRESS = 0x0020;
constexpr uint16_t STUN_ATTR_ERROR_CODE = 0x0009;

// STUN Magic Cookie (RFC 5389)
constexpr uint32_t STUN_MAGIC_COOKIE = 0x2112A442;

#pragma pack(push, 1)
struct STUNHeader {
  uint16_t messageType;
  uint16_t messageLength;
  uint32_t magicCookie;
  std::array<uint8_t, 12> transactionId;
};

struct STUNAttribute {
  uint16_t type;
  uint16_t length;
  // data follows
};

struct STUNMappedAddress {
  uint8_t reserved;
  uint8_t family;
  uint16_t port;
  uint32_t address;
};
#pragma pack(pop)

struct STUNResult {
  bool success = false;
  std::string externalIP;
  uint16_t externalPort = 0;
  std::string errorMessage;
};

struct STUNShard {
  static inline std::array<SHVar, 4> resultKeys{
      shards::Var("success"),       //
      shards::Var("external-ip"),   //
      shards::Var("external-port"), //
      shards::Var("error"),         //
  };
  static inline shards::Types resultTypes{
      shards::CoreInfo::BoolType,   //
      shards::CoreInfo::StringType, //
      shards::CoreInfo::IntType,    //
      shards::CoreInfo::StringType, //
  };
  static inline shards::Type resultType = shards::Type::TableOf(resultTypes, resultKeys);

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return resultType; }

  static SHOptionalString help() {
    return SHCCSTR(
        "Performs STUN (Session Traversal Utilities for NAT) binding request to discover external IP and port mapping. "
        "Returns a table with 'success', 'externalIP', 'externalPort', and 'error' fields.");
  }

  static SHOptionalString inputHelp() { return DefaultHelpText::InputHelpIgnored; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("Table containing STUN binding result with external IP/port information.");
  }

  ParamVar _stunServer{Var("stun.l.google.com")};
  ParamVar _stunPort{Var(19302)};
  ParamVar _timeout{Var(5000)};

  // State for async operation
  std::unique_ptr<boost::asio::io_context> _ioContext;
  std::unique_ptr<std::thread> _ioThread;
  std::atomic<bool> _responseReady{false};
  STUNResult _result;
  std::mutex _resultMutex;

  static inline Parameters params{{"Server", SHCCSTR("The STUN server hostname or IP address."), {CoreInfo::StringOrStringVar}},
                                  {"Port", SHCCSTR("The STUN server port."), {CoreInfo::IntOrIntVar}},
                                  {"Timeout", SHCCSTR("Timeout in milliseconds for STUN request."), {CoreInfo::IntOrIntVar}}};

  static SHParametersInfo parameters() { return SHParametersInfo(params); }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _stunServer = value;
      break;
    case 1:
      _stunPort = value;
      break;
    case 2:
      _timeout = value;
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _stunServer;
    case 1:
      return _stunPort;
    case 2:
      return _timeout;
    default:
      return Var::Empty;
    }
  }

  SHTypeInfo compose(const SHInstanceData &data) { return outputTypes().elements[0]; }

  void warmup(SHContext *context) {
    _stunServer.warmup(context);
    _stunPort.warmup(context);
    _timeout.warmup(context);
  }

  void cleanup(SHContext *context) {
    if (_ioContext) {
      _ioContext->stop();
    }
    if (_ioThread && _ioThread->joinable()) {
      _ioThread->join();
    }
    _ioContext.reset();
    _ioThread.reset();

    _stunServer.cleanup(context);
    _stunPort.cleanup(context);
    _timeout.cleanup(context);
  }

private:
  std::array<uint8_t, 12> generateTransactionId() {
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    std::array<uint8_t, 12> id;
    std::uniform_int_distribution<unsigned int> dis(0, 255);
    for (auto &byte : id) {
      byte = static_cast<uint8_t>(dis(gen));
    }
    return id;
  }

  std::vector<uint8_t> createBindingRequest() {
    std::vector<uint8_t> packet(20); // STUN header size
    STUNHeader *header = reinterpret_cast<STUNHeader *>(packet.data());

    header->messageType = htons(STUN_BINDING_REQUEST);
    header->messageLength = htons(0); // No attributes for basic binding request
    header->magicCookie = htonl(STUN_MAGIC_COOKIE);
    header->transactionId = generateTransactionId();

    return packet;
  }

  STUNResult parseBindingResponse(const std::vector<uint8_t> &data, const std::array<uint8_t, 12> &expectedTxId) {
    STUNResult result;

    if (data.size() < 20) {
      result.errorMessage = "Response too short";
      return result;
    }

    const STUNHeader *header = reinterpret_cast<const STUNHeader *>(data.data());

    // Verify magic cookie
    if (ntohl(header->magicCookie) != STUN_MAGIC_COOKIE) {
      result.errorMessage = "Invalid magic cookie";
      return result;
    }

    // Verify transaction ID
    if (header->transactionId != expectedTxId) {
      result.errorMessage = "Transaction ID mismatch";
      return result;
    }

    uint16_t msgType = ntohs(header->messageType);
    uint16_t msgLength = ntohs(header->messageLength);

    if (msgType == STUN_BINDING_ERROR_RESPONSE) {
      result.errorMessage = "STUN server returned error";
      return result;
    }

    if (msgType != STUN_BINDING_RESPONSE) {
      result.errorMessage = "Unexpected message type";
      return result;
    }

    // Parse attributes
    size_t offset = 20;
    while (offset + 4 <= data.size() && offset < 20 + msgLength) {
      const STUNAttribute *attr = reinterpret_cast<const STUNAttribute *>(data.data() + offset);
      uint16_t attrType = ntohs(attr->type);
      uint16_t attrLength = ntohs(attr->length);

      if (offset + 4 + attrLength > data.size()) {
        break; // Malformed attribute
      }

      if (attrType == STUN_ATTR_XOR_MAPPED_ADDRESS && attrLength >= 8) {
        const STUNMappedAddress *mappedAddr = reinterpret_cast<const STUNMappedAddress *>(data.data() + offset + 4);

        if (mappedAddr->family == 1) { // IPv4
          // XOR with magic cookie for XOR-MAPPED-ADDRESS
          uint32_t xorAddr = ntohl(mappedAddr->address) ^ STUN_MAGIC_COOKIE;
          uint16_t xorPort = ntohs(mappedAddr->port) ^ (STUN_MAGIC_COOKIE >> 16);

          result.externalIP = boost::asio::ip::address_v4(xorAddr).to_string();
          result.externalPort = xorPort;
          result.success = true;
          return result;
        }
      } else if (attrType == STUN_ATTR_MAPPED_ADDRESS && attrLength >= 8) {
        const STUNMappedAddress *mappedAddr = reinterpret_cast<const STUNMappedAddress *>(data.data() + offset + 4);

        if (mappedAddr->family == 1) { // IPv4
          result.externalIP = boost::asio::ip::address_v4(ntohl(mappedAddr->address)).to_string();
          result.externalPort = ntohs(mappedAddr->port);
          result.success = true;
          return result;
        }
      }

      // Move to next attribute (with padding)
      offset += 4 + ((attrLength + 3) & ~3);
    }

    result.errorMessage = "No mapped address found in response";
    return result;
  }

  void performSTUNRequest(const std::string &stunServer, uint16_t stunPort, std::chrono::milliseconds timeout) {
    try {
      // Resolve STUN server
      udp::resolver resolver(*_ioContext);
      auto endpoints = resolver.resolve(stunServer, std::to_string(stunPort));

      if (endpoints.empty()) {
        std::lock_guard<std::mutex> lock(_resultMutex);
        _result.errorMessage = "Failed to resolve STUN server";
        _responseReady = true;
        return;
      }

      udp::endpoint serverEndpoint = *endpoints.begin();

      // Create socket
      udp::socket socket(*_ioContext);
      socket.open(udp::v4());

      // Create binding request
      auto request = createBindingRequest();
      auto txId = reinterpret_cast<const STUNHeader *>(request.data())->transactionId;

      // Send request
      socket.send_to(boost::asio::buffer(request), serverEndpoint);

      // Setup timeout timer
      boost::asio::steady_timer timer(*_ioContext);
      timer.expires_after(timeout);
      timer.async_wait([this](boost::system::error_code ec) {
        if (!ec && !_responseReady.load()) {
          std::lock_guard<std::mutex> lock(_resultMutex);
          _result.errorMessage = "Timeout waiting for STUN response";
          _responseReady = true;
        }
      });

      // Wait for response
      auto responseBuffer = std::make_shared<std::vector<uint8_t>>(1024);
      auto responseEndpoint = std::make_shared<udp::endpoint>();

      socket.async_receive_from(boost::asio::buffer(*responseBuffer), *responseEndpoint,
                                [this, responseBuffer, txId, &timer](boost::system::error_code ec, std::size_t bytesReceived) {
                                  timer.cancel(); // Cancel timeout

                                  std::lock_guard<std::mutex> lock(_resultMutex);
                                  if (!ec && bytesReceived > 0) {
                                    responseBuffer->resize(bytesReceived);
                                    _result = parseBindingResponse(*responseBuffer, txId);
                                  } else {
                                    _result.errorMessage = ec ? ec.message() : "No response received";
                                  }
                                  _responseReady = true;
                                });

      // Run the io_context
      _ioContext->run();

    } catch (const std::exception &e) {
      std::lock_guard<std::mutex> lock(_resultMutex);
      _result.errorMessage = std::string("STUN error: ") + e.what();
      _responseReady = true;
    }
  }

  TableVar _resultTable;

public:
  SHVar activate(SHContext *context, const SHVar &input) {
    std::string server = SHSTRING_PREFER_SHSTRVIEW(_stunServer.get());
    uint16_t port = static_cast<uint16_t>(_stunPort.get().payload.intValue);
    auto timeoutMs = std::chrono::milliseconds(_timeout.get().payload.intValue);

    // Reset state
    _responseReady = false;
    _result = STUNResult{};

    // Create new io_context and thread for this request
    _ioContext = std::make_unique<boost::asio::io_context>();
    _ioThread = std::make_unique<std::thread>([this, server, port, timeoutMs]() { performSTUNRequest(server, port, timeoutMs); });

    // Suspend until response is ready
    while (!_responseReady.load()) {
      SH_SUSPEND(context, 0);
    }

    // Wait for thread to complete
    if (_ioThread && _ioThread->joinable()) {
      _ioThread->join();
    }

    // Get result
    STUNResult result;
    {
      std::lock_guard<std::mutex> lock(_resultMutex);
      result = _result;
    }

    // Create result table
    _resultTable.clear();
    _resultTable["success"] = Var(result.success);

    if (result.success) {
      _resultTable["external-ip"] = Var(result.externalIP);
      _resultTable["external-port"] = Var(static_cast<int64_t>(result.externalPort));
      _resultTable["error"] = Var("");
    } else {
      _resultTable["external-ip"] = Var("");
      _resultTable["external-port"] = Var(0);
      _resultTable["error"] = Var(result.errorMessage);
    }

    return Var(_resultTable);
  }
};

} // namespace Network
} // namespace shards

SHARDS_REGISTER_FN(network_stun) {
  using namespace shards::Network;
  REGISTER_SHARD("Network.STUN", STUNShard);
}