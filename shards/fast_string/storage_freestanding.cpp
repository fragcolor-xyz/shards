// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2020-2024 Fragcolor Pte. Ltd.

// Minimal fast_string storage implementation for freestanding/bare-metal targets
// Uses std::map instead of boost::container::flat_map

#include "storage.hpp"
#include <map>
#include <vector>
#include <string>
#include <cstring>

namespace shards::fast_string {

// Simple storage using std::map for string interning
struct Storage {
  // Map from string content to ID
  std::map<std::string, uint64_t, std::less<>> stringToId;
  // Reverse lookup: ID to string_view (stored strings)
  std::vector<std::string> idToString;

  uint64_t store(std::string_view sv) {
    // Check if already stored
    auto it = stringToId.find(sv);
    if (it != stringToId.end()) {
      return it->second;
    }

    // Store new string
    uint64_t id = idToString.size();
    idToString.emplace_back(sv);
    stringToId.emplace(idToString.back(), id);
    return id;
  }

  std::string_view load(uint64_t id) {
    if (id >= idToString.size()) {
      return {};
    }
    return idToString[id];
  }
};

Storage *storage = nullptr;

void init() {
  if (!storage) {
    storage = new Storage();
  }
}

uint64_t store(std::string_view sv) {
  if (!storage) {
    init();
  }
  return storage->store(sv);
}

std::string_view load(uint64_t id) {
  if (!storage) {
    return {};
  }
  return storage->load(id);
}

} // namespace shards::fast_string
