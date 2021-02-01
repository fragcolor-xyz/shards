/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2021 Giovanni Petrantoni */

#include "blocks/shared.hpp"
#include "runtime.hpp"

#include <nlohmann/json.hpp>
#include <stb_image.h>
#include <stb_image_write.h>

#define TINYGLTF_NO_INCLUDE_JSON
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#define TINYGLTF_USE_CPP14
// #define TINYGLTF_ENABLE_DRACO
#include <tinygltf/tiny_gltf.h>

using namespace tinygltf;

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
      Model model;
      TinyGLTF loader;
      std::string err;
      std::string warn;

      bool ret = loader.LoadASCIIFromFile(&model, &err, &warn,
                                          input.payload.stringValue);
      // bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, argv[1]); //
      // for binary glTF(.glb)

      if (!warn.empty()) {
        LOG(WARNING) << "GLTF warning: " << warn;
      }
      if (!err.empty()) {
        LOG(ERROR) << "GLTF error: " << err;
      }
      if (!ret) {
        throw ActivationError("Failed to parse glTF.");
      }

      return Var::Empty;
    });
  }
};

void registerBlocks() { REGISTER_CBLOCK("GLTF.Load", Load); }
} // namespace gltf
} // namespace chainblocks