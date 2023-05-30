#include "gfx.hpp"
#include "drawable_utils.hpp"
#include "shader/composition.hpp"
#include "shards_utils.hpp"
#include "gfx/error_utils.hpp"
#include "gfx/fwd.hpp"
#include "gfx/hash.hpp"
#include "gfx/hasherxxh128.hpp"
#include "gfx/unique_id.hpp"
#include "shards_utils.hpp"
#include "shader/translator.hpp"
#include <shards/linalg_shim.hpp>
#include <shards/iterator.hpp>
#include <gfx/feature.hpp>
#include <gfx/pipeline_step.hpp>
#include <gfx/steps/defaults.hpp>
#include <gfx/steps/effect.hpp>
#include <gfx/features/transform.hpp>
#include <gfx/texture.hpp>
#include <gfx/gfx_wgpu.hpp>
#include <magic_enum.hpp>
#include <shards/core/params.hpp>
#include <optional>

using namespace shards;

namespace gfx {
namespace shared {

WGPUTextureFormat getAttachmentFormat(const std::string &name, const SHVar &formatVar) {
  if (formatVar.valueType == SHType::None) {
    if (name == "color") {
      return WGPUTextureFormat_BGRA8UnormSrgb;
    } else if (name == "depth") {
      return WGPUTextureFormat_Depth32Float;
    }
  } else {
    checkEnumType(formatVar, Types::TextureFormatEnumInfo::Type, "Attachment format");
    return WGPUTextureFormat(formatVar.payload.enumValue);
  }

  throw std::runtime_error("Output :Format required");
}

template <typename T> void applyOutputs(SHContext *context, T &step, const SHVar &input) {
  checkType(input.valueType, SHType::Seq, "Outputs sequence");

  RenderStepOutput &output = (step.output = RenderStepOutput{}).value();
  output.attachments.clear();
  for (size_t i = 0; i < input.payload.seqValue.len; i++) {
    auto &elem = input.payload.seqValue.elements[i];
    checkType(elem.valueType, SHType::Table, "Output");

    auto &table = elem.payload.tableValue;

    SHVar nameVar;
    if (!getFromTable(context, table, Var("Name"), nameVar)) {
      throw std::runtime_error("Output :Name is required");
    }

    SHVar formatVar{};
    bool hasFormat = getFromTable(context, table, Var("Format"), formatVar);

    SHVar textureVar;
    bool hasTexture = getFromTable(context, table, Var("Texture"), textureVar);

    if (hasTexture && hasFormat)
      throw std::runtime_error(":Format and :Texture are exclusive");

    auto &attachment = output.attachments.emplace_back();

    std::string attachmentName = nameVar.payload.stringValue;
    WGPUTextureFormat textureFormat{};
    TexturePtr texture;
    if (hasTexture) {
      texture = varAsObjectChecked<TexturePtr>(textureVar, Types::Texture);
      textureFormat = texture->getFormat().pixelFormat;
    } else {
      textureFormat = getAttachmentFormat(attachmentName, formatVar);
    }

    TextureFormatDesc textureFormatDesc = getTextureFormatDescription(textureFormat);

    std::optional<ClearValues> clearValues;

    SHVar clearValueVar;
    if (getFromTable(context, table, Var("Clear"), clearValueVar)) {
      if (clearValueVar.valueType == SHType::Bool) {
        // Clear with default values
        if (clearValueVar.payload.boolValue) {
          // Set clear value based on texture usage
          bool isDepthStencil =
              hasAnyTextureFormatUsage(textureFormatDesc.usage, TextureFormatUsage::Depth | TextureFormatUsage::Stencil);
          clearValues = isDepthStencil ? ClearValues::getDefaultDepthStencil() : ClearValues::getDefaultColor();
        }
      } else {
        checkType(clearValueVar.valueType, SHType::Float4, "Clear");
        clearValues = ClearValues::getColorValue(toVec<float4>(clearValueVar));
      }
    } else {
      float depth = 1.0f;
      uint32_t stencil = 0;
      bool anySet = false;
      if (getFromTable(context, table, Var("ClearDepth"), clearValueVar)) {
        depth = toVec<float>(clearValueVar);
        anySet = true;
      }
      if (getFromTable(context, table, Var("ClearStencil"), clearValueVar)) {
        checkType(clearValueVar.valueType, SHType::Int, "ClearStencil");
        stencil = clearValueVar.payload.intValue;
        anySet = true;
      }
      if (anySet) {
        clearValues = ClearValues::getDepthStencilValue(depth, stencil);
      }
    }

    if (hasTexture) {
      const TexturePtr &texture = varAsObjectChecked<TexturePtr>(textureVar, Types::Texture);
      auto textureFormatFlags = texture->getFormat().flags;
      if (!textureFormatFlagsContains(textureFormatFlags, TextureFormatFlags::RenderAttachment)) {
        throw std::runtime_error("Invalid output texture, it wasn't created as a render target");
      }

      attachment = RenderStepOutput::Texture{
          .name = nameVar.payload.stringValue,
          .subResource = texture,
          .clearValues = clearValues,
      };
    } else {
      attachment = RenderStepOutput::Named{
          .name = std::move(attachmentName),
          .format = textureFormat,
          .clearValues = clearValues,
      };
    }
  }
}

template <typename T> void applyOutputScale(SHContext *context, T &step, const SHVar &input) {
  if (!step.output) {
    step.output = steps::getDefaultRenderStepOutput();
  }
  step.output->sizeScale = toVec<float2>(input);
}

template <typename T> void applyAll(SHContext *context, T &step, const ParamVar& outputs, const ParamVar& outputScale, const ParamVar& features) {
  if (!outputs.isNone()) {
    // Type checking is done in applyOutputs
    shared::applyOutputs(context, step, outputs.get());
  } else {
    if (step.output) {
      step.output.reset();
    }
  }

  if (!outputScale.isNone()) {
    shared::applyOutputScale(context, step, outputScale.get());
  } else {
    if (step.output) {
      step.output->sizeScale.reset();
    }
  }

  step.features.clear();
  if (!features.isNone()) {
    applyFeatures(context, step.features, features.get());
  }
}

} // namespace shared

struct HashState {
  Hash128 hash;

