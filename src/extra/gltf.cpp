/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2021 Giovanni Petrantoni */

#include "blocks/shared.hpp"
#include "runtime.hpp"

#include <filesystem>
namespace fs = std::filesystem;
using LastWriteTime = decltype(fs::last_write_time(fs::path()));

#include <nlohmann/json.hpp>
#include <stb_image.h>
#include <stb_image_write.h>

#define TINYGLTF_NO_INCLUDE_JSON
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#define TINYGLTF_USE_CPP14
// #define TINYGLTF_ENABLE_DRACO
#include <tinygltf/tiny_gltf.h>
using GLTFModel = tinygltf::Model;
using namespace tinygltf;
#undef Model

// A gltf model can contain a lot of things...
// Let's go over them in few iterations...
// [X] vertices and indices
// TODO

namespace chainblocks {
namespace gltf {
struct Model {
  GLTFModel gltf;
  size_t fileNameHash;
  LastWriteTime fileLastWrite;
};

struct Load {
  static constexpr uint32_t ModelCC = 'gltf';
  static inline Type ModelType{
      {CBType::Object, {.object = {.vendorId = CoreCC, .typeId = ModelCC}}}};
  static inline Type ModelVarType = Type::VariableOf(ModelType);
  static inline ObjectVar<Model> Var{"GLTF-Model", CoreCC, ModelCC};

  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return ModelType; }

  Model *_model{nullptr};
  TinyGLTF _loader;

  void cleanup() {
    if (_model) {
      Var.Release(_model);
      _model = nullptr;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    return awaitne(context, [&]() {
      std::string err;
      std::string warn;
      bool success = false;
      const auto filename = input.payload.stringValue;
      fs::path filepath(filename);
      const auto &ext = filepath.extension();
      const auto hash = std::hash<std::string_view>()(filename);

      if (_model) {
        Var.Release(_model);
        _model = nullptr;
      }
      _model = Var.New();

      _model->fileNameHash = hash;
      _model->fileLastWrite = fs::last_write_time(filepath);
      if (ext == ".glb") {
        _loader.LoadBinaryFromFile(&_model->gltf, &err, &warn, filename);
      } else {
        _loader.LoadASCIIFromFile(&_model->gltf, &err, &warn, filename);
      }

      if (!warn.empty()) {
        LOG(WARNING) << "GLTF warning: " << warn;
      }
      if (!err.empty()) {
        LOG(ERROR) << "GLTF error: " << err;
      }
      if (!success) {
        throw ActivationError("Failed to parse glTF.");
      }

      return Var.Get(_model);
    });
  }
};

void registerBlocks() { REGISTER_CBLOCK("GLTF.Load", Load); }
} // namespace gltf
} // namespace chainblocks