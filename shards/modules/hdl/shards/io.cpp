/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

// HDL-specific shards: Input and Output port declarations
// These ARE new shards because they have no software equivalent

#include "../hdl.hpp"
#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>

namespace shards {
namespace hdl {

// HDL.Input - Declares an input port (HDL-specific, no software equivalent)
struct InputShard {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::IntType; }
  static SHOptionalString help() {
    return SHCCSTR("Declares an input port for HDL synthesis. Returns a reference to the port signal.");
  }

  PARAM_VAR(_name, "Name", "The name of the input port.", {CoreInfo::StringType});
  PARAM_VAR(_width, "Width", "The bit width of the input (default: 8).", {CoreInfo::IntType});
  PARAM_VAR(_signed, "Signed", "Whether the input is signed (default: false).", {CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_name), PARAM_IMPL_FOR(_width), PARAM_IMPL_FOR(_signed));

  InputShard() {
    _width = Var(8);
    _signed = Var(false);
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return CoreInfo::IntType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    // In software mode, return 0 as placeholder
    return Var(0);
  }
};

// HDL handler for Input
struct InputHandler : public IHDLHandler {
  void translate(Shard *shard, HDLContext &context) override {
    SHVar nameVar = shard->getParam(shard, 0);
    SHVar widthVar = shard->getParam(shard, 1);
    SHVar signedVar = shard->getParam(shard, 2);

    std::string name;
    if (nameVar.valueType == SHType::String) {
      name = SHSTRVIEW(nameVar);
    } else {
      throw HDLError("HDL.Input requires a Name parameter");
    }

    int width = 8;
    if (widthVar.valueType == SHType::Int) {
      width = static_cast<int>(widthVar.payload.intValue);
    }

    bool isSigned = false;
    if (signedVar.valueType == SHType::Bool) {
      isSigned = signedVar.payload.boolValue;
    }

    Type type = BitWidth(width, isSigned);

    // Add input port to the module
    context.addInput(name, type);

    // Set current value to a reference to this input
    context.setCurrentValue(makeSignalRef(name, type), type);
  }
};

// HDL.Output - Declares an output port (HDL-specific, no software equivalent)
struct OutputShard {
  static SHTypesInfo inputTypes() { return CoreInfo::IntType; }
  static SHTypesInfo outputTypes() { return CoreInfo::IntType; }
  static SHOptionalString help() {
    return SHCCSTR("Declares an output port for HDL synthesis. Assigns the input value to the port.");
  }

  PARAM_VAR(_name, "Name", "The name of the output port.", {CoreInfo::StringType});
  PARAM_VAR(_width, "Width", "The bit width of the output (default: inferred).", {CoreInfo::IntType});
  PARAM_VAR(_signed, "Signed", "Whether the output is signed (default: false).", {CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_name), PARAM_IMPL_FOR(_width), PARAM_IMPL_FOR(_signed));

  OutputShard() {
    _width = Var(0); // 0 = infer from input
    _signed = Var(false);
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return data.inputType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    // In software mode, pass through
    return input;
  }
};

// HDL handler for Output
struct OutputHandler : public IHDLHandler {
  void translate(Shard *shard, HDLContext &context) override {
    SHVar nameVar = shard->getParam(shard, 0);
    SHVar widthVar = shard->getParam(shard, 1);
    SHVar signedVar = shard->getParam(shard, 2);

    std::string name;
    if (nameVar.valueType == SHType::String) {
      name = SHSTRVIEW(nameVar);
    } else {
      throw HDLError("HDL.Output requires a Name parameter");
    }

    if (!context.hasCurrentValue()) {
      throw HDLError("HDL.Output requires an input value");
    }

    int width = 0;
    if (widthVar.valueType == SHType::Int) {
      width = static_cast<int>(widthVar.payload.intValue);
    }

    bool isSigned = false;
    if (signedVar.valueType == SHType::Bool) {
      isSigned = signedVar.payload.boolValue;
    }

    // Determine output type
    Type type;
    if (width == 0) {
      auto currentType = context.getCurrentType();
      if (currentType) {
        type = *currentType;
      } else {
        type = BitWidth(8, false);
      }
    } else {
      type = BitWidth(width, isSigned);
    }

    // Add output port
    context.addOutput(name, type);

    // Assign current value to output
    context.addAssign(name);
  }
};

// Static handler instances
static InputHandler inputHandler;
static OutputHandler outputHandler;

void registerHDLIOShards() {
  auto &registry = getHDLRegistry();
  registry.registerHandler("HDL.Input", &inputHandler);
  registry.registerHandler("HDL.Output", &outputHandler);
}

} // namespace hdl
} // namespace shards

// Register HDL-specific shards
SHARDS_REGISTER_FN(hdl_io) {
  REGISTER_SHARD("HDL.Input", shards::hdl::InputShard);
  REGISTER_SHARD("HDL.Output", shards::hdl::OutputShard);
}
