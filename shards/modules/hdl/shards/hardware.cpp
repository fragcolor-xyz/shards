/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

// Core HDL handlers for Set, Get, and other fundamental shards

#include "../hdl.hpp"
#include <shards/core/shared.hpp>

namespace shards {
namespace hdl {

// Handler for Set - assigns current value to a wire/variable
struct SetHandler : public IHDLHandler {
  void translate(Shard *shard, HDLContext &context) override {
    // Get variable name from first parameter
    SHVar nameVar = shard->getParam(shard, 0);

    std::string name;
    if (nameVar.valueType == SHType::String) {
      name = SHSTRVIEW(nameVar);
    } else if (nameVar.valueType == SHType::ContextVar) {
      name = nameVar.payload.stringValue;
    } else {
      throw HDLError("Set requires a variable name");
    }

    if (!context.hasCurrentValue()) {
      throw HDLError("Set requires an input value");
    }

    // Get type BEFORE taking value (take clears it)
    auto type = context.getCurrentType();
    if (!type) {
      throw HDLError("Cannot determine type for Set");
    }

    auto value = context.takeCurrentValue();

    // Check if current value is a signal reference to an existing signal
    // If so, this is just an alias (like `HDL.Input(...) >= a`)
    if (auto *ref = dynamic_cast<SignalRef *>(value.get())) {
      // Check if this is referencing an existing input/output port
      auto existingSignal = context.findSignal(ref->name);
      if (existingSignal && existingSignal->isPort) {
        // Just create an alias - don't create a new wire
        // Register the variable name as pointing to this signal
        context.signals.emplace(name, *existingSignal);
        context.setCurrentValue(makeSignalRef(ref->name, *type), *type);
        return;
      }
    }

    // Otherwise, create a new wire and assign
    context.addWire(name, *type);
    context.addAssign(name, value->clone());

    // Keep the value on the stack (Set is passthrough)
    context.setCurrentValue(makeSignalRef(name, *type), *type);
  }
};

// Handler for Get - retrieves a wire/variable value
struct GetHandler : public IHDLHandler {
  void translate(Shard *shard, HDLContext &context) override {
    SHVar nameVar = shard->getParam(shard, 0);

    std::string name;
    if (nameVar.valueType == SHType::String) {
      name = SHSTRVIEW(nameVar);
    } else if (nameVar.valueType == SHType::ContextVar) {
      name = nameVar.payload.stringValue;
    } else {
      throw HDLError("Get requires a variable name");
    }

    // Look up the signal
    auto signalOpt = context.findSignal(name);
    if (!signalOpt) {
      throw HDLError(fmt::format("Signal '{}' not found", name));
    }

    context.setCurrentValue(makeSignalRef(name, signalOpt->type), signalOpt->type);
  }
};

// Handler for Ref - same as Get for HDL purposes
struct RefHandler : public IHDLHandler {
  void translate(Shard *shard, HDLContext &context) override {
    SHVar nameVar = shard->getParam(shard, 0);

    std::string name;
    if (nameVar.valueType == SHType::String) {
      name = SHSTRVIEW(nameVar);
    } else if (nameVar.valueType == SHType::ContextVar) {
      name = nameVar.payload.stringValue;
    } else {
      throw HDLError("Ref requires a variable name");
    }

    auto signalOpt = context.findSignal(name);
    if (!signalOpt) {
      throw HDLError(fmt::format("Signal '{}' not found", name));
    }

    context.setCurrentValue(makeSignalRef(name, signalOpt->type), signalOpt->type);
  }
};

// Handler for Const - creates a literal value
struct ConstHandler : public IHDLHandler {
  void translate(Shard *shard, HDLContext &context) override {
    SHVar valueVar = shard->getParam(shard, 0);

    if (valueVar.valueType == SHType::Int) {
      // Infer width from value - for now use 32-bit
      Type type = U32();
      context.setCurrentValue(makeLiteral(valueVar.payload.intValue, type), type);
    } else {
      throw HDLError("Const only supports integer values for HDL synthesis");
    }
  }
};

// Static handler instances
static SetHandler setHandler;
static GetHandler getHandler;
static RefHandler refHandler;
static ConstHandler constHandler;

void registerHDLCoreShards() {
  auto &registry = getHDLRegistry();

  // Register handlers for core variable shards
  registry.registerHandler("Set", &setHandler);
  registry.registerHandler("Get", &getHandler);
  registry.registerHandler("Ref", &refHandler);
  registry.registerHandler("Const", &constHandler);
}

} // namespace hdl
} // namespace shards
