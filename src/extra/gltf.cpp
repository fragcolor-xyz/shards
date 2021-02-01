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
enum class DataType {
  Meshes,
  Materials,
  Animations,
};

struct Load {
  static constexpr uint32_t GLTFTypesCC = 'gltf';
  static inline Type DataTypeType{
      {CBType::Enum,
       {.enumeration = {.vendorId = CoreCC, .typeId = GLTFTypesCC}}}};
  static inline EnumInfo<DataType> DataTypeInfo{"GLTFData", CoreCC,
                                                GLTFTypesCC};

  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyTableType; }

  std::vector<DataType> _types;

  CBVar activate(CBContext *context, const CBVar &input) {
    return awaitne(context, [&]() {
      cgltf_options options{};
      cgltf_data *data{nullptr};
      cgltf_result result =
          cgltf_parse_file(&options, input.payload.stringValue, &data);
      if (result == cgltf_result_success) {
        for (auto type : _types) {
          switch (type) {
          case DataType::Meshes: {
            const auto nmeshes = data->meshes_count;
            for (cgltf_size i = 0; i < nmeshes; i++) {
              const cgltf_mesh &mesh = data->meshes[i];
            }
          } break;
          default:
            throw ActivationError("GLTF data type not yet supported.");
          }
        }
        DEFER(cgltf_free(data));
      }
      return Var::Empty;
    });
  }
};

void registerBlocks() { REGISTER_CBLOCK("GLTF.Load", Load); }
} // namespace gltf
} // namespace chainblocks