  template <typename T> bool update(const T &step) {
    HasherXXH128<> hasher;
    for (const FeaturePtr &feature : step.features) {
      hasher(feature->getId());
    }

    auto newHash = hasher.getDigest();
    if (hash != newHash) {
      hash = newHash;
      return true;
    } else {
      return false;
    }
  }
};

struct DrawablePassShard {
  /* Parameters
    :Outputs [
      {:Name <string> :Format <format>}
      {:Name <string> :Format <format> :Clear true}
      {:Name <string> :Format <format> :Clear (SHType::Float4 0 0 0 0)}
      {:Name <string> :Format <format> :ClearDepth 1.0 :ClearStencil 0}
      {:Name <string> :Texture <texture>}
      ...
    ]
    :OutputScale (SHType::Float2 1.0 1.0)
    :Queue <queue>
    :Features [<feature> <feature> ...]
    :Sort <mode>
*/

  static inline Type DrawQueueVarType = Type::VariableOf(Types::DrawQueue);

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return Types::PipelineStep; }

  PARAM_PARAMVAR(_outputs, "Outputs", "", {CoreInfo::AnySeqType, CoreInfo::AnyVarSeqType});
  PARAM_PARAMVAR(_outputScale, "OutputScale", "", {CoreInfo::Float2Type, CoreInfo::Float2VarType});
  PARAM_PARAMVAR(_queue, "Queue", "", {DrawQueueVarType});
  PARAM_EXT(ParamVar, _features, Types::FeaturesParameterInfo);
  PARAM_VAR(_sort, "Sort", "", {CoreInfo::AnyEnumType});

  PARAM_IMPL(PARAM_IMPL_FOR(_outputs), PARAM_IMPL_FOR(_outputScale),
             PARAM_IMPL_FOR(_queue), PARAM_IMPL_FOR(_features), PARAM_IMPL_FOR(_sort));

  PipelineStepPtr *_step{};
  HashState _hashState;

  void cleanup() {
    if (_step) {
      Types::PipelineStepObjectVar.Release(_step);
      _step = nullptr;
    }
    PARAM_CLEANUP();
  }

  void warmup(SHContext *context) {
    _step = Types::PipelineStepObjectVar.New();
    *_step = makePipelineStep(RenderDrawablesStep());
    PARAM_WARMUP(context);
  }

  void applyQueue(SHContext *context, RenderDrawablesStep &step) {
    step.drawQueue = varAsObjectChecked<DrawQueuePtr>(_queue.get(), Types::DrawQueue);
  }

  void applySorting(SHContext *context, RenderDrawablesStep &step) {
    checkEnumType(_sort, Types::SortModeEnumInfo::Type, "DrawablePass Sort");
    step.sortMode = (gfx::SortMode)_sort.payload.enumValue;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    RenderDrawablesStep &step = std::get<RenderDrawablesStep>(*_step->get());

    shared::applyAll(context, step, _outputs, _outputScale, _features);

    if (!(_sort.valueType == SHType::None)) {
      applySorting(context, step);
    } else {
      step.sortMode = SortMode::Batch;
    }
    if (!_queue.isNone()) {
      applyQueue(context, step);
    } else {
      throw formatException("DrawablePass requires a queue");
    }

    if (_hashState.update(step)) {
      // *_step = cloneSelfWithId(_step->get(), );
      step.id = renderStepIdGenerator.getNext();
    }

    return Types::PipelineStepObjectVar.Get(_step);
  }
};

