/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2021 Giovanni Petrantoni */

#include "./bgfx.hpp"
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

/*
TODO:
GLTF.Draw - depending on GFX blocks
GLTF.Simulate - depending on physics simulation blocks
*/
namespace chainblocks {
namespace gltf {
struct Model {
  struct {
    bgfx::VertexBufferHandle vb = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle ib = BGFX_INVALID_HANDLE;
  } gfx;

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
      GLTFModel gltf;
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
        _loader.LoadBinaryFromFile(&gltf, &err, &warn, filename);
      } else {
        _loader.LoadASCIIFromFile(&gltf, &err, &warn, filename);
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

struct Draw : public BGFX::BaseConsumer {
  ParamVar _model{};
  CBVar *_bgfxContext{nullptr};
  std::array<CBExposedTypeInfo, 1> _required;

  static CBTypesInfo inputTypes() { return CoreInfo::Float4x4Types; }
  static CBTypesInfo outputTypes() { return CoreInfo::Float4x4Types; }

  static inline Parameters Params{{"Model",
                                   CBCCSTR("The GLTF model to render."),
                                   {Load::ModelType, Load::ModelVarType}}};
  static CBParametersInfo parameters() { return Params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _model = value;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _model;
      break;
    default:
      throw InvalidParameterIndex();
    }
  }

  CBExposedTypesInfo requiredVariables() {
    int idx = -1;
    if (_model.isVariable()) {
      idx++;
      _required[idx].name = _model.variableName();
      _required[idx].help = CBCCSTR("The required model.");
      _required[idx].exposedType = Load::ModelType;
    }
    if (idx == -1) {
      return {};
    } else {
      return {_required.data(), uint32_t(idx + 1), 0};
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    BGFX::BaseConsumer::compose(data);

    if (data.inputType.seqTypes.elements[0].basicType == CBType::Seq) {
      // TODO
      OVERRIDE_ACTIVATE(data, activate);
    } else {
      OVERRIDE_ACTIVATE(data, activateSingle);
    }
    return data.inputType;
  }

  void warmup(CBContext *context) { _model.warmup(context); }

  void cleanup() { _model.cleanup(); }

  CBVar activate(CBContext *context, const CBVar &input) {
    throw ActivationError("Not yet implemented.");
    return input;
  }

  CBVar activateSingle(CBContext *context, const CBVar &input) { return input; }
};

void registerBlocks() {
  REGISTER_CBLOCK("GLTF.Load", Load);
  REGISTER_CBLOCK("GLTF.Draw", Draw);
}
} // namespace gltf
} // namespace chainblocks