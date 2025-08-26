#pragma once

#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <memory>
#include <optional>
#include <string>
#include <shared_mutex>
#include <atomic>
#include <vector>
#include "petname.h"

namespace shards::dbg {

using json = nlohmann::json;
using tcp = boost::asio::ip::tcp;

struct BreakpointRequest {
  int line;
  bool verified;
};

enum class CommandType {
  None,
  Pause,
  Continue,
  StepIn,
  StepOut,
  StepOver,
  Stop,
};

struct Command {
  CommandType type;
};

enum class DebuggerState { NotStarted, Initializing, Running, Paused, Stopped };

struct Thread {
  std::string name;
  uint64_t id;
};

/**
 * Specifies the checksum algorithm to use
 * Values: MD5, SHA1, SHA256, timestamp
 */
enum class ChecksumAlgorithm { MD5, SHA1, SHA256, Timestamp };

/**
 * The checksum of an item calculated by the specified algorithm.
 */
struct Checksum {
  /**
   * The algorithm used to calculate this checksum.
   */
  ChecksumAlgorithm algorithm;

  /**
   * Value of the checksum, encoded as a hexadecimal value.
   */
  std::string checksum;
};

struct Source {
  /**
   * The short name of the source. Every source returned from the debug adapter
   * has a name.
   * When sending a source to the debug adapter this name is optional.
   */
  std::optional<std::string> name;

  /**
   * The path of the source to be shown in the UI.
   * It is only used to locate and load the content of the source if no
   * `sourceReference` is specified (or its value is 0).
   */
  std::optional<std::string> path;

  /**
   * If the value > 0 the contents of the source must be retrieved through the
   * `source` request (even if a path is specified).
   * Since a `sourceReference` is only valid for a session, it can not be used
   * to persist a source.
   * The value should be less than or equal to 2147483647 (2^31-1).
   */
  std::optional<int32_t> sourceReference;

  /**
   * A hint for how to present the source in the UI.
   * A value of `deemphasize` can be used to indicate that the source is not
   * available or that it is skipped on stepping.
   * Values: 'normal', 'emphasize', 'deemphasize'
   */
  enum class PresentationHint { Normal, Emphasize, Deemphasize };
  std::optional<PresentationHint> presentationHint;

  /**
   * The origin of this source. For example, 'internal module', 'inlined content
   * from source map', etc.
   */
  std::optional<std::string> origin;

  /**
   * A list of sources that are related to this source. These may be the source
   * that generated this source.
   */
  std::optional<std::vector<Source>> sources;

  /**
   * Additional data that a debug adapter might want to loop through the client.
   * The client should leave the data intact and persist it across sessions. The
   * client should not interpret the data.
   */
  std::optional<json> adapterData;

  /**
   * The checksums associated with this file.
   */
  std::optional<std::vector<Checksum>> checksums;
};

struct StackFrame {
  /**
   * An identifier for the stack frame. It must be unique across all threads.
   * This id can be used to retrieve the scopes of the frame with the `scopes`
   * request or to restart the execution of a stack frame.
   */
  uint64_t id;

  /**
   * The name of the stack frame, typically a method name.
   */
  std::string name;

  /**
   * The source of the frame.
   */
  std::optional<Source> source;

  /**
   * The line within the source of the frame. If the source attribute is missing
   * or doesn't exist, `line` is 0 and should be ignored by the client.
   */
  uint32_t line;

  /**
   * Start position of the range covered by the stack frame. It is measured in
   * UTF-16 code units and the client capability `columnsStartAt1` determines
   * whether it is 0- or 1-based. If attribute `source` is missing or doesn't
   * exist, `column` is 0 and should be ignored by the client.
   */
  uint32_t column;

  /**
   * The end line of the range covered by the stack frame.
   */
  std::optional<uint32_t> endLine;

  /**
   * End position of the range covered by the stack frame. It is measured in
   * UTF-16 code units and the client capability `columnsStartAt1` determines
   * whether it is 0- or 1-based.
   */
  std::optional<uint32_t> endColumn;

  /**
   * Indicates whether this frame can be restarted with the `restartFrame`
   * request. Clients should only use this if the debug adapter supports the
   * `restart` request and the corresponding capability `supportsRestartFrame`
   * is true. If a debug adapter has this capability, then `canRestart` defaults
   * to `true` if the property is absent.
   */
  std::optional<bool> canRestart;

  /**
   * A memory reference for the current instruction pointer in this frame.
   */
  std::optional<std::string> instructionPointerReference;

  /**
   * The module associated with this frame, if any.
   */
  std::variant<std::monostate, uint64_t, std::string> moduleId;

