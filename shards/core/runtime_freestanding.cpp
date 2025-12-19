/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

// Minimal runtime for freestanding/bare-metal builds
// This provides only the essential functions needed for running pre-serialized wires

#include "platform.hpp"

#if !SH_FREESTANDING
#error "runtime_freestanding.cpp should only be compiled for freestanding builds"
#endif

#include <shards/shards.h>
#include <shards/shards.hpp>
#include <shards/utility.hpp>
#include <shards/log/log.hpp>

#include <cstring>
#include <map>
#include <string>
#include <unordered_map>

namespace shards {

// Forward declaration of InternalCore for OwnedVar
struct InternalCore;
using OwnedVar = TOwnedVar<InternalCore>;

// Forward declarations
struct Globals;
Globals &GetGlobals();

// Globals structure - minimal for freestanding
struct Globals {
  std::unordered_map<std::string, SHVar> Settings;
  std::unordered_map<std::string_view, SHShardConstructor> ShardsRegister;
  std::unordered_map<std::string_view, std::string_view> ShardNamesToFullTypeNames;
  std::unordered_map<int64_t, SHObjectInfo> ObjectTypesRegister;
  std::unordered_map<std::string_view, int64_t> ObjectTypesRegisterByName;
  std::unordered_map<int64_t, SHEnumInfo> EnumTypesRegister;
  std::unordered_map<std::string_view, int64_t> EnumTypesRegisterByName;
  std::string RootPath;
  std::string ExePath;
  std::unordered_map<uint32_t, SHOptionalString> *CompressedStrings{nullptr};
};

static Globals *g_globals = nullptr;

Globals &GetGlobals() {
  if (!g_globals) {
    static Globals globals;
    g_globals = &globals;
  }
  return *g_globals;
}

// Array operations from foundation.hpp
template <typename T> inline void arrayGrow(T &arr, size_t addlen, size_t min_cap = 4) {
  size_t min_len = arr.len + addlen;
  if (min_len > min_cap) min_cap = min_len;
  if (min_cap <= arr.cap) return;
  if (min_cap < 2 * arr.cap) min_cap = 2 * arr.cap;
  auto newbuf = new (std::align_val_t{16}) uint8_t[sizeof(arr.elements[0]) * min_cap];
  if (arr.elements) {
    memcpy(newbuf, arr.elements, sizeof(arr.elements[0]) * arr.len);
    ::operator delete[](arr.elements, std::align_val_t{16});
  }
  arr.elements = (decltype(arr.elements))newbuf;
  size_t size = sizeof(arr.elements[0]) * (min_cap - arr.len);
  memset(arr.elements + arr.len, 0x0, size);
  arr.cap = uint32_t(min_cap);
}

template <typename T> inline void arrayFree(T &arr) {
  if (arr.elements) {
    ::operator delete[](arr.elements, std::align_val_t{16});
  }
  memset(&arr, 0x0, sizeof(T));
}

// String operations
void stringGrow(SHStringPayload *str, uint32_t newCap) { arrayGrow(*str, newCap); }
void stringFree(SHStringPayload *str) { arrayFree(*str); }

// String storage - simple map for freestanding
static std::map<uint32_t, std::string> s_stringStorage;

SHString getString(uint32_t crc) {
  auto it = s_stringStorage.find(crc);
  if (it != s_stringStorage.end()) {
    return it->second.c_str();
  }
  return nullptr;
}

void setString(uint32_t crc, SHString str) {
  s_stringStorage[crc] = str ? str : "";
}

// Compressed strings - stub for freestanding (no compression support)
#ifndef SH_STRIP_HELP_STRINGS
#ifdef SH_COMPRESSED_STRINGS
SHOptionalString getCompiledCompressedString(uint32_t crc) {
  return SHOptionalString{nullptr, crc};
}
#else
SHOptionalString setCompiledCompressedString(uint32_t crc, const char *str) {
  return SHOptionalString{str, crc};
}
#endif
#endif

// Shard registration
void registerShard(std::string_view name, SHShardConstructor constructor, std::string_view fullTypeName) {
  GetGlobals().ShardsRegister[name] = constructor;
  if (!fullTypeName.empty()) {
    GetGlobals().ShardNamesToFullTypeNames[name] = fullTypeName;
  }
}

Shard *createShard(std::string_view name) {
  auto &globals = GetGlobals();
  auto it = globals.ShardsRegister.find(name);
  if (it != globals.ShardsRegister.end()) {
    return it->second();
  }
  return nullptr;
}

void registerShards() {
  // Empty for freestanding - shards are registered at compile time
}

// Object type registration
void registerObjectType(int32_t vendorId, int32_t typeId, SHObjectInfo info) {
  int64_t id = (int64_t(vendorId) << 32) | uint32_t(typeId);
  GetGlobals().ObjectTypesRegister[id] = info;
  if (info.name) {
    GetGlobals().ObjectTypesRegisterByName[info.name] = id;
  }
}

const SHObjectInfo *findObjectInfo(int32_t vendorId, int32_t typeId) {
  int64_t id = (int64_t(vendorId) << 32) | uint32_t(typeId);
  auto it = GetGlobals().ObjectTypesRegister.find(id);
  if (it != GetGlobals().ObjectTypesRegister.end()) {
    return &it->second;
  }
  return nullptr;
}

int64_t findObjectTypeId(std::string_view name) {
  auto it = GetGlobals().ObjectTypesRegisterByName.find(name);
  if (it != GetGlobals().ObjectTypesRegisterByName.end()) {
    return it->second;
  }
  return 0;
}

// Enum type registration
void registerEnumType(int32_t vendorId, int32_t typeId, SHEnumInfo info) {
  int64_t id = (int64_t(vendorId) << 32) | uint32_t(typeId);
  GetGlobals().EnumTypesRegister[id] = info;
  if (info.name) {
    GetGlobals().EnumTypesRegisterByName[info.name] = id;
  }
}

const SHEnumInfo *findEnumInfo(int32_t vendorId, int32_t typeId) {
  int64_t id = (int64_t(vendorId) << 32) | uint32_t(typeId);
  auto it = GetGlobals().EnumTypesRegister.find(id);
  if (it != GetGlobals().EnumTypesRegister.end()) {
    return &it->second;
  }
  return nullptr;
}

int64_t findEnumId(std::string_view name) {
  auto it = GetGlobals().EnumTypesRegisterByName.find(name);
  if (it != GetGlobals().EnumTypesRegisterByName.end()) {
    return it->second;
  }
  return 0;
}

// Image operations
void imageIncRef(SHImage *ptr) {
  if (ptr) {
    ptr->refCount++;
  }
}

void imageDecRef(SHImage *ptr) {
  if (ptr) {
    ptr->refCount--;
    if (ptr->refCount == 0) {
      if (ptr->free) {
        ptr->free(ptr);
      } else {
        ::operator delete(ptr, std::align_val_t{16});
      }
    }
  }
}

SHImage *imageNew(uint32_t dataLen) {
  size_t totalSize = sizeof(SHImage) + dataLen;
  auto *img = static_cast<SHImage *>(::operator new(totalSize, std::align_val_t{16}));
  memset(img, 0, totalSize);
  img->refCount = 1;
  img->data = reinterpret_cast<uint8_t *>(img + 1);
  return img;
}

uint32_t imageGetPixelSize(SHImage *img) {
  if (!img) return 0;
  uint32_t channelSize = 1;
  if (img->flags & SHIMAGE_FLAGS_16BITS_INT) channelSize = 2;
  else if (img->flags & SHIMAGE_FLAGS_32BITS_FLOAT) channelSize = 4;
  return img->channels * channelSize;
}

uint32_t imageGetRowStride(SHImage *img) {
  if (!img) return 0;
  if (img->rowStride > 0) return img->rowStride;
  return img->width * imageGetPixelSize(img);
}

uint32_t imageDeriveDataLength(SHImage *ptr) {
  if (!ptr) return 0;
  return imageGetRowStride(ptr) * ptr->height;
}

SHImage *imageClone(SHImage *ptr) {
  if (!ptr) return nullptr;
  uint32_t dataLen = imageDeriveDataLength(ptr);
  SHImage *clone = imageNew(dataLen);
  clone->width = ptr->width;
  clone->height = ptr->height;
  clone->rowStride = ptr->rowStride;
  clone->channels = ptr->channels;
  clone->flags = ptr->flags;
  if (dataLen > 0 && ptr->data) {
    memcpy(clone->data, ptr->data, dataLen);
  }
  return clone;
}

// Reference counting for shards
void incRef(ShardPtr shard) {
  if (shard) {
    shard->refCount++;
  }
}

void decRef(ShardPtr shard) {
  if (shard) {
    shard->refCount--;
    if (shard->refCount == 0) {
      shard->destroy(shard);
    }
  }
}

// Variable operations - stubs
SHVar *findVariable(SHContext *ctx, std::string_view name) { return nullptr; }
SHVar *referenceVariable(SHContext *ctx, std::string_view name) { return nullptr; }
SHVar *referenceGlobalVariable(SHContext *ctx, std::string_view name) { return nullptr; }
SHVar *referenceWireVariable(SHWire *wire, std::string_view name) { return nullptr; }
void releaseVariable(SHVar *variable) {}
void setSharedVariable(std::string_view name, const SHVar &value) {}
void unsetSharedVariable(std::string_view name) {}
SHVar getSharedVariable(std::string_view name) { return Var::Empty; }

// Context operations - stubs
entt::id_type findId(SHContext *ctx) noexcept { return 0; }
SHWireState suspend(SHContext *context, double seconds) { return SHWireState::Continue; }
SHWireState unsafeSuspend(SHContext *context, double seconds) { return SHWireState::Continue; }

// Shard activation - stub
SHWireState activateShards(ShardPtr *shards, SHContext *context, const SHVar &wireInput, SHVar &output) noexcept {
  return SHWireState::Continue;
}

SHWireState activateShards2(ShardPtr *shards, SHContext *context, const SHVar &wireInput, SHVar &output) noexcept {
  return SHWireState::Continue;
}

// Wire composition - stubs
SHComposeResult composeWire(const Shards wire, SHInstanceData data) {
  SHComposeResult result{};
  result.failed = false;
  return result;
}

SHComposeResult composeWireNoExcept(const SHWire *wire, SHInstanceData &data) noexcept {
  SHComposeResult result{};
  result.failed = false;
  return result;
}

SHComposeResult composeShardsNoExcept(Shards wire, SHInstanceData &data) noexcept {
  SHComposeResult result{};
  result.failed = false;
  return result;
}

// Error formatting - stub
std::string formatErrorStack(const std::vector<shards::Error> &errorStack, std::string_view indent) {
  return "";
}

void appendIndented(std::string &out, std::string_view in, std::string_view indent) {
  out += indent;
  out += in;
}

// Type matching - stub
bool matchTypes(const SHTypeInfo &inputType, const SHTypeInfo &receiverType, bool isParameter, bool strict,
                bool relaxEmptyTableCheck, bool relaxEmptySeqCheck) {
  return true;
}

// Wire abort
void abortWire(SHContext *ctx, std::string_view errorText) {}

// Trigger callbacks - stubs
void triggerVarValueChange(SHContext *context, const SHVar *name, const SHVar *key, bool isGlobal, const SHVar *var) {}
void triggerVarValueChange(SHWire *w, const SHVar *name, const SHVar *key, bool isGlobal, const SHVar *var) {}

} // namespace shards

