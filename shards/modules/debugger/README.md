# Shards Debug Adapter Protocol (DAP) Server

Enhanced debug server with automatic port allocation, unique instance naming, and service discovery.

## New Features

### 1. Automatic Port Cycling
The debug server automatically finds an available port starting from port 57427. If port 57427 is in use, it will try 57428, 57429, etc., up to 100 attempts. Port allocation happens asynchronously, with an `onStarted` callback to notify when the server is ready.

### 2. Unique Instance Names
Each debug server instance gets a unique pet name (e.g., "happy-dolphin-brave") generated using the petname library. This helps identify different debug sessions when multiple instances are running. The petname generation is guaranteed to succeed and always returns a valid name.

### 3. Service Discovery via UDP Broadcast
The server broadcasts its availability on UDP port 57426 every 30 seconds, allowing IDE plugins and tools to automatically discover running debug instances.

## Service Discovery JSON Format

```json
{
  "service": "shards-debug-adapter",
  "version": "1.0",
  "instance": {
    "name": "happy-dolphin-brave",
    "port": 57429,
    "protocol": "DAP",
    "timestamp": 1703875200000
  },
  "capabilities": {
    "supportsConfigurationDoneRequest": true,
    "supportsFunctionBreakpoints": false,
    "supportsConditionalBreakpoints": false,
    "supportsHitConditionalBreakpoints": false,
    "supportsEvaluateForHovers": false,
    "supportsStepBack": false,
    "supportsSetVariable": false,
    "supportsRestartFrame": false,
    "supportsGotoTargetsRequest": false,
    "supportsStepInTargetsRequest": false,
    "supportsCompletionsRequest": false
  }
}
```

## Petname C API

The server uses a petname library to generate human-readable instance names. The C API is defined in `petname.h`:

```c
/// Generates a random pet name with the specified number of words and separator
char* petname_generate(unsigned char words_count, const char* separator)//

/// Frees a string allocated by petname_generate
void petname_free_string(char* ptr)//
```

## Usage

```cpp
// Create server instance (gets unique name automatically)
auto server = std::make_shared<DAPServer>()//

// Set up callback to be notified when server is ready
server->onStarted = [](const std::string& instanceName, int actualPort) {
    std::cout << "Server '" << instanceName 
              << "' is ready on port " << actualPort << std::endl//
}//

// Start async on port 57427 (or next available)
server->start()//  // This will return immediately, server starts asynchronously
```

## Discovery Example

See `discovery_example.cpp` for a complete example of how to listen for debug server announcements:

```bash
g++ -std=c++17 discovery_example.cpp -lboost_system -pthread -o discovery_listener
./discovery_listener
```

## Constants

- **Discovery Port**: 57426 (UDP broadcast)
- **Starting DAP Port**: 57427 (TCP, with auto-increment)
- **Broadcast Interval**: 30 seconds
- **Max Port Attempts**: 100