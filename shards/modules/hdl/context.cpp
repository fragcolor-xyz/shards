/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

#include "context.hpp"
#include "registry.hpp"
#include <spdlog/fmt/fmt.h>

namespace shards {
namespace hdl {

HDLContext::HDLContext(HDLRegistry &registry) : registry(registry) {}

void HDLContext::beginModule(const std::string &name) {
  module = std::make_unique<Module>(name);
  signals.clear();
  currentValue.reset();
  currentType.reset();
  tempCounter = 0;
}

std::unique_ptr<Module> HDLContext::endModule() {
  currentValue.reset();
  currentType.reset();
  signals.clear();
  return std::move(module);
}

void HDLContext::addInput(const std::string &name, const Type &type) {
  if (!module) {
    throw HDLError("No module being built");
  }
  if (signals.count(name)) {
    throw HDLError(fmt::format("Signal '{}' already defined", name));
  }

  module->addPort(name, type, PortDirection::Input);
  signals.emplace(name, SignalInfo(name, type, true, PortDirection::Input));
}

void HDLContext::addOutput(const std::string &name, const Type &type) {
  if (!module) {
    throw HDLError("No module being built");
  }
  if (signals.count(name)) {
    throw HDLError(fmt::format("Signal '{}' already defined", name));
  }

  module->addPort(name, type, PortDirection::Output);
  signals.emplace(name, SignalInfo(name, type, true, PortDirection::Output));
}

void HDLContext::addWire(const std::string &name, const Type &type) {
  if (!module) {
    throw HDLError("No module being built");
  }
  if (signals.count(name)) {
    throw HDLError(fmt::format("Signal '{}' already defined", name));
  }

  module->addWire(name, type);
  signals.emplace(name, SignalInfo(name, type, false));
}

void HDLContext::addAssign(const std::string &target) {
  if (!module) {
    throw HDLError("No module being built");
  }
  if (!currentValue) {
    throw HDLError("No current value to assign");
  }

  module->addAssign(target, std::move(currentValue));
  currentValue.reset();
  currentType.reset();
}

void HDLContext::addAssign(const std::string &target, BlockPtr value) {
  if (!module) {
    throw HDLError("No module being built");
  }
  if (!value) {
    throw HDLError("Cannot assign null value");
  }

  module->addAssign(target, std::move(value));
}

std::optional<SignalInfo> HDLContext::findSignal(const std::string &name) const {
  auto it = signals.find(name);
  if (it != signals.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::string HDLContext::getUniqueName(const std::string &hint) {
  return fmt::format("_hdl_{}_{}", hint, tempCounter++);
}

void HDLContext::setCurrentValue(BlockPtr value, const Type &type) {
  currentValue = std::move(value);
  currentType = type;
}

BlockPtr HDLContext::takeCurrentValue() {
  currentType.reset();
  return std::move(currentValue);
}

BlockPtr HDLContext::getCurrentValueRef() const {
  if (currentValue) {
    return currentValue->clone();
  }
  return nullptr;
}

std::optional<Type> HDLContext::getCurrentType() const { return currentType; }

void HDLContext::clearCurrentValue() {
  currentValue.reset();
  currentType.reset();
}

bool HDLContext::hasCurrentValue() const { return currentValue != nullptr; }

} // namespace hdl
} // namespace shards