struct EffectPassShard {
  /* Input table
  {
    :Outputs <same as drawable pass>
    :OutputScale (SHType::Float2 1.0 1.0)
    :Inputs [<name1> <name2> ...]
    :EntryPoint <shader code>
    :Params {:name <value> ...}
    :Features [<feature> <feature> ...]
  }
*/

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return Types::PipelineStep; }

  // Outputs should be a seq of tables, but no CoreInfo of such type. Type checking done in applyOutputs
  PARAM_PARAMVAR(_outputs, "Outputs", "", {CoreInfo::AnySeqType, CoreInfo::AnyVarSeqType});
  PARAM_PARAMVAR(_outputScale, "OutputScale", "", {CoreInfo::Float2Type, CoreInfo::Float2VarType});
  PARAM_PARAMVAR(_inputs, "Inputs", "", {CoreInfo::StringSeqType, CoreInfo::StringVarSeqType});
  PARAM_PARAMVAR(_entryPoint, "EntryPoint", "", {CoreInfo::ShardRefSeqType, CoreInfo::ShardRefVarSeqType});
  PARAM_PARAMVAR(_params, "Params", "", {CoreInfo::AnyTableType, CoreInfo::AnyVarTableType});
  PARAM_PARAMVAR(_features, "Features", "", {CoreInfo::AnySeqType, CoreInfo::AnyVarSeqType});

  PARAM_IMPL(PARAM_IMPL_FOR(_outputs), PARAM_IMPL_FOR(_outputScale), PARAM_IMPL_FOR(_inputs),
             PARAM_IMPL_FOR(_entryPoint), PARAM_IMPL_FOR(_params), PARAM_IMPL_FOR(_features));

  FeaturePtr wrapperFeature;
  PipelineStepPtr *_step{};
  gfx::shader::VariableMap _composedWith;
  std::vector<FeaturePtr> _generatedFeatures;

  void cleanup() {
    if (_step) {
      Types::PipelineStepObjectVar.Release(_step);
      _step = nullptr;
    }
    PARAM_CLEANUP();
  }

  void warmup(SHContext *context) {
    _step = Types::PipelineStepObjectVar.New();
    *_step = makePipelineStep(RenderFullscreenStep());

    wrapperFeature = std::make_shared<Feature>();

    PARAM_WARMUP(context);
  }

  void applyComposeWith(SHContext *context, const SHVar &input) {
    checkType(input.valueType, SHType::Table, ":ComposeWith table");

    _composedWith.clear();
    for (auto &[k, v] : input.payload.tableValue) {
      if (k.valueType != SHType::String)
        throw formatException("ComposeWith key must be a string");
      std::string keyStr(k.payload.stringValue, k.payload.stringLen);
      ParamVar pv(v);
      pv.warmup(context);
      auto &var = _composedWith.emplace(std::move(keyStr), pv.get()).first->second;
      if (var.valueType == SHType::None) {
        throw formatException("Required variable {} not found", k);
      }
    }
  }

  void applyInputs(SHContext *context, RenderFullscreenStep &step, const SHVar &input) {
    checkType(input.valueType, SHType::Seq, ":Inputs");

    step.inputs.clear();
    for (size_t i = 0; i < input.payload.seqValue.len; i++) {
      auto &elem = input.payload.seqValue.elements[i];
      checkType(elem.valueType, SHType::String, "Input");

      step.inputs.emplace_back(elem.payload.stringValue);
    }
  }

  void applyParams(SHContext *context, RenderFullscreenStep &step, const SHVar &input) {
    checkType(input.valueType, SHType::Table, ":Params");
    const SHTable &inputTable = input.payload.tableValue;

    initShaderParams(context, inputTable, step.parameters);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    RenderFullscreenStep &step = std::get<RenderFullscreenStep>(*_step->get());

    // Basically applyAll but not with table input. Can extract out into a function
    shared::applyAll(context, step, _outputs, _outputScale, _features);
    // End of applyAll

    if (!_inputs.isNone()) {
      applyInputs(context, step, _inputs.get());
    } else {
      step.inputs = RenderStepInputs{"color"};
    }

    if (!_params.isNone()) {
      applyParams(context, step, _params.get());
    }

    // NOTE: First check these variables to see if we need to invalidate the feature Id (to break caching)
    // TODO: Check if there should be a ComposeWith parameter in the first place
    // SHVar composeWithVar;
    // if(!_composeWith)
    // if (getFromTable(context, inputTable, "ComposeWith", composeWithVar)) {
    //   // Always create a new object to force shader recompile
    //   wrapperFeature = std::make_shared<Feature>();
    //   applyComposeWith(context, composeWithVar);
    // }

    // Only do this once
    if (wrapperFeature->shaderEntryPoints.empty()) {
      _generatedFeatures.clear();

      if (!_entryPoint.isNone()) {
        // Add default base transform
        _generatedFeatures.push_back(features::Transform::create(false, false));

        shader::EntryPoint entryPoint;
        entryPoint.stage = ProgrammableGraphicsStage::Fragment;
        shader::applyShaderEntryPoint(context, entryPoint, _entryPoint.get(), _composedWith);
        wrapperFeature->shaderEntryPoints.emplace_back(std::move(entryPoint));
        _generatedFeatures.push_back(wrapperFeature);
      }
    }

    for (auto &feature : _generatedFeatures) {
      step.features.push_back(feature);
    }

    return Types::PipelineStepObjectVar.Get(_step);
  }
};

void registerRenderStepShards() {
  REGISTER_SHARD("GFX.DrawablePass", DrawablePassShard);
  REGISTER_SHARD("GFX.EffectPass", EffectPassShard);
}
} // namespace gfx
