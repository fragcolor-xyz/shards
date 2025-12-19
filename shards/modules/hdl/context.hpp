/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

#ifndef SHARDS_MODULE_HDL_CONTEXT_HPP
#define SHARDS_MODULE_HDL_CONTEXT_HPP

#include "types.hpp"
#include "blocks.hpp"
#include <string>
#include <map>
#include <vector>
#include <optional>
#include <memory>

namespace shards {
namespace hdl {

// Forward declarations
struct HDLRegistry;

// Information about a signal in the current context
struct SignalInfo {
  std::string name;       // The signal name
  Type type;              // The signal type
  bool isPort;            // Is this a port or internal wire?
  PortDirection direction; // Only meaningful if isPort

  SignalInfo(const std::string &name, const Type &type, bool isPort = false,
             PortDirection dir = PortDirection::Input)
      : name(name), type(type), isPort(isPort), direction(dir) {}
};

// Translation context for HDL module generation
// Tracks signals, manages the current value on the "stack", and builds the module IR
struct HDLContext {
  HDLRegistry &registry;

  // The module being built
  std::unique_ptr<Module> module;

  // Signal table: maps signal names to their info
  std::map<std::string, SignalInfo> signals;

  // The current value "on the stack" (result of last shard)
  BlockPtr currentValue;
  std::optional<Type> currentType;

  // Temporary variable counter for unique names
  int tempCounter = 0;

  HDLContext(HDLRegistry &registry);
  HDLContext(const HDLContext &) = delete;
  HDLContext &operator=(const HDLContext &) = delete;

  // Start building a new module
  void beginModule(const std::string &name);

  // Finish building the current module and return it
  std::unique_ptr<Module> endModule();

  // Add an input port
  void addInput(const std::string &name, const Type &type);

  // Add an output port
  void addOutput(const std::string &name, const Type &type);

  // Add an internal wire
  void addWire(const std::string &name, const Type &type);

  // Add an assignment: target = currentValue
  void addAssign(const std::string &target);

  // Add an assignment with explicit value
  void addAssign(const std::string &target, BlockPtr value);

  // Look up a signal by name
  std::optional<SignalInfo> findSignal(const std::string &name) const;

  // Get a unique temporary name
  std::string getUniqueName(const std::string &hint = "tmp");

  // Set the current value (the "stack top")
  void setCurrentValue(BlockPtr value, const Type &type);

  // Take ownership of the current value
  BlockPtr takeCurrentValue();

  // Get a reference to the current value (clones it)
  BlockPtr getCurrentValueRef() const;

  // Get the type of the current value
  std::optional<Type> getCurrentType() const;

  // Clear the current value
  void clearCurrentValue();

  // Check if there's a current value
  bool hasCurrentValue() const;
};

// Error type for HDL translation errors
struct HDLError : public std::runtime_error {
  HDLError(const std::string &msg) : std::runtime_error(msg) {}
};

} // namespace hdl
} // namespace shards

#endif // SHARDS_MODULE_HDL_CONTEXT_HPP
