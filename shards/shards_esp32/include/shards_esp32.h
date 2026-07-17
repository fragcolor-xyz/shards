// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2020-2024 Fragcolor Pte. Ltd.

#ifndef SHARDS_ESP32_H
#define SHARDS_ESP32_H

#include <shards/shards.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the Shards ESP32 runtime.
 * Must be called before any other shards_esp32_* functions.
 * Registers all available shards and initializes the mesh.
 */
void shards_esp32_init(void);

/**
 * Shutdown the Shards ESP32 runtime.
 * Stops all running wires and cleans up resources.
 */
void shards_esp32_shutdown(void);

/**
 * Load a wire from a VFS path (SPIFFS, LittleFS, SD, etc.).
 * The wire is deserialized from the binary format.
 *
 * @param path VFS path to the serialized wire file (e.g., "/spiffs/main.shw")
 * @return Wire reference on success, NULL on failure
 */
SHWireRef shards_esp32_load_wire(const char* path);

/**
 * Load a wire from memory buffer.
 * Useful for wires embedded as const data in the firmware.
 *
 * @param data Pointer to serialized wire data
 * @param size Size of the data in bytes
 * @return Wire reference on success, NULL on failure
 */
SHWireRef shards_esp32_load_wire_from_memory(const uint8_t* data, size_t size);

/**
 * Schedule a wire for execution on the mesh.
 * The wire will start running on the next tick.
 *
 * @param wire Wire reference to schedule
 * @return true on success, false on failure
 */
bool shards_esp32_schedule_wire(SHWireRef wire);

/**
 * Run a single wire to completion (blocking).
 * For one-shot wires that don't loop.
 *
 * @param wire Wire reference to run
 * @param input Input value for the wire
 * @return Output structure with result and state
 */
SHRunWireOutput shards_esp32_run_wire(SHWireRef wire, SHVar input);

/**
 * Tick all scheduled wires once.
 * Should be called regularly from the main loop or a FreeRTOS task.
 * Each wire will execute until it suspends or completes.
 */
void shards_esp32_tick(void);

/**
 * Check if any wires are still running.
 *
 * @return true if at least one wire is running
 */
bool shards_esp32_has_running_wires(void);

/**
 * Get the last error message, if any.
 *
 * @return Error message string, or NULL if no error
 */
const char* shards_esp32_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // SHARDS_ESP32_H
