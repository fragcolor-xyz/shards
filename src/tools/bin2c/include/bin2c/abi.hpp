#pragma once
#include <stdint.h>

namespace bin2c {

enum Flags : uint64_t {
  FLAGS_None = 0,
};

struct File {
  uint64_t length{};
  uint64_t flags{};
  uint8_t data[1];
};
} // namespace bin2c