  /**
   * A hint for how to present this frame in the UI.
   * A value of `label` can be used to indicate that the frame is an artificial
   * frame that is used as a visual label or separator. A value of `subtle` can
   * be used to change the appearance of a frame in a 'subtle' way.
   * Values: 'normal', 'label', 'subtle'
   */
  std::optional<std::string> presentationHint;
};

struct Variable {
  /**
   * The variable's name.
   */
  std::string name;

  /**
   * The variable's value.
   * This can be a multi-line text, e.g. for a function the body of a function.
   * For structured variables (which do not have a simple value), it is
   * recommended to provide a one-line representation of the structured object.
   * This helps to identify the structured object in the collapsed state when
   * its children are not yet visible.
   * An empty string can be used if no value should be shown in the UI.
   */
  std::string value;

  /**
   * The type of the variable's value. Typically shown in the UI when hovering
   * over the value.
   * This attribute should only be returned by a debug adapter if the
   * corresponding capability `supportsVariableType` is true.
   */
  std::optional<std::string> type;

  /**
   * Properties of a variable that can be used to determine how to render the
   * variable in the UI.
   */
  // std::optional<VariablePresentationHint> presentationHint;

  /**
   * The evaluatable name of this variable which can be passed to the `evaluate`
   * request to fetch the variable's value.
   */
  std::optional<std::string> evaluateName;

  /**
   * If `variablesReference` is > 0, the variable is structured and its children
   * can be retrieved by passing `variablesReference` to the `variables` request
   * as long as execution remains suspended. See 'Lifetime of Object References'
   * in the Overview section for details.
   */
  uint64_t variablesReference;

  /**
   * The number of named child variables.
   * The client can use this information to present the children in a paged UI
   * and fetch them in chunks.
   */
  std::optional<uint32_t> namedVariables;

  /**
   * The number of indexed child variables.
   * The client can use this information to present the children in a paged UI
   * and fetch them in chunks.
   */
  std::optional<uint32_t> indexedVariables;

  /**
   * A memory reference associated with this variable.
   * For pointer type variables, this is generally a reference to the memory
   * address contained in the pointer.
   * For executable data, this reference may later be used in a `disassemble`
   * request.
   * This attribute may be returned by a debug adapter if corresponding
   * capability `supportsMemoryReferences` is true.
   */
  std::optional<std::string> memoryReference;

  /**
   * A reference that allows the client to request the location where the
   * variable is declared. This should be present only if the adapter is likely
   * to be able to resolve the location.
   * 
   * This reference shares the same lifetime as the `variablesReference`. See
   * 'Lifetime of Object References' in the Overview section for details.
   */
  std::optional<uint64_t> declarationLocationReference;

  /**
   * A reference that allows the client to request the location where the
   * variable's value is declared. For example, if the variable contains a
   * function pointer, the adapter may be able to look up the function's
   * location. This should be present only if the adapter is likely to be able
   * to resolve the location.
   * 
   * This reference shares the same lifetime as the `variablesReference`. See
   * 'Lifetime of Object References' in the Overview section for details.
   */
  std::optional<uint64_t> valueLocationReference;
};

/**
 * Provides formatting information for a value.
 */
struct ValueFormat {
  /**
   * Display the value in hex.
   */
  std::optional<bool> hex;
};

/**
 * Arguments for variables request.
 */
struct VariablesArguments {
  /**
   * The variable for which to retrieve its children. The `variablesReference`
   * must have been obtained in the current suspended state.
   */
  uint64_t variablesReference;

  /**
   * Filter to limit the child variables to either named or indexed.
   * Values: 'indexed', 'named'
   */
  std::optional<std::string> filter;

  /**
   * The index of the first variable to return; if omitted children start at 0.
   */
  std::optional<uint32_t> start;

  /**
   * The number of variables to return. If count is missing or 0, all variables
   * are returned.
   */
  std::optional<uint32_t> count;

  /**
   * Specifies details on how to format the Variable values.
   */
  std::optional<ValueFormat> format;
};

/**
 * A Scope is a named container for variables. Optionally a scope can map to a
 * source or a range within a source.
 */
struct Scope {
  /**
   * Name of the scope such as 'Arguments', 'Locals', or 'Registers'. This
   * string is shown in the UI as is and can be translated.
   */
  std::string name;

  /**
   * A hint for how to present this scope in the UI. If this attribute is
   * missing, the scope is shown with a generic UI.
   * Values: 'arguments', 'locals', 'registers', 'returnValue', etc.
   */
  std::optional<std::string> presentationHint;

