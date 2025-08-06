#include "debugger.hpp"
#include "cmdline.hpp"
#ifndef SHARDS_DEBUGGER_STANDALONE
#include <shards/modules/langffi/line_info.hpp>
#include <shards/utility.hpp>
#endif
#include <shards/log/log.hpp>
#include <shards/core/assert.hpp>
#include <boost/asio.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <iostream>
#include <algorithm>
#include <chrono>

namespace shards::dbg {

Source parseSource(const json &source) {
  Source result;

  if (source.contains("name")) {
    result.name = source["name"].get<std::string>();
  }

  if (source.contains("path")) {
    result.path = source["path"].get<std::string>();
  }

  if (source.contains("sourceReference")) {
    result.sourceReference = source["sourceReference"].get<int32_t>();
  }

  if (source.contains("presentationHint")) {
    std::string hint = source["presentationHint"].get<std::string>();
    if (hint == "normal") {
      result.presentationHint = Source::PresentationHint::Normal;
    } else if (hint == "emphasize") {
      result.presentationHint = Source::PresentationHint::Emphasize;
    } else if (hint == "deemphasize") {
      result.presentationHint = Source::PresentationHint::Deemphasize;
    }
  }

  if (source.contains("origin")) {
    result.origin = source["origin"].get<std::string>();
  }

  if (source.contains("sources") && source["sources"].is_array()) {
    std::vector<Source> sources;
    for (const auto &src : source["sources"]) {
      sources.push_back(parseSource(src));
    }
    result.sources = std::move(sources);
  }

  if (source.contains("adapterData")) {
    result.adapterData = source["adapterData"];
  }

  return result;
}

DAPServer::DAPServer() {
  logger_ = shards::logging::getOrCreate("dap_server");
  debuggerState = DebuggerState::NotStarted;
  sequenceNumber = 0;

  // Generate unique instance name
  char *petname = petname_generate(3, "-");
  instance_name_ = std::string(petname);
  petname_free_string(petname);

  SPDLOG_LOGGER_INFO(logger_, "Debug server instance name: {}", instance_name_);
}

void DAPServer::start() {
  shassert(!acceptor_);

  // Start async port allocation
  auto timer = std::make_shared<boost::asio::steady_timer>(io_context_);
  timer->async_wait([this, timer](boost::system::error_code ec) {
    if (!ec) {
      // Find available port and bind atomically to avoid race conditions
      const int max_attempts = 100;
      bool bound = false;

      for (int attempt = 0; attempt < max_attempts && !bound; ++attempt) {
        int test_port = STARTING_PORT + attempt;

        try {
          // Try to bind directly to the port - no separate availability check
          auto ep = tcp::endpoint(boost::asio::ip::address_v4::any(), test_port);
          acceptor_.emplace(io_context_);
          acceptor_->open((ep.protocol)());
          acceptor_->bind(ep);
          acceptor_->listen();
          port_ = test_port;
          bound = true;
          SPDLOG_LOGGER_INFO(logger_, "DAP server '{}' bound to port {}", instance_name_, port_);

          // Start service discovery
          startServiceDiscovery();

          // Notify that server is started and ready
          if (onStarted) {
            onStarted(instance_name_, port_);
          }

          accept_connections();
        } catch (const std::exception &e) {
          // Port is in use or other error, try next port
          acceptor_.reset();
          SPDLOG_LOGGER_DEBUG(logger_, "Failed to bind to port {}: {}", test_port, e.what());
          if (attempt == max_attempts - 1) {
            // Last attempt failed
            SPDLOG_LOGGER_ERROR(logger_, "Error starting DAP server after {} attempts: {}", max_attempts, e.what());
          }
        }
      }

      if (!bound) {
        SPDLOG_LOGGER_WARN(logger_, "Could not find available port after {} attempts starting from {}", max_attempts,
                           STARTING_PORT);
      }
    }
  });

  try {
    io_context_.run();
    SPDLOG_LOGGER_ERROR(logger_, "DAP server stopped");
  } catch (std::exception &e) {
    SPDLOG_LOGGER_ERROR(logger_, "Error running DAP server IO context: {}", e.what());
  }
}

void DAPServer::stop() {
  stopServiceDiscovery();
  if (acceptor_) {
    io_context_.stop();
    acceptor_.reset();
  }
}

void DAPServer::accept_connections() {
  auto socket = std::make_shared<tcp::socket>(io_context_);
  acceptor_->async_accept(*socket, [this, socket](boost::system::error_code ec) {
    if (!ec) {
      SPDLOG_LOGGER_INFO(logger_, "New client connected");
      addClient(socket);
      handle_client(socket);
    } else {
      SPDLOG_LOGGER_ERROR(logger_, "Error accepting connection: {}", ec.message());
    }
    accept_connections();
  });
}

void DAPServer::handle_client(std::shared_ptr<tcp::socket> socket) {
  auto buffer = std::make_shared<boost::asio::streambuf>();
  read_message(socket, buffer);

  // Simulate continue, when disconnected
  DEFER({ setCommand(CommandType::Continue); });
}

void DAPServer::read_message(std::shared_ptr<tcp::socket> socket, std::shared_ptr<boost::asio::streambuf> buffer) {
  // Read Content-Length header
  boost::asio::async_read_until(*socket, *buffer, "\r\n\r\n",
                                [this, socket, buffer](boost::system::error_code ec, std::size_t bytes_transferred) {
                                  if (!ec) {
                                    std::string header_data{boost::asio::buffers_begin(buffer->data()),
                                                            boost::asio::buffers_begin(buffer->data()) + bytes_transferred};

                                    // Parse Content-Length
                                    size_t content_length = parse_content_length(header_data);
                                    buffer->consume(bytes_transferred);

                                    if (content_length > 0) {
                                      read_content(socket, buffer, content_length);
                                    } else {
                                      read_message(socket, buffer);
                                    }
                                  } else {
                                    SPDLOG_LOGGER_ERROR(logger_, "Error reading header: {}", ec.message());
                                  }
                                });
}

void DAPServer::read_content(std::shared_ptr<tcp::socket> socket, std::shared_ptr<boost::asio::streambuf> buffer,
                             size_t content_length) {
  // Ensure we have enough data
  if (buffer->size() < content_length) {
    boost::asio::async_read(*socket, *buffer, boost::asio::transfer_at_least(content_length - buffer->size()),
                            [this, socket, buffer, content_length](boost::system::error_code ec, std::size_t) {
                              if (!ec) {
                                process_message(socket, buffer, content_length);
                              } else {
                                SPDLOG_LOGGER_ERROR(logger_, "Error reading content: {}", ec.message());
                              }
                            });
  } else {
    process_message(socket, buffer, content_length);
  }
}

void DAPServer::process_message(std::shared_ptr<tcp::socket> socket, std::shared_ptr<boost::asio::streambuf> buffer,
                                size_t content_length) {
  std::string json_data{boost::asio::buffers_begin(buffer->data()), boost::asio::buffers_begin(buffer->data()) + content_length};
  buffer->consume(content_length);

  try {
    json request = json::parse(json_data);
    SPDLOG_LOGGER_DEBUG(logger_, "Received: {}", request.dump(2));

    json response = handle_dap_request(request);
    send_response(socket, response);

  } catch (const std::exception &e) {
    SPDLOG_LOGGER_ERROR(logger_, "Error parsing JSON: {}", e.what());
  }

  // Continue reading messages
  read_message(socket, buffer);
}

size_t DAPServer::parse_content_length(const std::string &header) {
  const std::string prefix = "Content-Length: ";
  size_t pos = header.find(prefix);
  if (pos != std::string::npos) {
    size_t start = pos + prefix.length();
    size_t end = header.find("\r\n", start);
    if (end != std::string::npos) {
      return std::stoul(header.substr(start, end - start));
    }
  }
  return 0;
}

void DAPServer::setCommand(CommandType commandType) {
  SPDLOG_LOGGER_INFO(logger_, "Command set to {}", static_cast<int>(commandType));
  if (handleCommand) {
    handleCommand(Command{commandType});
  }
}

void DAPServer::addClient(std::shared_ptr<tcp::socket> socket) {
  std::unique_lock<std::shared_mutex> lock(clientsMutex);
  connectedClients.push_back(socket);
}

void DAPServer::removeClient(std::shared_ptr<tcp::socket> socket) {
  std::unique_lock<std::shared_mutex> lock(clientsMutex);
  connectedClients.erase(std::remove(connectedClients.begin(), connectedClients.end(), socket), connectedClients.end());
}

void DAPServer::send_event(const json &event) {
  std::string json_str = event.dump();
  std::string message = "Content-Length: " + std::to_string(json_str.length()) + "\r\n\r\n" + json_str;

  std::shared_lock<std::shared_mutex> lock(clientsMutex);
  for (auto &client : connectedClients) {
    if (client && client->is_open()) {
      auto buffer = std::make_shared<std::string>(message);
      boost::asio::async_write(*client, boost::asio::buffer(*buffer),
                               [this, client, buffer](boost::system::error_code ec, std::size_t) {
                                 if (ec) {
                                   SPDLOG_LOGGER_ERROR(logger_, "Error sending event: {}", ec.message());
                                   // Remove client on error
                                   removeClient(client);
                                 } else {
                                   SPDLOG_LOGGER_DEBUG(logger_, "Event sent successfully");
                                 }
                               });
    }
  }
}

void DAPServer::sendInitializedEvent() {
  json event = {{"seq", sequenceNumber.fetch_add(1)}, {"type", "event"}, {"event", "initialized"}};

  SPDLOG_LOGGER_INFO(logger_, "Sending initialized event");
  send_event(event);
}

void DAPServer::sendStoppedEvent(const std::string &reason, uint64_t threadId, const std::string &description) {
  json body = {{"reason", reason}, {"threadId", threadId}, {"allThreadsStopped", true}};

  if (!description.empty()) {
    body["description"] = description;
  }

  json event = {{"seq", sequenceNumber.fetch_add(1)}, {"type", "event"}, {"event", "stopped"}, {"body", body}};

  SPDLOG_LOGGER_INFO(logger_, "Sending stopped event: {}", reason);
  send_event(event);
  setState(DebuggerState::Paused);
}

void DAPServer::sendContinuedEvent(int threadId, bool allThreadsContinued) {
  json event = {{"seq", sequenceNumber.fetch_add(1)},
                {"type", "event"},
                {"event", "continued"},
                {"body", {{"threadId", threadId}, {"allThreadsContinued", allThreadsContinued}}}};

  SPDLOG_LOGGER_INFO(logger_, "Sending continued event");
  send_event(event);
  setState(DebuggerState::Running);
}

json DAPServer::handle_dap_request(const json &request) {
  std::string command = request.value("command", "");
  int seq = request.value("seq", 0);

  json response = {{"seq", seq + 1000}, // Response seq
                   {"type", "response"},
                   {"request_seq", seq},
                   {"command", command},
                   {"success", true}};

  bool handled = true;
  if (command == "initialize") {
    setState(DebuggerState::Initializing);
    response["body"] = {{"supportsConfigurationDoneRequest", true},
                        {"supportsFunctionBreakpoints", false},
                        {"supportsConditionalBreakpoints", false},
                        {"supportsHitConditionalBreakpoints", false},
                        {"supportsEvaluateForHovers", false},
                        {"exceptionBreakpointFilters", json::array()},
                        {"supportsStepBack", false},
                        {"supportsSetVariable", false},
                        {"supportsRestartFrame", false},
                        {"supportsGotoTargetsRequest", false},
                        {"supportsStepInTargetsRequest", false},
                        {"supportsCompletionsRequest", false}};

    // Send initialized event after responding to initialize request
    // Use a small delay to ensure response is sent first
    auto timer = std::make_shared<boost::asio::steady_timer>(io_context_, std::chrono::milliseconds(10));
    timer->async_wait([this, timer](boost::system::error_code ec) {
      if (!ec) {
        sendInitializedEvent();
      }
    });

  } else if (command == "launch" || command == "attach") {
    response["body"] = json::object();
    setState(DebuggerState::Running);

  } else if (command == "setBreakpoints") {
    json args = request.value("arguments", json::object());
    json sourceJson = args.value("source", json::object());
    auto source = parseSource(sourceJson);
    std::string sourcePath = source.path.value_or("");
    json breakpoints = args.value("breakpoints", json::array());
    std::vector<BreakpointRequest> breakpointRequests;
    for (const auto &bp : breakpoints) {
      int line = bp.value("line", 0);
      if (line > 0) {
        breakpointRequests.push_back({line, true});
      }
    }

    if (setBreakpoints) {
      setBreakpoints(source, breakpointRequests);
    }

    // Set new breakpoints
    json responseBreakpoints = json::array();
    for (const auto &bp : breakpointRequests) {
      if (bp.line > 0) {
        responseBreakpoints.push_back({{"verified", bp.verified}, {"line", bp.line}});
      }
    }

    response["body"] = {{"breakpoints", responseBreakpoints}};
  } else if (command == "configurationDone") {
    response["body"] = json::object();
  } else if (command == "threads") {
    json threads_json = json::array();
    std::vector<Thread> threads;
    if (requestThreads) {
      requestThreads(threads);
      for (const auto &thread : threads) {
        threads_json.push_back({{"id", thread.id}, {"name", thread.name}});
      }
    }
    response["body"] = {{"threads", threads_json}};
  } else if (command == "stackTrace") {
    json args = request.value("arguments", json::object());
    uint64_t threadId = args.value("threadId", 1ull);
    int startFrame = args.value("startFrame", 0);
    int levels = args.value("levels", 0); // 0 means all frames

    json stackFrames = json::array();
    std::vector<StackFrame> frames;

    if (requestCallStack) {
      requestCallStack(threadId, frames);

      // Apply startFrame and levels filtering
      size_t endFrame = frames.size();
      if (levels > 0 && startFrame + levels < frames.size()) {
        endFrame = startFrame + levels;
      }

      for (size_t i = startFrame; i < endFrame && i < frames.size(); ++i) {
        const auto &frame = frames[i];
        json frameJson = {{"id", frame.id}, {"name", frame.name}, {"line", frame.line}, {"column", frame.column}};

        // Add source if present
        if (frame.source.has_value()) {
          json sourceJson = json::object();
          const auto &source = frame.source.value();

          if (source.name.has_value()) {
            sourceJson["name"] = source.name.value();
          }
          if (source.path.has_value()) {
            sourceJson["path"] = source.path.value();
          }
          if (source.sourceReference.has_value()) {
            sourceJson["sourceReference"] = source.sourceReference.value();
          }
          if (source.presentationHint.has_value()) {
            std::string hint;
            switch (source.presentationHint.value()) {
            case Source::PresentationHint::Normal:
              hint = "normal";
              break;
            case Source::PresentationHint::Emphasize:
              hint = "emphasize";
              break;
            case Source::PresentationHint::Deemphasize:
              hint = "deemphasize";
              break;
            }
            sourceJson["presentationHint"] = hint;
          }
          if (source.origin.has_value()) {
            sourceJson["origin"] = source.origin.value();
          }
          if (source.adapterData.has_value()) {
            sourceJson["adapterData"] = source.adapterData.value();
          }
          if (source.checksums.has_value()) {
            json checksumsJson = json::array();
            for (const auto &checksum : source.checksums.value()) {
              std::string algorithm;
              switch (checksum.algorithm) {
              case ChecksumAlgorithm::MD5:
                algorithm = "MD5";
                break;
              case ChecksumAlgorithm::SHA1:
                algorithm = "SHA1";
                break;
              case ChecksumAlgorithm::SHA256:
                algorithm = "SHA256";
                break;
              case ChecksumAlgorithm::Timestamp:
                algorithm = "timestamp";
                break;
              }
              checksumsJson.push_back({{"algorithm", algorithm}, {"checksum", checksum.checksum}});
            }
            sourceJson["checksums"] = checksumsJson;
          }

          frameJson["source"] = sourceJson;
        }

        // Add optional fields
        if (frame.endLine.has_value()) {
          frameJson["endLine"] = frame.endLine.value();
        }
        if (frame.endColumn.has_value()) {
          frameJson["endColumn"] = frame.endColumn.value();
        }
        if (frame.canRestart.has_value()) {
          frameJson["canRestart"] = frame.canRestart.value();
        }
        if (frame.instructionPointerReference.has_value()) {
          frameJson["instructionPointerReference"] = frame.instructionPointerReference.value();
        }
        if (frame.presentationHint.has_value()) {
          frameJson["presentationHint"] = frame.presentationHint.value();
        }

        // Handle moduleId variant
        if (std::holds_alternative<uint64_t>(frame.moduleId)) {
          frameJson["moduleId"] = std::get<uint64_t>(frame.moduleId);
        } else if (std::holds_alternative<std::string>(frame.moduleId)) {
          frameJson["moduleId"] = std::get<std::string>(frame.moduleId);
        }

        stackFrames.push_back(frameJson);
      }
    }

    response["body"] = {{"stackFrames", stackFrames}, {"totalFrames", frames.size()}};
  } else if (command == "scopes") {
    json args = request.value("arguments", json::object());

    // Parse ScopesArguments
    ScopesArguments scopesArgs;
    scopesArgs.frameId = args.value("frameId", 0);

    // Get scopes through callback
    std::vector<Scope> scopes;
    if (requestScopes) {
      requestScopes(scopesArgs, scopes);
    }

    // Convert scopes to JSON
    json scopesJson = json::array();
    for (const auto &scope : scopes) {
      json scopeJson = {{"name", scope.name}, {"variablesReference", scope.variablesReference}, {"expensive", scope.expensive}};

      if (scope.presentationHint.has_value()) {
        scopeJson["presentationHint"] = scope.presentationHint.value();
      }

      if (scope.namedVariables.has_value()) {
        scopeJson["namedVariables"] = scope.namedVariables.value();
      }

      if (scope.indexedVariables.has_value()) {
        scopeJson["indexedVariables"] = scope.indexedVariables.value();
      }

      if (scope.source.has_value()) {
        json sourceJson = json::object();
        const auto &source = scope.source.value();

        if (source.name.has_value()) {
          sourceJson["name"] = source.name.value();
        }
        if (source.path.has_value()) {
          sourceJson["path"] = source.path.value();
        }
        if (source.sourceReference.has_value()) {
          sourceJson["sourceReference"] = source.sourceReference.value();
        }
        if (source.presentationHint.has_value()) {
          std::string hint;
          switch (source.presentationHint.value()) {
          case Source::PresentationHint::Normal:
            hint = "normal";
            break;
          case Source::PresentationHint::Emphasize:
            hint = "emphasize";
            break;
          case Source::PresentationHint::Deemphasize:
            hint = "deemphasize";
            break;
          }
          sourceJson["presentationHint"] = hint;
        }
        if (source.origin.has_value()) {
          sourceJson["origin"] = source.origin.value();
        }
        if (source.adapterData.has_value()) {
          sourceJson["adapterData"] = source.adapterData.value();
        }
        if (source.checksums.has_value()) {
          json checksumsJson = json::array();
          for (const auto &checksum : source.checksums.value()) {
            std::string algorithm;
            switch (checksum.algorithm) {
            case ChecksumAlgorithm::MD5:
              algorithm = "MD5";
              break;
            case ChecksumAlgorithm::SHA1:
              algorithm = "SHA1";
              break;
            case ChecksumAlgorithm::SHA256:
              algorithm = "SHA256";
              break;
            case ChecksumAlgorithm::Timestamp:
              algorithm = "timestamp";
              break;
            }
            checksumsJson.push_back({{"algorithm", algorithm}, {"checksum", checksum.checksum}});
          }
          sourceJson["checksums"] = checksumsJson;
        }

        scopeJson["source"] = sourceJson;
      }

      if (scope.line.has_value()) {
        scopeJson["line"] = scope.line.value();
      }

      if (scope.column.has_value()) {
        scopeJson["column"] = scope.column.value();
      }

      if (scope.endLine.has_value()) {
        scopeJson["endLine"] = scope.endLine.value();
      }

      if (scope.endColumn.has_value()) {
        scopeJson["endColumn"] = scope.endColumn.value();
      }

      scopesJson.push_back(scopeJson);
    }

    response["body"] = {{"scopes", scopesJson}};
  } else if (command == "variables") {
    json args = request.value("arguments", json::object());

    // Parse VariablesArguments
    VariablesArguments variablesArgs;
    variablesArgs.variablesReference = args.value("variablesReference", 0);

    if (args.contains("filter")) {
      variablesArgs.filter = args["filter"].get<std::string>();
    }

    if (args.contains("start")) {
      variablesArgs.start = args["start"].get<uint32_t>();
    }

    if (args.contains("count")) {
      variablesArgs.count = args["count"].get<uint32_t>();
    }

    if (args.contains("format")) {
      ValueFormat format;
      json formatJson = args["format"];
      if (formatJson.contains("hex")) {
        format.hex = formatJson["hex"].get<bool>();
      }
      variablesArgs.format = format;
    }

    // Get variables through callback
    std::vector<Variable> variables;
    if (requestVariables) {
      requestVariables(variablesArgs, variables);
    }

    // Convert variables to JSON
    json variablesJson = json::array();
    for (const auto &variable : variables) {
      json varJson = {{"name", variable.name}, {"value", variable.value}, {"variablesReference", variable.variablesReference}};

      if (variable.type.has_value()) {
        varJson["type"] = variable.type.value();
      }

      if (variable.evaluateName.has_value()) {
        varJson["evaluateName"] = variable.evaluateName.value();
      }

      if (variable.namedVariables.has_value()) {
        varJson["namedVariables"] = variable.namedVariables.value();
      }

      if (variable.indexedVariables.has_value()) {
        varJson["indexedVariables"] = variable.indexedVariables.value();
      }

      if (variable.memoryReference.has_value()) {
        varJson["memoryReference"] = variable.memoryReference.value();
      }

      if (variable.declarationLocationReference.has_value()) {
        varJson["declarationLocationReference"] = variable.declarationLocationReference.value();
      }

      if (variable.valueLocationReference.has_value()) {
        varJson["valueLocationReference"] = variable.valueLocationReference.value();
      }

      variablesJson.push_back(varJson);
    }

    response["body"] = {{"variables", variablesJson}};
  } else if (command == "continue") {
    setCommand(CommandType::Continue);
    response["body"] = {{"allThreadsContinued", true}};
    // Send continued event
    // sendContinuedEvent();
  } else if (command == "pause") {
    setCommand(CommandType::Pause);
    response["body"] = json::object();

    // Send stopped event with pause reason
    // sendStoppedEvent("pause", 1, "Execution paused by user");
  } else if (command == "stepIn") {
    setCommand(CommandType::StepIn);
    response["body"] = json::object();

    // For now, simulate step completion with a stopped event
    // sendStoppedEvent("step", 1, "Step in completed");
  } else if (command == "stepOut") {
    setCommand(CommandType::StepOut);
    response["body"] = json::object();

    // For now, simulate step completion with a stopped event
    // sendStoppedEvent("step", 1, "Step out completed");

  } else if (command == "next") {
    setCommand(CommandType::StepOver);
    response["body"] = json::object();
  } else if (command == "disconnect" || command == "terminate") {
    setCommand(CommandType::Stop);
    setState(DebuggerState::Stopped);
    response["body"] = json::object();
  } else {
    handled = false;
  }
  if (handled) {
    SPDLOG_LOGGER_DEBUG(logger_, "Handled {} request", command);
  } else {
    SPDLOG_LOGGER_WARN(logger_, "Unhandled command: {}", command);
    response["success"] = false;
    response["message"] = "Command not implemented";
  }

  return response;
}

void DAPServer::send_response(std::shared_ptr<tcp::socket> socket, const json &response) {
  std::string json_str = response.dump();
  std::string message = "Content-Length: " + std::to_string(json_str.length()) + "\r\n\r\n" + json_str;

  auto buffer = std::make_shared<std::string>(std::move(message));
  boost::asio::async_write(*socket, boost::asio::buffer(*buffer),
                           [this, socket, buffer](boost::system::error_code ec, std::size_t) {
                             if (ec) {
                               SPDLOG_LOGGER_ERROR(logger_, "Error sending response: {}", ec.message());
                             } else {
                               SPDLOG_LOGGER_DEBUG(logger_, "Response sent successfully");
                             }
                           });
}

void DAPServer::startServiceDiscovery() {
  try {
    udp_socket_.emplace(io_context_, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0));
    udp_socket_->set_option(boost::asio::socket_base::broadcast(true));

    broadcast_timer_ = std::make_unique<boost::asio::steady_timer>(io_context_);

    SPDLOG_LOGGER_INFO(logger_, "Started service discovery broadcasting on port {}", DISCOVERY_PORT);
    sendServiceAnnouncement();
  } catch (const std::exception &e) {
    SPDLOG_LOGGER_ERROR(logger_, "Failed to start service discovery: {}", e.what());
  }
}

