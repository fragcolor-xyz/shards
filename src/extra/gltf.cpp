/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2021 Giovanni Petrantoni */

#include "blocks/shared.hpp"
#include "runtime.hpp"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

// A gltf model can contain a lot of things...
// Let's go over them in few iterations...
// [X] vertices and indices
// TODO

namespace chainblocks {
namespace gltf {
struct Load {
  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyTableType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    return awaitne(context, [&]() {
      cgltf_options options{};
      cgltf_data *data{nullptr};
      cgltf_result result =
          cgltf_parse_file(&options, input.payload.stringValue, &data);
      if (result == cgltf_result_success) {
        DEFER(cgltf_free(data));
      }
      return Var::Empty;
    });
  }
};

void registerBlocks() { REGISTER_CBLOCK("GLTF.Load", Load); }
} // namespace gltf
} // namespace chainblocks