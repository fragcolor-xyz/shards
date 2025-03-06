#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/core/wire_runtime.hpp>
#include <shards/common_types.hpp>
#include <boost/core/span.hpp>
#include <string>
#include <spdlog/fmt/fmt.h>

namespace shards::Debug {
struct WireVariables {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR("A shard that can be set to break in the debugger at a specific location"); }

  PARAM_PARAMVAR(_tag, "Tag", "Any tag to identify this debug shard", {shards::CoreInfo::NoneType, shards::CoreInfo::AnyType});
  PARAM_PARAMVAR(_inspect, "Inspect", "Anything to visualize", {shards::CoreInfo::NoneType, shards::CoreInfo::AnyType});
  PARAM_IMPL(PARAM_IMPL_FOR(_tag), PARAM_IMPL_FOR(_inspect));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return data.inputType;
  }

  static inline constexpr size_t colSizes[] = {
      20,
      20,
      20,
      20,
  };

  struct ColFormatter {
    std::string line;
    void padTill(size_t col) {
      size_t toPad = 0;
      for (size_t i = 0; i < std::min(col, std::size(colSizes)); i++) {
        toPad += colSizes[col];
      }
      if (line.size() >= toPad) {
        line += " ";
      } else {
        while (line.size() < toPad) {
          line += " ";
        }
      }
    }
    void append(const std::string &str) { line += str; }
  };

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto wire = shContext->currentWire();
    auto &varInfo = wire->runtimeVariableInfo;

    if (!_tag.isNone()) {
      SHLOG_INFO("Wire \"{}\" variables({}):", wire->name, _tag.get());
    } else {
      SHLOG_INFO("Wire \"{}\" variables:", wire->name);
    }

    if (!varInfo) {
      SHLOG_INFO("  No variable info available");
      return input;
    }

    // Helper lambda to get variable type string
    auto getVariableTypeString = [&](size_t index) -> std::string {
      if (index < varInfo->refOffset()) {
        return "Local";
      } else if (index < varInfo->inheritedOffset()) {
        return "Ref";
      } else if (index < varInfo->externalOffset()) {
        return "Inherited";
      } else if (index < varInfo->globalOffset()) {
        return "External";
      } else {
        return "Global";
      }
    };

    ColFormatter fmt;
    fmt.append("[id] [name]");
    fmt.padTill(1);
    fmt.append("[addr]");
    fmt.padTill(2);
    fmt.append("[type]");
    fmt.padTill(3);
    fmt.append("[value]");
    SHLOG_INFO("  {}", fmt.line);

    // Iterate through all variables
    for (size_t i = 0; i < varInfo->variables.size(); ++i) {
      const auto &varDecl = varInfo->variables[i];
      auto slot = varInfo->variableSlots[i];
      const auto typeStr = getVariableTypeString(i);

      ColFormatter fmt;
      fmt.append(fmt::format("[{}] {}", i, varDecl.name));
      fmt.padTill(1);
      fmt.append(fmt::format("0x{:x}", reinterpret_cast<uintptr_t>(slot)));
      fmt.padTill(2);
      fmt.append(typeStr);
      fmt.padTill(3);
      if (slot) {
        fmt.append(fmt::format("{}", *slot));
      } else {
        fmt.append("<null>");
      }

      // Get the variable slot
      SHLOG_INFO("  {}", fmt.line);
    }

    return input;
  }
};

SHARDS_REGISTER_FN(wires) { REGISTER_SHARD("Debug.WireVariables", WireVariables); }
} // namespace shards::Debug