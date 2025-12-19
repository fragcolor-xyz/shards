/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

#include "hdl.hpp"
#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/iterator.hpp>

namespace shards {
namespace hdl {

// The main "Module" shard that compiles a Shards block to Verilog
// Similar to GFX.Feature/GFX.EffectPass in the shader translator
struct ModuleShard {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }
  static SHOptionalString help() {
    return SHCCSTR("Compiles the given shard sequence into Verilog HDL code for FPGA synthesis. "
                   "Use regular shards like Math.Add, Math.Multiply inside, along with "
                   "HDL.Input and HDL.Output for port declarations.");
  }

  PARAM_VAR(_moduleName, "Name", "The name of the Verilog module to generate.", {CoreInfo::StringType});
  PARAM(ShardsVar, _contents, "Contents", "The hardware description shards to compile.", {CoreInfo::ShardsOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_moduleName), PARAM_IMPL_FOR(_contents));

  std::string _output;

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    // Compose the contents to validate them
    _contents.compose(data);
    return CoreInfo::StringType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    // Create translation context
    HDLContext hdlCtx(getHDLRegistry());

    // Get module name
    std::string moduleName = "top";
    if (!_moduleName->isNone() && _moduleName->valueType == SHType::String) {
      moduleName = SHSTRVIEW(_moduleName);
    }

    // Begin module
    hdlCtx.beginModule(moduleName);

    // Process each shard in the contents - just like shader translator does!
    const auto &shards = _contents.shards();
    for (uint32_t i = 0; i < shards.len; i++) {
      Shard *shard = shards.elements[i];
      if (!shard) continue;

      auto *handler = hdlCtx.registry.resolve(shard);
      if (!handler) {
        throw HDLError(fmt::format("No HDL translation for shard: {} - this shard cannot be synthesized to hardware",
                                   shard->name(shard)));
      }
      handler->translate(shard, hdlCtx);
    }

    // End module and emit Verilog
    auto module = hdlCtx.endModule();
    _output = emitVerilog(*module);

    return Var(_output);
  }
};

} // namespace hdl
} // namespace shards

SHARDS_REGISTER_FN(hdl) {
  using namespace shards::hdl;

  // Register the main Module shard
  REGISTER_SHARD("HDL.Module", ModuleShard);

  // Register all HDL translation handlers
  registerAllHDLShards();
}
