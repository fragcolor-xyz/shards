#include "boost/filesystem/path.hpp"
#include <shards/common_types.hpp>
#include "gfx/gltf/animation.hpp"
#include <shards/core/params.hpp>
#include <shards/shards.hpp>
#include "shards_types.hpp"
#include "shards_utils.hpp"
#include "drawable_utils.hpp"
#include "anim/types.hpp"
#include "anim/path.hpp"
#include <memory>
#include <deque>
#include <optional>
#include <shards/core/shards_macros.hpp>
#include <shards/core/foundation.hpp>
#include <gfx/drawables/mesh_tree_drawable.hpp>
#include <gfx/gltf/gltf.hpp>
#include <gfx/paths.hpp>
#include <tracy/Tracy.hpp>
#include <shards/linalg_shim.hpp>
#include <shards/core/runtime.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string_view>

using namespace shards;

namespace gfx {

#define GLTF

static const char internalComponentIdentifier = '$';

// Checks if the given name points to an internal glTF component (translation/rotation/scale)
static bool isGltfBuiltinTarget(std::string_view path) { return path.starts_with(internalComponentIdentifier); }

static std::optional<animation::BuiltinTarget> getGltfBuiltinTarget(std::string_view path) {
  if (!isGltfBuiltinTarget(path))
    return std::nullopt;
  path = path.substr(1);
  if (path == "t") {
    return animation::BuiltinTarget::Translation;
  } else if (path == "r") {
    return animation::BuiltinTarget::Rotation;
  } else if (path == "s") {
    return animation::BuiltinTarget::Scale;
  } else {
    throw std::runtime_error(fmt::format("Invalid internal glTF target {}", path));
  }
}

static OwnedVar getGltfBuiltinTargetPath(animation::BuiltinTarget target) {
  OwnedVar result;
  char pathStr[] = {internalComponentIdentifier, 0, 0};
  switch (target) {
  case animation::BuiltinTarget::Rotation:
    pathStr[1] = 'r';
    break;
  case animation::BuiltinTarget::Scale:
    pathStr[1] = 's';
    break;
  case animation::BuiltinTarget::Translation:
    pathStr[1] = 't';
    break;
  }
  pathStr[2] = 0;
  cloneVar(result, Var(pathStr, 2));
  return result;
}

SeqVar getAnimationPath(MeshTreeDrawable::Ptr node) {
  SeqVar result;
  auto rootNode = MeshTreeDrawable::findRoot(node);
  MeshTreeDrawable::traverseDown(rootNode, node, [&](auto &node) {
    SHVar tmp{
        .payload = {.stringValue = node->label.c_str(), .stringLen = uint32_t(node->label.size())},
        .valueType = SHType::String,
    };
    SHVar cloned{};
    cloneVar(cloned, tmp);
    result.push_back(cloned);
  });

  return result;
};

SeqVar getAnimationPath(MeshTreeDrawable::Ptr node, animation::BuiltinTarget target) {
  SeqVar result = getAnimationPath(node);
  result.emplace_back() = getGltfBuiltinTargetPath(target);
  return result;
};

struct GLTFShard {
  static inline Type PathInputType = CoreInfo::StringType;
  static inline Type ByteInputType = CoreInfo::BytesType;
  static inline Type TransformVarType = Type::VariableOf(CoreInfo::Float4x4Type);
  static inline Type AnimationTable = Type::TableOf(Animations::Types::Animation);

  static SHTypesInfo inputTypes() { return CoreInfo::Float4x4Type; }
  static SHTypesInfo outputTypes() { return Types::Drawable; }

  PARAM_PARAMVAR(_path, "Path", "The path to load the model from",
                 {CoreInfo::NoneType, CoreInfo::StringType, CoreInfo::StringVarType});
  PARAM_PARAMVAR(_bytes, "Bytes", "The bytes to load the model from",
                 {CoreInfo::NoneType, CoreInfo::BytesType, CoreInfo::BytesVarType});
  PARAM_PARAMVAR(_copy, "Copy", "Reference to another glTF model to copy",
                 {CoreInfo::NoneType, Type::VariableOf(Types::Drawable)});
  PARAM_EXT(ParamVar, _params, Types::ParamsParameterInfo);
  PARAM_EXT(ParamVar, _features, Types::FeaturesParameterInfo);
  PARAM(ShardsVar, _animController, "AnimationController", "The animation controller", {CoreInfo::ShardsOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_path), PARAM_IMPL_FOR(_bytes), PARAM_IMPL_FOR(_copy), PARAM_IMPL_FOR(_params),
             PARAM_IMPL_FOR(_features), PARAM_IMPL_FOR(_animController));