  /**
   * The variables of this scope can be retrieved by passing the value of
   * `variablesReference` to the `variables` request as long as execution
   * remains suspended.
   */
  uint64_t variablesReference;

  /**
   * The number of named variables in this scope.
   * The client can use this information to present the variables in a paged UI
   * and fetch them in chunks.
   */
  std::optional<uint32_t> namedVariables;

  /**
   * The number of indexed variables in this scope.
   * The client can use this information to present the variables in a paged UI
   * and fetch them in chunks.
   */
  std::optional<uint32_t> indexedVariables;

  /**
   * If true, the number of variables in this scope is large or expensive to
   * retrieve.
   */
  bool expensive;

  /**
   * The source for this scope.
   */
  std::optional<Source> source;

  /**
   * The start line of the range covered by this scope.
   */
  std::optional<uint32_t> line;

  /**
   * Start position of the range covered by the scope. It is measured in UTF-16
   * code units and the client capability `columnsStartAt1` determines whether
   * it is 0- or 1-based.
   */
  std::optional<uint32_t> column;

  /**
   * The end line of the range covered by this scope.
   */
  std::optional<uint32_t> endLine;

  /**
   * End position of the range covered by the scope. It is measured in UTF-16
   * code units and the client capability `columnsStartAt1` determines whether
   * it is 0- or 1-based.
   */
  std::optional<uint32_t> endColumn;
};

/**
 * Arguments for scopes request.
 */
struct ScopesArguments {
  /**
   * Retrieve the scopes for the stack frame identified by `frameId`. The
   * `frameId` must have been obtained in the current suspended state.
   */
  uint64_t frameId;
};


struct DAPServer {
  boost::asio::io_context io_context_;
  std::optional<tcp::acceptor> acceptor_;
  std::mutex starup_mtx_;
  std::atomic_bool pendingStop_{};
  std::optional<boost::asio::ip::udp::socket> udp_socket_;
  std::unique_ptr<boost::asio::steady_timer> broadcast_timer_;
  int port_;
  std::string instance_name_;
  std::shared_ptr<spdlog::logger> logger_;
  
  // Service discovery
  static constexpr int DISCOVERY_PORT = 57426;
  static constexpr int STARTING_PORT = 57427;

  // Debugger state management
  std::atomic<DebuggerState> debuggerState;
  std::vector<std::shared_ptr<tcp::socket>> connectedClients;
  mutable std::shared_mutex clientsMutex;
  std::atomic<int> sequenceNumber;

  size_t numFastBroadcast = 16;

  std::function<void(std::vector<Thread> &)> requestThreads;
  std::function<void(uint64_t threadId, std::vector<StackFrame> &)> requestCallStack;
  std::function<void(const Source &, std::vector<BreakpointRequest> &)> setBreakpoints;
  std::function<void(const Command &)> handleCommand;
  std::function<void(const VariablesArguments &, std::vector<Variable> &)> requestVariables;
  std::function<void(const ScopesArguments &, std::vector<Scope> &)> requestScopes;
  std::function<void(const std::string& instanceName, int actualPort)> onStarted;

public:
  explicit DAPServer();
  void start();
  void stop();
  
  // New methods
  void startServiceDiscovery();
  void stopServiceDiscovery();
  void sendServiceAnnouncement();
  const std::string& getInstanceName() const { return instance_name_; }
  int getActualPort() const { return port_; }

  // Event sending methods
  void sendInitializedEvent();
  void sendStoppedEvent(const std::string &reason, uint64_t threadId = 1, const std::string &description = "");
  void sendContinuedEvent(int threadId = 1, bool allThreadsContinued = true);

  // State management
  DebuggerState getState() const { return debuggerState.load(); }
  void setState(DebuggerState state) { debuggerState.store(state); }

private:
  void accept_connections();
  void handle_client(std::shared_ptr<tcp::socket> socket);
  void read_message(std::shared_ptr<tcp::socket> socket, std::shared_ptr<boost::asio::streambuf> buffer);
  void read_content(std::shared_ptr<tcp::socket> socket, std::shared_ptr<boost::asio::streambuf> buffer, size_t content_length);
  void process_message(std::shared_ptr<tcp::socket> socket, std::shared_ptr<boost::asio::streambuf> buffer,
                       size_t content_length);
  size_t parse_content_length(const std::string &header);
  json handle_dap_request(const json &request);
  void send_response(std::shared_ptr<tcp::socket> socket, const json &response);
  void send_event(const json &event);

  void setCommand(CommandType commandType);
  void addClient(std::shared_ptr<tcp::socket> socket);
  void removeClient(std::shared_ptr<tcp::socket> socket);
};

} // namespace shards::dbg