// Global init - minimal for freestanding
void shInit() {
  static bool initialized = false;
  if (initialized) return;
  initialized = true;
  shards::GetGlobals();
  shards::registerShards();
}

void shInitLog() {}

// C interface
extern "C" {

void shards_install_signal_handlers() {}

void triggerVarValueChange(SHContext *ctx, const SHVar *name, const SHVar *key, bool isGlobal, const SHVar *var) {
  shards::triggerVarValueChange(ctx, name, key, isGlobal, var);
}

SHVar serializeVar(const SHVar *var) { return shards::Var::Empty; }
SHVar deserializeVar(const SHVar *bytes_buffer_var) { return shards::Var::Empty; }

static bool sh_current_interface_loaded = false;
static SHCore sh_current_interface{};

SHCore *__cdecl shardsInterface(uint32_t abi_version) {
  if (SHARDS_CURRENT_ABI != abi_version) {
    return nullptr;
  }

  if (sh_current_interface_loaded) {
    return &sh_current_interface;
  }

  sh_current_interface_loaded = true;
  auto *result = &sh_current_interface;

  result->alloc = [](uint32_t size) -> void * {
    auto mem = ::operator new(size, std::align_val_t{16});
    memset(mem, 0, size);
    return mem;
  };

  result->free = [](void *ptr) { ::operator delete(ptr, std::align_val_t{16}); };

  result->stringGrow = &shards::stringGrow;
  result->stringFree = &shards::stringFree;

  result->registerShard = [](const char *fullName, SHShardConstructor constructor) noexcept {
    shards::registerShard(fullName, constructor, std::string_view{});
  };

  result->registerObjectType = [](int32_t vendorId, int32_t typeId, SHObjectInfo info) noexcept {
    shards::registerObjectType(vendorId, typeId, info);
  };

  result->registerEnumType = [](int32_t vendorId, int32_t typeId, SHEnumInfo info) noexcept {
    shards::registerEnumType(vendorId, typeId, info);
  };

  return result;
}

void shards_log(int level, SHStringWithLen msg, const char *file, const char *function, int line) {}
void shards_log_flush() {}
void shards_decompress_strings() {}

} // extern "C"