  enum LoadMode {
    Invalid,
    LoadFileStatic,
    LoadFileDynamic,
    LoadMemory,
    LoadCopy,
  };

  std::optional<glTF> _model;
  LoadMode _loadMode{};
  bool _hasConstTransform{};
  SHDrawable *_drawable{};
  bool _dynamicsApplied{};
  TableVar _animations;

  MeshTreeDrawable::Ptr &getMeshTreeDrawable() { return std::get<MeshTreeDrawable::Ptr>(_drawable->drawable); }

  bool hasAnimationController() const { return _animController.shards().len > 0; }

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

    if (hasAnimationController()) {
      SHInstanceData childData = data;
      childData.inputType = AnimationTable;
      _animController.compose(childData);
      SHLOG_TRACE("Checking animation frame data {}: {}", data.wire->name, _animController.composeResult().outputType);
      if (!shards::matchTypes(_animController.composeResult().outputType, Animations::Types::AnimationValues, false, true))
        throw std::runtime_error(fmt::format("Invalid animation frame data: {}, expected: {}",
                                             _animController.composeResult().outputType, Animations::Types::AnimationValues));
    }

    return Types::Drawable;
  }

  void shardifyAnimationData() {
    ZoneScoped;
    for (auto &[name, animation] : _model->animations) {
      SeqVar &tracks = _animations.get<SeqVar>(Var(name));
      for (auto &track : animation.tracks) {
        TableVar trackTable;
        trackTable.get<SeqVar>(Var("Path")) = getAnimationPath(track.targetNode.lock(), track.target);

        std::optional<Var> interpolationValue;
        if (track.interpolation != animation::Interpolation::Linear) {
          auto &enumType = Animations::Types::InterpolationEnumInfo::Type;
          switch (track.interpolation) {
          case animation::Interpolation::Step:
            interpolationValue = Var::Enum(Animations::Interpolation::Step, enumType);
            break;
          case animation::Interpolation::Cubic:
            interpolationValue = Var::Enum(Animations::Interpolation::Cubic, enumType);
            break;
          default:
            throw std::out_of_range("Unsupported value");
            break;
          }
        }

        SeqVar frames;
        frames.resize(track.times.size());

        size_t frameIndex{};
        for (auto &time : track.times) {
          TableVar frameTableVar;

          frameTableVar.get<Var>(Var("Time")) = Var{time};

          if (interpolationValue) {
            frameTableVar.get<Var>(Var("Interpolation")) = *interpolationValue;
          }

          if (track.interpolation == animation::Interpolation::Cubic) {
            std::visit([&](auto &&v) -> void { frameTableVar.get<Var>(Var("In")) = toVar(v); },
                       track.getValue(frameIndex * 3 + 0));
            std::visit([&](auto &&v) -> void { frameTableVar.get<Var>(Var("Value")) = toVar(v); },
                       track.getValue(frameIndex * 3 + 1));
            std::visit([&](auto &&v) -> void { frameTableVar.get<Var>(Var("Out")) = toVar(v); },
                       track.getValue(frameIndex * 3 + 2));
          } else {
            Var &value = frameTableVar.get<Var>(Var("Value"));
            std::visit([&](auto &&v) -> void { value = toVar(v); }, track.getValue(frameIndex));
          }

          frames[frameIndex] = std::move(frameTableVar.asOwned());
          ++frameIndex;
        }

        trackTable.get<SeqVar>(Var("Frames")) = std::move(frames);
        tracks.emplace_back() = std::move(trackTable.asOwned());
      }
    }
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);

    _drawable = Types::DrawableObjectVar.New();
    _drawable->drawable = MeshTreeDrawable::Ptr();

