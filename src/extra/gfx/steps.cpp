#include "../gfx.hpp"
#include "shards_utils.hpp"
#include <gfx/feature.hpp>
#include <gfx/pipeline_step.hpp>
#include <gfx/steps/defaults.hpp>
#include <gfx/steps/effect.hpp>
#include <gfx/texture.hpp>
#include <magic_enum.hpp>
#include <params.hpp>
#include <optional>

using namespace shards;

namespace gfx {

struct DrawablePassShard {
  /* Input table
  {
    :Outputs [
      {:Name <string> :Format <format>}
      {:Name <string> :Format <format> :Clear true}
      {:Name <string> :Format <format> :Clear (Float4 0 0 0 0)}
      {:Name <string> :Format <format> :ClearDepth 1.0 :ClearStencil 0}
      {:Name <string> :Texture <texture>}
      ...
    ]
    :OutputScale (Float2 1.0 1.0)
    :Queue <queue>
    :Features [<feature> <feature> ...]
  }
*/

  static SHTypesInfo inputTypes() { return CoreInfo::AnyTableType; }
  static SHTypesInfo outputTypes() { return Types::PipelineStep; }

  PARAM_IMPL(DrawablePassShard);

  PipelineStepPtr *_step{};

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

  WGPUTextureFormat getAttachmentFormat(const std::string &name, const SHVar &formatVar) {
    if (formatVar.valueType == SHType::None) {
      if (name == "color") {
        return WGPUTextureFormat_BGRA8UnormSrgb;
      } else if (name == "depth") {
        return WGPUTextureFormat_Depth32Float;
      }
    } else {
      // TODO: Convert var to WGPUTextureFormat
      // possibly reuse code from Texture shard
    }

    throw std::runtime_error("Output :Format required");
  }

  void applyFeatures(SHContext *context, RenderDrawablesStep &step, const SHVar &input) {
    checkType(input.valueType, SHType::Seq, ":Features");
    for (size_t i = 0; i < input.payload.seqValue.len; i++) {
      auto &elem = input.payload.seqValue.elements[i];
      step.features.push_back(*varAsObjectChecked<FeaturePtr>(elem, Types::Feature));
    }
  }

  void applyOutputs(SHContext *context, RenderDrawablesStep &step, const SHVar &input) {
    checkType(input.valueType, SHType::Seq, "Outputs sequence");

    RenderStepOutput &output = (step.output = RenderStepOutput{}).value();
    for (size_t i = 0; i < input.payload.seqValue.len; i++) {
      auto &elem = input.payload.seqValue.elements[i];
      checkType(elem.valueType, SHType::Table, "Output");

      auto &table = elem.payload.tableValue;

      SHVar nameVar;
      if (!getFromTable(context, table, "Name", nameVar)) {
        throw std::runtime_error("Output :Name is required");
      }

      SHVar formatVar{};
      bool hasFormat = getFromTable(context, table, "Format", formatVar);

      SHVar textureVar;
      bool hasTexture = getFromTable(context, table, "Texture", textureVar);

      if (hasTexture && hasFormat)
        throw std::runtime_error(":Format and :Texture are exclusive");

      auto &attachment = output.attachments.emplace_back();

      std::string attachmentName = nameVar.payload.stringValue;
      WGPUTextureFormat textureFormat{};
      TexturePtr texture;
      if (hasTexture) {
        texture = *varAsObjectChecked<TexturePtr>(textureVar, Types::Texture);
        textureFormat = texture->getFormat().pixelFormat;
      } else {
        textureFormat = getAttachmentFormat(attachmentName, formatVar);
      }

      TextureFormatDesc textureFormatDesc = getTextureFormatDescription(textureFormat);

      std::optional<ClearValues> clearValues;

      SHVar clearValueVar;
      if (getFromTable(context, table, "Clear", clearValueVar)) {
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
        if (getFromTable(context, table, "ClearDepth", clearValueVar)) {
          depth = toVec<float>(clearValueVar);
          anySet = true;
        }
        if (getFromTable(context, table, "ClearStencil", clearValueVar)) {
          checkType(clearValueVar.valueType, SHType::Int, "ClearStencil");
          stencil = clearValueVar.payload.intValue;
          anySet = true;
        }
        if (anySet) {
          clearValues = ClearValues::getDepthStencilValue(depth, stencil);
        }
      }

      if (hasTexture) {
        const TexturePtr &texture = *varAsObjectChecked<TexturePtr>(textureVar, Types::Texture);
        auto textureFormatFlags = texture->getFormat().flags;
        if (!textureFormatFlagsContains(textureFormatFlags, TextureFormatFlags::RenderAttachment)) {
          throw std::runtime_error("Invalid output texture, it wasn't created as a render target");
        }

        attachment = RenderStepOutput::Texture{
            .name = nameVar.payload.stringValue,
            .handle = texture,
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

  void applyOutputScale(SHContext *context, RenderDrawablesStep &step, const SHVar &input) {
    if (!step.output) {
      step.output = steps::getDefaultRenderStepOutput();
    }
    step.output->sizeScale = toVec<float2>(input);
  }

  void applyQueue(SHContext *context, RenderDrawablesStep &step, const SHVar &input) {
    step.drawQueue = *varAsObjectChecked<DrawQueuePtr>(input, Types::DrawQueue);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    RenderDrawablesStep &step = std::get<RenderDrawablesStep>(*_step->get());

    checkType(input.valueType, SHType::Table, "Input table");
    const SHTable &inputTable = input.payload.tableValue;

    SHVar outputsVar;
    if (getFromTable(context, inputTable, "Outputs", outputsVar))
      applyOutputs(context, step, outputsVar);
    else {
      if (step.output)
        step.output.reset();
    }

    SHVar outputScaleVar;
    if (getFromTable(context, inputTable, "OutputScale", outputScaleVar)) {
      applyOutputScale(context, step, outputScaleVar);
    } else {
      if (step.output)
        step.output->sizeScale.reset();
    }

    SHVar queueVar;
    if (getFromTable(context, inputTable, "Queue", queueVar))
      applyQueue(context, step, queueVar);
    else {
      step.drawQueue = DrawQueuePtr();
    }

    SHVar featuresVar;
    if (getFromTable(context, inputTable, "Features", featuresVar))
      applyFeatures(context, step, featuresVar);
    else {
      step.features.clear();
    }

    return Types::PipelineStepObjectVar.Get(_step);
  }
};

void registerRenderStepShards() { REGISTER_SHARD("GFX.DrawablePass", DrawablePassShard); }
} // namespace gfx
