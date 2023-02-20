#include "boost/filesystem/path.hpp"
#include "common_types.hpp"
#include "extra/gfx/shards_types.hpp"
#include "gfx/drawables/mesh_tree_drawable.hpp"
#include "gfx/unique_id.hpp"
#include "shards_types.hpp"
#include "shards_utils.hpp"
#include "drawable_utils.hpp"
#include <memory>
#include <shards_macros.hpp>
#include <foundation.hpp>
#include <gfx/gltf/gltf.hpp>
#include <gfx/paths.hpp>
#include <linalg_shim.hpp>
#include <runtime.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include "params.hpp"

using namespace shards;

namespace gfx {

struct GLTFShard {
  static inline Type PathInputType = CoreInfo::StringType;
  static inline Type ByteInputType = CoreInfo::BytesType;
  static inline Type TransformVarType = Type::VariableOf(CoreInfo::Float4x4Type);

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return Types::Drawable; }

  PARAM_PARAMVAR(_transform, "Transform", "The transform variable to use", {CoreInfo::NoneType, TransformVarType});
  PARAM_PARAMVAR(_path, "Path", "The path to load the model from",
                 {CoreInfo::NoneType, CoreInfo::StringType, CoreInfo::StringVarType});
  PARAM_PARAMVAR(_bytes, "Bytes", "The bytes to load the model from",
                 {CoreInfo::NoneType, CoreInfo::BytesType, CoreInfo::BytesVarType});
  PARAM_PARAMVAR(_copy, "Copy", "Reference to another glTF model to copy",
                 {CoreInfo::NoneType, Type::VariableOf(Types::Drawable)});
  PARAM_EXT(ParamVar, _params, Types::ParamsParameterInfo);
  PARAM_EXT(ParamVar, _features, Types::FeaturesParameterInfo);
  PARAM_IMPL(GLTFShard, PARAM_IMPL_FOR(_transform), PARAM_IMPL_FOR(_path), PARAM_IMPL_FOR(_bytes), PARAM_IMPL_FOR(_copy),
             PARAM_IMPL_FOR(_params), PARAM_IMPL_FOR(_features));

  enum LoadMode {
    Invalid,
    LoadFileStatic,
    LoadFileDynamic,
    LoadMemory,
    LoadCopy,
  };

  LoadMode _loadMode{};
  bool _hasConstTransform{};
  SHDrawable *_drawable{};
  bool _dynamicsApplied{};

  MeshTreeDrawable::Ptr &getMeshTreeDrawable() { return std::get<MeshTreeDrawable::Ptr>(_drawable->drawable); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    bool havePath = !_path.isNone();
    bool haveBytes = !_bytes.isNone();
    bool haveCopy = !_copy.isNone();

    size_t numSources = (havePath ? 1 : 0) + (haveBytes ? 1 : 0) + (haveCopy ? 1 : 0);
    if (numSources > 1) {
      throw ComposeError("glTF can only have one source (Path, Bytes or Copy)");
    } else if (havePath) {
      if (_path.isNotNullConstant()) {
        _loadMode = LoadFileStatic;
      } else {
        _loadMode = LoadFileDynamic;
      }
    } else if (haveBytes) {
      _loadMode = LoadMemory;
    } else if (haveCopy) {
      _loadMode = LoadCopy;
    } else {
      throw ComposeError("glTF Binary, file path or copy source required");
    }

    return Types::Drawable;
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);

    _drawable = Types::DrawableObjectVar.New();

    switch (_loadMode) {
    case LoadMode::LoadFileStatic:
      _drawable->drawable = loadGltfFromFile(_path.get().payload.stringValue);
      break;
    case LoadMode::LoadFileDynamic:
    case LoadMode::LoadMemory:
    case LoadMode::LoadCopy:
      _drawable->drawable = MeshTreeDrawable::Ptr();
      break;
    default:
      throw std::out_of_range("glTF load mode");
    }

    _dynamicsApplied = false;
  }

  void cleanup() {
    PARAM_CLEANUP()

    if (_drawable) {
      Types::DrawableObjectVar.Release(_drawable);
      _drawable = {};
    }
  }

  void applyRecursiveParams(SHContext *context) {
    auto &meshTreeDrawable = getMeshTreeDrawable();

    MeshTreeDrawable::foreach (meshTreeDrawable, [&](MeshTreeDrawable::Ptr item) {
      for (auto &drawable : item->drawables) {
        if (!_params.isNone()) {
          initShaderParams(context, _params.get().payload.tableValue, drawable->parameters);
        }

        drawable->features.clear();
        if (!_features.isNone()) {
          applyFeatures(context, drawable->features, _features.get());
        }
      }
    });
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto &drawable = getMeshTreeDrawable();
    if (!drawable) {
      switch (_loadMode) {
      case LoadFileDynamic:
        drawable = loadGltfFromFile(_path.get().payload.stringValue);
        break;
      case LoadMemory:
        drawable = loadGltfFromMemory(_bytes.get().payload.bytesValue, _bytes.get().payload.bytesSize);
        break;
      case LoadCopy: {
        auto shOther = varAsObjectChecked<SHDrawable>(_copy.get(), Types::Drawable);
        MeshTreeDrawable::Ptr &other = *std::get_if<MeshTreeDrawable::Ptr>(&shOther->drawable);
        drawable = std::static_pointer_cast<MeshTreeDrawable>(other->clone());
      } break;
      default:
        throw std::out_of_range("glTF load mode");
        break;
      }
    }

    if (!_transform.isNone()) {
      drawable->transform = toFloat4x4(_transform.get());
    }

    // Only apply recursive parameters once
    if (!_dynamicsApplied) {
      applyRecursiveParams(context);
    }

    return Types::DrawableObjectVar.Get(_drawable);
  }
};

void registerGLTFShards() { REGISTER_SHARD("GFX.glTF", GLTFShard); }
} // namespace gfx