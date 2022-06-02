#include "shards_types.hpp"
#include <shards_macros.hpp>
#include <foundation.hpp>
#include <gfx/gltf/gltf.hpp>
#include <gfx/paths.hpp>
#include <runtime.hpp>
#include <spdlog/spdlog.h>

using namespace shards;

namespace gfx {

struct GLTFShard {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return Types::DrawableHierarchy; }

  static SHParametersInfo parameters() {
    static Parameters params{
        {"StaticPath", SHCCSTR("The static path to load a model from during warmup"), {CoreInfo::StringType}},
    };
    return params;
  }

  std::string _staticModelPath;
  DrawableHierarchyPtr _staticModel;

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _staticModelPath = value.payload.stringValue;
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_staticModelPath);
    default:
      return Var::Empty;
    }
  }

  SHTypeInfo compose(SHInstanceData &data) {
    if (!_staticModelPath.empty()) {
      // Load statically
    } else {
      SHType inputBasicType = data.inputType.basicType;
      if (inputBasicType != SHType::Bytes && inputBasicType != SHType::String) {
        throw ComposeError("glTF Binary or file path required when not loading a static model");
      }
      // Dynamic
    }
    return Types::DrawableHierarchy;
  }

  void warmup(SHContext *context) {
    if (!_staticModelPath.empty()) {
      // Since loading from files is more of a debug funcitonality, try to load using the chainblocks relative path
      std::string resolvedPath = gfx::resolveDataPath(_staticModelPath).string();

      SPDLOG_DEBUG("Loading static glTF model from {}", resolvedPath);
      _staticModel = loadGlTF(resolvedPath.c_str());
    }
  }

  void cleanup() { _staticModel.reset(); }

  SHVar activate(SHContext *context, const SHVar &input) {
    SHDrawableHierarchy *result = Types::DrawableHierarchyObjectVar.New();
    try {
      if (_staticModel) {
        result->drawableHierarchy = _staticModel->clone();
      } else {
        throw ActivationError("No glTF model was loaded");
      }

    } catch (...) {
      Types::DrawableHierarchyObjectVar.Release(result);
      throw;
    }

    return Types::DrawableHierarchyObjectVar.Get(result);
  }
};

void registerGLTFShards() { REGISTER_SHARD("GFX.glTF", GLTFShard); }
} // namespace gfx