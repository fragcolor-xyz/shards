/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2021 Giovanni Petrantoni */

#include "./bgfx.hpp"
#include "blocks/shared.hpp"
#include "linalg-shim.hpp"
#include "runtime.hpp"

#include <deque>
#include <filesystem>
#include <optional>
namespace fs = std::filesystem;
using LastWriteTime = decltype(fs::last_write_time(fs::path()));

#include <boost/algorithm/string.hpp>

#include <nlohmann/json.hpp>
#include <stb_image.h>
#include <stb_image_write.h>

#define TINYGLTF_IMPLEMENTATION
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
struct GFXPrimitive {
  bgfx::VertexBufferHandle vb = BGFX_INVALID_HANDLE;
  bgfx::IndexBufferHandle ib = BGFX_INVALID_HANDLE;
  bgfx::VertexLayout layout{};

  GFXPrimitive() {}

  GFXPrimitive(GFXPrimitive &&other) {
    std::swap(vb, other.vb);
    std::swap(ib, other.ib);
    std::swap(layout, other.layout);
  }

  ~GFXPrimitive() {
    if (vb.idx != bgfx::kInvalidHandle) {
      bgfx::destroy(vb);
    }
    if (ib.idx != bgfx::kInvalidHandle) {
      bgfx::destroy(ib);
    }
  }
};

using GFXPrimitiveRef = std::reference_wrapper<GFXPrimitive>;

struct GFXMesh {
  std::string name;
  std::deque<GFXPrimitiveRef> primitives;
};

using GFXMeshRef = std::reference_wrapper<GFXMesh>;

struct Node;
using NodeRef = std::reference_wrapper<Node>;

struct Node {
  std::string name;
  Mat4 transform;

  std::optional<GFXMeshRef> mesh;

  std::deque<NodeRef> children;
};

struct Model {
  std::deque<Node> nodes;
  std::deque<GFXMesh> gfxMeshes;
  std::deque<GFXPrimitive> gfxPrimitives;
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
  size_t _fileNameHash;
  LastWriteTime _fileLastWrite;

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

      if (!fs::exists(filepath)) {
        throw ActivationError("GLTF model file does not exist.");
      }

      if (_model) {
        Var.Release(_model);
        _model = nullptr;
      }
      _model = Var.New();

      _fileNameHash = hash;
      _fileLastWrite = fs::last_write_time(filepath);
      if (ext == ".glb") {
        success = _loader.LoadBinaryFromFile(&gltf, &err, &warn, filename);
      } else {
        success = _loader.LoadASCIIFromFile(&gltf, &err, &warn, filename);
      }

      if (!warn.empty()) {
        LOG(WARNING) << "GLTF warning: " << warn;
      }
      if (!err.empty()) {
        LOG(ERROR) << "GLTF error: " << err;
      }
      if (!success) {
        throw ActivationError("Failed to parse GLTF.");
      }

      if (gltf.defaultScene == -1) {
        throw ActivationError("GLTF model had no default scene.");
      }

      const auto &scene = gltf.scenes[gltf.defaultScene];
      for (const int gltfNodeIdx : scene.nodes) {
        const auto &glnode = gltf.nodes[gltfNodeIdx];
        Node node{glnode.name};

        if (glnode.matrix.size() != 0) {
          node.transform = Mat4::FromVector(glnode.matrix);
        } else {
          const auto t = linalg::translation_matrix(
              glnode.translation.size() != 0
                  ? Vec3::FromVector(glnode.translation)
                  : Vec3());
          const auto r = linalg::rotation_matrix(
              glnode.rotation.size() != 0 ? Vec4::FromVector(glnode.rotation)
                                          : Vec4::Quaternion());
          const auto s = linalg::scaling_matrix(
              glnode.scale.size() != 0 ? Vec3::FromVector(glnode.scale)
                                       : Vec3(1.0, 1.0, 1.0));
          node.transform = linalg::mul(linalg::mul(t, r), s);
        }

        // if (glnode.skin != -1) {
        //   // TODO
        // }

        if (glnode.mesh != -1) {
          const auto &glmesh = gltf.meshes[glnode.mesh];
          GFXMesh mesh{glmesh.name};
          for (const auto &glprims : glmesh.primitives) {
            GFXPrimitive prims{};
            // we gotta do few things here
            // build a layout
            // populate vb and ib
            for (const auto &[attributeName, attributeIdx] :
                 glprims.attributes) {
              if (attributeName == "POSITION") {
                prims.layout.add(bgfx::Attrib::Position, 3,
                                 bgfx::AttribType::Float);
              } else if (attributeName == "NORMAL") {
                prims.layout.add(bgfx::Attrib::Normal, 3,
                                 bgfx::AttribType::Float);
              } else if (attributeName == "TANGENT") {
                prims.layout.add(bgfx::Attrib::Tangent, 3,
                                 bgfx::AttribType::Float);
                // we also precompute bitangents here
                prims.layout.add(bgfx::Attrib::Bitangent, 3,
                                 bgfx::AttribType::Float);
              } else if (boost::starts_with(attributeName, "TEXCOORD_")) {
                int strIndex = std::stoi(attributeName.substr(9));
                if (strIndex >= 8) {
                  throw ActivationError("GLTF TEXCOORD_ limit exceeded.");
                }
                auto texcoord = decltype(bgfx::Attrib::TexCoord0)(
                    int(bgfx::Attrib::TexCoord0) + strIndex);
                prims.layout.add(texcoord, 2, bgfx::AttribType::Float);
              } else if (boost::starts_with(attributeName, "COLOR_")) {
                int strIndex = std::stoi(attributeName.substr(6));
                if (strIndex >= 4) {
                  throw ActivationError("GLTF COLOR_ limit exceeded.");
                }
                auto texcoord = decltype(bgfx::Attrib::Color0)(
                    int(bgfx::Attrib::Color0) + strIndex);
                prims.layout.add(texcoord, 4, bgfx::AttribType::Uint8, true);
              }
              // TODO JOINTS_ and WEIGHTS_
            }
            prims.layout.end();

            mesh.primitives.emplace_back(
                _model->gfxPrimitives.emplace_back(std::move(prims)));
          }
          node.mesh = _model->gfxMeshes.emplace_back(std::move(mesh));
        }

        // if (glnode.camera != -1) {
        //   // TODO
        // }

        _model->nodes.emplace_back(std::move(node));
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

#ifdef CB_INTERNAL_TESTS
#include "gltf_tests.cpp"
#endif