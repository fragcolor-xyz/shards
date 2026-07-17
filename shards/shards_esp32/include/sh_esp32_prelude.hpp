#pragma once
// ESP32 force-include prelude. The core .cpp TUs are normally compiled with the
// desktop precompiled header (shards/core/pch.cpp -> runtime.hpp + spdlog +
// boost/filesystem). IDF doesn't use that PCH, so each TU is missing the prelude
// it assumes (std containers, Tracy no-op macros, SHLOG_*, complete SHWire/etc).
// This mirrors that prelude MINUS boost/filesystem (guarded out on ESP32).
#include <vector>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <set>
#include <unordered_map>
#include <tracy/Wrapper.hpp>
#include <spdlog/spdlog.h>
#include <shards/log/log.hpp>
#include <shards/core/runtime.hpp>