void DAPServer::stopServiceDiscovery() {
  if (broadcast_timer_) {
    broadcast_timer_->cancel();
    broadcast_timer_.reset();
  }

  if (udp_socket_) {
    udp_socket_->close();
    udp_socket_.reset();
  }
}

void DAPServer::sendServiceAnnouncement() {
  if (!udp_socket_ || !broadcast_timer_) {
    return;
  }

  try {

    static std::string cmdLine = getCmdLine();

    // Create service announcement JSON
    json announcement = {
        {"service", "shards-debug-adapter"},
        {"version", "1.0"},
        {"name", instance_name_},
        {"cmd", cmdLine},
        {"port", port_},
        {"timestamp",
         std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()},
    };

    std::string message = announcement.dump();

    auto endpoint = boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::broadcast(), DISCOVERY_PORT);

    auto buffer = std::make_shared<std::string>(std::move(message));
    udp_socket_->async_send_to(boost::asio::buffer(*buffer), endpoint, [this, buffer](boost::system::error_code ec, std::size_t) {
      if (ec) {
        SPDLOG_LOGGER_DEBUG(logger_, "Error sending service announcement: {}", ec.message());
      } else {
        SPDLOG_LOGGER_DEBUG(logger_, "Service announcement sent for instance '{}'", instance_name_);
      }

      // Schedule next announcement in 30 seconds
      if (broadcast_timer_) {
        if (numFastBroadcast > 0) {
          broadcast_timer_->expires_after(std::chrono::seconds(1));
          numFastBroadcast--;
        } else {
          broadcast_timer_->expires_after(std::chrono::seconds(5));
        }
        broadcast_timer_->async_wait([this](boost::system::error_code ec) {
          if (!ec) {
            sendServiceAnnouncement();
          }
        });
      }
    });

  } catch (const std::exception &e) {
    SPDLOG_LOGGER_ERROR(logger_, "Failed to send service announcement: {}", e.what());
  }
}

} // namespace shards::dbg