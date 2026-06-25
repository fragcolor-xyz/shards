// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2020-2024 Fragcolor Pte. Ltd.

#include "platform.hpp"

#if SH_FREERTOS

#include "shards_esp32.h"
#include <shards/core/runtime.hpp>
#include <shards/core/foundation.hpp>
#include <esp_log.h>
#include <memory>
#include <string>

static const char* TAG = "shards_esp32";

namespace shards::esp32 {

// Forward declarations from fs_esp32.cpp
std::shared_ptr<SHWire> loadWireFromPath(const char* path);
std::shared_ptr<SHWire> loadWireFromMemory(const uint8_t* data, size_t size);

// Global state
static std::shared_ptr<SHMesh> g_mesh;
static std::string g_lastError;
static bool g_initialized = false;

static void setError(const char* msg) {
  g_lastError = msg;
  ESP_LOGE(TAG, "%s", msg);
}

static void clearError() {
  g_lastError.clear();
}

} // namespace shards::esp32

extern "C" {

void shards_esp32_init(void) {
  using namespace shards::esp32;

  if (g_initialized) {
    ESP_LOGW(TAG, "Already initialized");
    return;
  }

  ESP_LOGI(TAG, "Initializing Shards ESP32 runtime");

  // Register core shards
  // Note: This requires the shard registration functions to be linked
  shards::registerShards();

  // Create the mesh for scheduling wires
  g_mesh = SHMesh::make();

  g_initialized = true;
  ESP_LOGI(TAG, "Shards ESP32 runtime initialized");
}

void shards_esp32_shutdown(void) {
  using namespace shards::esp32;

  if (!g_initialized) {
    return;
  }

  ESP_LOGI(TAG, "Shutting down Shards ESP32 runtime");

  if (g_mesh) {
    g_mesh->terminate();
    g_mesh.reset();
  }

  g_initialized = false;
  ESP_LOGI(TAG, "Shards ESP32 runtime shut down");
}

SHWireRef shards_esp32_load_wire(const char* path) {
  using namespace shards::esp32;
  clearError();

  if (!g_initialized) {
    setError("Runtime not initialized");
    return nullptr;
  }

  if (!path) {
    setError("Path is null");
    return nullptr;
  }

  auto wire = loadWireFromPath(path);
  if (!wire) {
    setError("Failed to load wire from path");
    return nullptr;
  }

  // Hand the caller an owning SHWireRef (newRef heap-allocates a shared_ptr copy
  // so the wire outlives this function's local).
  return wire->newRef();
}

SHWireRef shards_esp32_load_wire_from_memory(const uint8_t* data, size_t size) {
  using namespace shards::esp32;
  clearError();

  if (!g_initialized) {
    setError("Runtime not initialized");
    return nullptr;
  }

  if (!data || size == 0) {
    setError("Invalid data");
    return nullptr;
  }

  auto wire = loadWireFromMemory(data, size);
  if (!wire) {
    setError("Failed to load wire from memory");
    return nullptr;
  }

  // Hand the caller an owning SHWireRef (newRef heap-allocates a shared_ptr copy
  // so the wire outlives this function's local).
  return wire->newRef();
}

bool shards_esp32_schedule_wire(SHWireRef wire) {
  using namespace shards::esp32;
  clearError();

  if (!g_initialized) {
    setError("Runtime not initialized");
    return false;
  }

  if (!wire) {
    setError("Wire is null");
    return false;
  }

  if (!g_mesh) {
    setError("Mesh not available");
    return false;
  }

  auto sharedWire = SHWire::sharedFromRef(wire);
  g_mesh->schedule(sharedWire);
  ESP_LOGI(TAG, "Scheduled wire: %s", sharedWire->name.c_str());
  return true;
}

SHRunWireOutput shards_esp32_run_wire(SHWireRef wire, SHVar input) {
  using namespace shards::esp32;
  clearError();

  SHRunWireOutput result{};
  result.state = SHRunWireOutputState::Failed;

  if (!g_initialized) {
    setError("Runtime not initialized");
    return result;
  }

  if (!wire) {
    setError("Wire is null");
    return result;
  }

  auto sharedWire = SHWire::sharedFromRef(wire);

  // Create context for this wire execution
  shards::Coroutine coro;
  auto context = std::make_shared<SHContext>(&coro, sharedWire.get());

  // Run the wire
  result = shards::runWire(sharedWire.get(), context.get(), input);

  if (result.state == SHRunWireOutputState::Failed) {
    setError("Wire execution failed");
  }

  return result;
}

void shards_esp32_tick(void) {
  using namespace shards::esp32;

  if (!g_initialized || !g_mesh) {
    return;
  }

  g_mesh->tick();
}

bool shards_esp32_has_running_wires(void) {
  using namespace shards::esp32;

  if (!g_initialized || !g_mesh) {
    return false;
  }

  return !g_mesh->empty();
}

const char* shards_esp32_get_last_error(void) {
  using namespace shards::esp32;

  if (g_lastError.empty()) {
    return nullptr;
  }
  return g_lastError.c_str();
}

} // extern "C"

#endif // SH_FREERTOS