    switch (_loadMode) {
    case LoadMode::LoadFileStatic:
      _model.emplace(loadGltfFromFile(SHSTRVIEW(_path.get())));
      break;
    case LoadMode::LoadFileDynamic:
    case LoadMode::LoadMemory:
    case LoadMode::LoadCopy:
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

    _model.reset();
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

  // Modifies path and returns the mesh tree node found
  // the path will contain the elements after the node that was found
  // there can only be builtin target identifiers left in the path
  MeshTreeDrawable::Ptr findNode(Animations::Path &p) {
    MeshTreeDrawable::Ptr node = _model->root;
    while (true) {
      if (!node)
        return MeshTreeDrawable::Ptr();

      // Stop when end of path is reached
      // or we encounter a builtin target identifier
      if (!p || isGltfBuiltinTarget(p.getHead()))
        return node;

      for (auto &child : node->getChildren()) {
        if (child->label == p.getHead()) {
          node = child;
          p = p.next();
          goto _next;
        }
      }
      return MeshTreeDrawable::Ptr();
    _next:;
    }
  }

  std::optional<animation::BuiltinTarget> findTarget(Animations::Path &p) {
    if (p.length == 1) {
      return getGltfBuiltinTarget(p.getHead());
    }
    return std::nullopt;
  }

  void applyFrame(const MeshTreeDrawable::Ptr &node, animation::BuiltinTarget target, const SHVar &value) {
    switch (target) {
    case animation::BuiltinTarget::Rotation:
      node->trs.rotation = toVec<float4>(value);
      break;
    case animation::BuiltinTarget::Scale:
      node->trs.scale = toVec<float3>(value);
      break;
    case animation::BuiltinTarget::Translation:
      node->trs.translation = toVec<float3>(value);
      break;
    }
  }

  void applyAnimationData(SeqVar &data) {
    ZoneScoped;
    for (auto &v : data) {
      TableVar &valueTable = (TableVar &)v;
      auto &pathSeq = valueTable.get<SeqVar>(Var("Path"));

      Animations::Path path(pathSeq);
      MeshTreeDrawable::Ptr node = findNode(path);
      if (!node) {
        SHLOG_WARNING("Animation node path not found: {}", pathSeq);
        continue;
      }
      auto target = findTarget(path);
      if (!target) {
        SHLOG_WARNING("Animation node path not found: {}", pathSeq);
        continue;
      }

      auto &value = valueTable.get<Var>(Var("Value"));
      applyFrame(node, target.value(), value);
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto &drawable = getMeshTreeDrawable();
    if (!drawable) {
      switch (_loadMode) {
      case LoadFileDynamic:
        _model.emplace(loadGltfFromFile(SHSTRVIEW(_path.get())));
        break;
      case LoadMemory:
        _model.emplace(loadGltfFromMemory(_bytes.get().payload.bytesValue, _bytes.get().payload.bytesSize));
        break;
      case LoadCopy: {
        auto &shOther = varAsObjectChecked<SHDrawable>(_copy.get(), Types::Drawable);
        MeshTreeDrawable::Ptr &other = *std::get_if<MeshTreeDrawable::Ptr>(&shOther.drawable);
        _model.emplace();
        _model->root = std::static_pointer_cast<MeshTreeDrawable>(other->clone());
        _model->animations = shOther.animations;
      } break;
      case LoadMode::LoadFileStatic:
        assert(_model); // Loaded in warmup
        break;
      default:
        throw std::out_of_range("glTF load mode");
        break;
      }

      _drawable->drawable = _model->root;
      _drawable->animations = _model->animations;

      if (hasAnimationController()) {
        shardifyAnimationData();
      }
    }

    if (_animController.shards().len > 0) {
      SHVar animationData;
      {
        ZoneScopedN("AnimationController");
        SHWireState state = _animController.activate(context, _animations, animationData);
        if (state == SHWireState::Error) {
          throw std::runtime_error("Animation controller failed");
        }
      }

      applyAnimationData((SeqVar &)animationData);
    }

    drawable->trs = toFloat4x4(input);

    // Only apply recursive parameters once
    if (!_dynamicsApplied) {
      applyRecursiveParams(context);
    }

    return Types::DrawableObjectVar.Get(_drawable);
  }
};

void registerGLTFShards() { REGISTER_SHARD("GFX.glTF", GLTFShard); }
} // namespace gfx