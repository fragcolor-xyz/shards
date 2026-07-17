// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2020-2024 Fragcolor Pte. Ltd.

#include "platform.hpp"

#if SH_FREERTOS

#include "shards_esp32.h"
#include <shards/core/serialization.hpp>
#include <shards/core/foundation.hpp>
#include <esp_log.h>
#include <cstdio>
#include <vector>

static const char* TAG = "shards_fs";

namespace shards::esp32 {

// VFS-based binary reader for deserializing wires
struct VFSReader {
  FILE* file{nullptr};
  bool valid_{false};

  VFSReader(const char* path) {
    file = fopen(path, "rb");
    valid_ = (file != nullptr);
    if (!valid_) {
      ESP_LOGE(TAG, "Failed to open file: %s", path);
    }
  }

  ~VFSReader() {
    if (file) {
      fclose(file);
      file = nullptr;
    }
  }

  void operator()(uint8_t* buf, size_t size) {
    if (file && size > 0) {
      size_t read = fread(buf, 1, size, file);
      if (read != size) {
        ESP_LOGW(TAG, "Short read: expected %d, got %d", (int)size, (int)read);
      }
    }
  }

  bool valid() const { return valid_; }
};

// Memory-based binary reader for embedded wire data
struct MemoryReader {
  const uint8_t* data;
  size_t size;
  size_t offset{0};

  MemoryReader(const uint8_t* data, size_t size) : data(data), size(size) {}

  void operator()(uint8_t* buf, size_t len) {
    if (offset + len > size) {
      ESP_LOGW(TAG, "Read past end: offset=%d, len=%d, size=%d",
               (int)offset, (int)len, (int)size);
      len = (offset < size) ? (size - offset) : 0;
    }
    if (len > 0) {
      memcpy(buf, data + offset, len);
      offset += len;
    }
  }

  bool valid() const { return data != nullptr && size > 0; }
};

std::shared_ptr<SHWire> loadWireFromPath(const char* path) {
  VFSReader reader(path);
  if (!reader.valid()) {
    return nullptr;
  }

  try {
    Serialization serial;
    SHVar wire_var{};
    serial.deserialize(reader, wire_var);

    if (wire_var.valueType != SHType::Wire) {
      ESP_LOGE(TAG, "Deserialized value is not a wire");
      destroyVar(wire_var);
      return nullptr;
    }

    auto wire = SHWire::sharedFromRef(wire_var.payload.wireValue);
    ESP_LOGI(TAG, "Loaded wire: %s", wire->name.c_str());
    return wire;
  } catch (const std::exception& e) {
    ESP_LOGE(TAG, "Failed to deserialize wire: %s", e.what());
    return nullptr;
  }
}

std::shared_ptr<SHWire> loadWireFromMemory(const uint8_t* data, size_t size) {
  MemoryReader reader(data, size);
  if (!reader.valid()) {
    return nullptr;
  }

  try {
    Serialization serial;
    SHVar wire_var{};
    serial.deserialize(reader, wire_var);

    if (wire_var.valueType != SHType::Wire) {
      ESP_LOGE(TAG, "Deserialized value is not a wire");
      destroyVar(wire_var);
      return nullptr;
    }

    auto wire = SHWire::sharedFromRef(wire_var.payload.wireValue);
    ESP_LOGI(TAG, "Loaded wire from memory: %s", wire->name.c_str());
    return wire;
  } catch (const std::exception& e) {
    ESP_LOGE(TAG, "Failed to deserialize wire from memory: %s", e.what());
    return nullptr;
  }
}

} // namespace shards::esp32

#endif // SH_FREERTOS
