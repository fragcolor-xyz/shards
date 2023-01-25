#include "../gfx.hpp"
#include "buffer_vars.hpp"
#include "common_types.hpp"
#include "drawable_utils.hpp"
#include "extra/gfx.hpp"
#include "extra/gfx/shards_types.hpp"
#include "foundation.hpp"
#include "gfx/error_utils.hpp"
#include "gfx/params.hpp"
#include "gfx/shader/types.hpp"
#include "gfx/texture.hpp"
#include "runtime.hpp"
#include "shader/translator.hpp"
#include "shards.h"
#include "shards.hpp"
#include "shards_utils.hpp"
#include <array>
#include <deque>
#include <gfx/context.hpp>
#include <gfx/features/base_color.hpp>
#include <gfx/features/debug_color.hpp>
#include <gfx/features/transform.hpp>
#include <gfx/features/wireframe.hpp>
#include <gfx/features/velocity.hpp>
#include <linalg_shim.hpp>
#include <magic_enum.hpp>
#include <memory>
#include <queue>
#include <shards/shared.hpp>
#include <stdexcept>
#include <variant>
#include <webgpu-headers/webgpu.h>

using namespace shards;

namespace gfx {
enum class BuiltinFeatureId { Transform, BaseColor, VertexColorFromNormal, Wireframe, Velocity };
}

ENUM_HELP(gfx::BuiltinFeatureId, gfx::BuiltinFeatureId::Transform, SHCCSTR("Add basic world/view/projection transform"));
ENUM_HELP(gfx::BuiltinFeatureId, gfx::BuiltinFeatureId::BaseColor,
          SHCCSTR("Add basic color from vertex color and (optional) color texture"));
ENUM_HELP(gfx::BuiltinFeatureId, gfx::BuiltinFeatureId::VertexColorFromNormal, SHCCSTR("Outputs color from vertex color"));
ENUM_HELP(gfx::BuiltinFeatureId, gfx::BuiltinFeatureId::Wireframe, SHCCSTR("Modifies the main color to visualize vertex edges"));
ENUM_HELP(gfx::BuiltinFeatureId, gfx::BuiltinFeatureId::Velocity,
          SHCCSTR("Outputs object velocity into the velocity global & output"));

namespace gfx {
using shards::Mat4;

struct BuiltinFeatureShard {
  DECL_ENUM_INFO(BuiltinFeatureId, BuiltinFeatureId, 'feid');

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return Types::Feature; }

  static inline Parameters params{{"Id", SHCCSTR("Builtin feature id."), {BuiltinFeatureIdEnumInfo::Type}}};
  static SHParametersInfo parameters() { return params; }

  BuiltinFeatureId _id{};
  FeaturePtr *_feature{};

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _id = BuiltinFeatureId(value.payload.enumValue);
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var::Enum(_id, VendorId, BuiltinFeatureIdEnumInfo::TypeId);
    default:
      return Var::Empty;
    }
  }

  void cleanup() {
    if (_feature) {
      Types::FeatureObjectVar.Release(_feature);
      _feature = nullptr;
    }
  }

  void warmup(SHContext *context) {
    _feature = Types::FeatureObjectVar.New();
    switch (_id) {
    case BuiltinFeatureId::Transform:
      *_feature = features::Transform::create();
      break;
    case BuiltinFeatureId::BaseColor:
      *_feature = features::BaseColor::create();
      break;
    case BuiltinFeatureId::VertexColorFromNormal:
      *_feature = features::DebugColor::create("normal", ProgrammableGraphicsStage::Vertex);
      break;
    case BuiltinFeatureId::Wireframe:
      *_feature = features::Wireframe::create();
      break;
    case BuiltinFeatureId::Velocity:
      *_feature = features::Velocity::create();
      break;
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) { return Types::FeatureObjectVar.Get(_feature); }
};

struct FeatureShard {
  /* Input table
    {
      :Shaders [
        {:Name <string> :Stage <enum> :Before [...] :After [...] :EntryPoint (-> ...)}
      ]
      :State {
        :Blend {...}
      }
      :DrawData [(-> ...)]/(-> ...)
      :Params [
        {:Name <string> :Type <type> :Dimension <number>}
      ]
    }
  */

  static inline shards::Types GeneratedInputTableTypes{{Types::DrawQueue, Types::View}};
  static inline std::array<SHString, 2> GeneratedInputTableKeys{"Queue", "View"};
  static inline Type GeneratedInputTableType = Type::TableOf(GeneratedInputTableTypes, GeneratedInputTableKeys);

  static SHTypesInfo inputTypes() { return CoreInfo::AnyTableType; }
  static SHTypesInfo outputTypes() { return Types::Feature; }

  static SHParametersInfo parameters() {

    static Parameters p{
        {"Generated",
         SHCCSTR("The shards that are run to generated the draw data, can use nested rendering commmands."),
         {CoreInfo::NoneType, CoreInfo::WireType, Type::SeqOf(CoreInfo::WireType)}},
    };
    return p;
  }

  IterableExposedInfo _sharedCopy;

  std::vector<std::shared_ptr<SHWire>> _generated;
  OwnedVar _generatedRaw;

  FeaturePtr *_featurePtr{};
  SHVar _variable{};

  std::shared_ptr<SHMesh> _generatorsMesh;

  void setGenerated(SHVar &var) {
    _generated.clear();
    if (var.valueType != SHType::None) {
      if (var.valueType == SHType::Wire) {
        // A single shards sequence
        _generated.emplace_back(*(std::shared_ptr<SHWire> *)var.payload.wireValue);
      } else {
        auto &seq = var.payload.seqValue;
        if (seq.len == 0)
          return;
        // Multiple separate shards sequences
        for (uint32_t i = 0; i < seq.len; i++) {
          _generated.emplace_back(*(std::shared_ptr<SHWire> *)seq.elements[i].payload.wireValue);
        }
      }
    }
  }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _generatedRaw = value;
      setGenerated(_generatedRaw);
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _generatedRaw;
    default:
      return Var::Empty;
    }
  }

  void cleanup() {
    if (_generatorsMesh) {
      _generatorsMesh->terminate();
      _generatorsMesh.reset();
    }

    if (_featurePtr) {
      clear();
      Types::FeatureObjectVar.Release(_featurePtr);
      _featurePtr = nullptr;
    }
  }

  void warmup(SHContext *context) {
    _featurePtr = Types::FeatureObjectVar.New();
    *_featurePtr = std::make_shared<Feature>();

    if (!_generated.empty())
      _generatorsMesh = SHMesh::make();
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    // Capture variables for callbacks
    // TODO: Refactor IterableArray
    const IterableExposedInfo _hack(data.shared);
    _sharedCopy = _hack;

    SHInstanceData generatorInstanceData{};
    generatorInstanceData.inputType = GeneratedInputTableType;

    ExposedInfo exposed;
    exposed.push_back(RequiredGraphicsRendererContext::getExposedTypeInfo());
    generatorInstanceData.shared = SHExposedTypesInfo(exposed);

    // Compose generator wires
    for (auto &generated : _generated) {
      generatorInstanceData.wire = generated.get();
      generated->composeResult = composeWire(
          generated->shards,
          [](const struct Shard *errorShard, SHString errorTxt, SHBool nonfatalWarning, void *userData) {
            if (!nonfatalWarning) {
              auto msg = "Feature: failed inner wire validation, error: " + std::string(errorTxt);
              throw shards::SHException(msg);
            } else {
              SHLOG_INFO("Feature: warning during inner wire validation: {}", errorTxt);
            }
          },
          nullptr, generatorInstanceData);

      if (generated->composeResult->outputType.basicType != SHType::Table) {
        throw formatException("Feature generated wire should return a parameter table");
      }
    }

    return Types::Feature;
  }

  void applyBlendComponent(SHContext *context, BlendComponent &blendComponent, const SHVar &input) {
    checkType(input.valueType, SHType::Table, ":Blend Alpha/Color table");
    const SHTable &inputTable = input.payload.tableValue;

    SHVar operationVar;
    if (getFromTable(context, inputTable, "Operation", operationVar)) {
      blendComponent.operation = WGPUBlendOperation(operationVar.payload.enumValue);
    } else {
      throw formatException(":Blend table require an :Operation");
    }

    auto applyFactor = [](SHContext *context, WGPUBlendFactor &factor, const char *key, const SHTable &inputTable) {
      SHVar factorVar;
      if (getFromTable(context, inputTable, key, factorVar)) {
        factor = WGPUBlendFactor(factorVar.payload.enumValue);
      } else {
        throw formatException(":Blend table require a :{} factor", key);
      }
    };
    applyFactor(context, blendComponent.srcFactor, "Src", inputTable);
    applyFactor(context, blendComponent.dstFactor, "Dst", inputTable);
  }

  void applyBlendState(SHContext *context, BlendState &blendState, const SHVar &input) {
    checkType(input.valueType, SHType::Table, ":State :Blend table");
    const SHTable &inputTable = input.payload.tableValue;

    SHVar colorVar;
    if (getFromTable(context, inputTable, "Color", colorVar)) {
      applyBlendComponent(context, blendState.color, colorVar);
    }
    SHVar alphaVar;
    if (getFromTable(context, inputTable, "Alpha", alphaVar)) {
      applyBlendComponent(context, blendState.alpha, alphaVar);
    }
  }

  void applyState(SHContext *context, FeaturePipelineState &state, const SHVar &input) {
    checkType(input.valueType, SHType::Table, ":State table");
    const SHTable &inputTable = input.payload.tableValue;

    SHVar depthCompareVar;
    if (getFromTable(context, inputTable, "DepthCompare", depthCompareVar)) {
      checkEnumType(depthCompareVar, Types::CompareFunctionEnumInfo::Type, ":Shaders DepthCompare");
      state.set_depthCompare(WGPUCompareFunction(depthCompareVar.payload.enumValue));
    }

    SHVar depthWriteVar;
    if (getFromTable(context, inputTable, "DepthWrite", depthWriteVar)) {
      state.set_depthWrite(depthWriteVar.payload.boolValue);
    }

    SHVar colorWriteVar;
    if (getFromTable(context, inputTable, "ColorWrite", colorWriteVar)) {
      WGPUColorWriteMask mask{};
      auto apply = [&mask](SHVar &var) {
        checkEnumType(var, Types::ColorMaskEnumInfo::Type, ":ColorWrite");
        (uint8_t &)mask |= WGPUColorWriteMask(var.payload.enumValue);
      };

      // Combine single enum or seq[enum] bitflags into mask
      if (colorWriteVar.valueType == SHType::Enum) {
        apply(colorWriteVar);
      } else if (colorWriteVar.valueType == SHType::Seq) {
        auto &seq = colorWriteVar.payload.seqValue;
        for (size_t i = 0; i < seq.len; i++) {
          apply(seq.elements[i]);
        }
      } else {
        throw formatException(":ColorWrite requires an enum (sequence)");
      }

      state.set_colorWrite(mask);
    }

    SHVar blendVar;
    if (getFromTable(context, inputTable, "Blend", blendVar)) {
      BlendState blendState{
          .color = BlendComponent::Opaque,
          .alpha = BlendComponent::Opaque,
      };
      applyBlendState(context, blendState, blendVar);
      state.set_blend(blendState);
    }

    SHVar flipFrontFaceVar;
    if (getFromTable(context, inputTable, "FlipFrontFace", flipFrontFaceVar)) {
      state.set_flipFrontFace(flipFrontFaceVar.payload.boolValue);
    }

    SHVar cullingVar;
    if (getFromTable(context, inputTable, "Culling", cullingVar)) {
      state.set_culling(cullingVar.payload.boolValue);
    }
  }

  void applyShaderDependency(SHContext *context, shader::EntryPoint &entryPoint, shader::DependencyType type,
                             const SHVar &input) {
    checkType(input.valueType, SHType::String, "Shader dependency");
    const SHString &inputString = input.payload.stringValue;

    shader::NamedDependency &dep = entryPoint.dependencies.emplace_back();
    dep.name = inputString;
    dep.type = type;
  }

  void applyShader(SHContext *context, Feature &feature, const SHVar &input) {
    shader::EntryPoint &entryPoint = feature.shaderEntryPoints.emplace_back();

    checkType(input.valueType, SHType::Table, ":Shaders table");
    const SHTable &inputTable = input.payload.tableValue;

    SHVar stageVar;
    if (getFromTable(context, inputTable, "Stage", stageVar)) {
      checkEnumType(stageVar, Types::ProgrammableGraphicsStageEnumInfo::Type, ":Shaders Stage");
      entryPoint.stage = ProgrammableGraphicsStage(stageVar.payload.enumValue);
    } else
      entryPoint.stage = ProgrammableGraphicsStage::Fragment;

    SHVar depsVar;
    if (getFromTable(context, inputTable, "Before", depsVar)) {
      checkType(depsVar.valueType, SHType::Seq, ":Shaders Dependencies (Before)");
      const SHSeq &seq = depsVar.payload.seqValue;
      for (size_t i = 0; i < seq.len; i++) {
        applyShaderDependency(context, entryPoint, shader::DependencyType::Before, seq.elements[i]);
      }
    }
    if (getFromTable(context, inputTable, "After", depsVar)) {
      checkType(depsVar.valueType, SHType::Seq, ":Shaders Dependencies (After)");
      const SHSeq &seq = depsVar.payload.seqValue;
      for (size_t i = 0; i < seq.len; i++) {
        applyShaderDependency(context, entryPoint, shader::DependencyType::After, seq.elements[i]);
      }
    }

    SHVar nameVar;
    if (getFromTable(context, inputTable, "Name", nameVar)) {
      checkType(nameVar.valueType, SHType::String, ":Shaders Name");
      entryPoint.name = nameVar.payload.stringValue;
    }

    SHVar entryPointVar;
    if (getFromTable(context, inputTable, "EntryPoint", entryPointVar)) {
      applyShaderEntryPoint(context, entryPoint, entryPointVar);
    } else {
      throw formatException(":Shader table requires an :EntryPoint");
    }
  }

  void applyShaders(SHContext *context, Feature &feature, const SHVar &input) {
    checkType(input.valueType, SHType::Seq, ":Shaders param");

    const SHSeq &seq = input.payload.seqValue;
    for (size_t i = 0; i < seq.len; i++) {
      applyShader(context, feature, seq.elements[i]);
    }
  }

  static ParamVariant paramToVariant(SHContext *context, SHVar v) {
    SHVar *ref{};
    if (v.valueType == SHType::ContextVar) {
      ref = referenceVariable(context, v.payload.stringValue);
      v = *ref;
    }

    ParamVariant variant;
    varToParam(v, variant);

    if (ref)
      releaseVariable(ref);

    return variant;
  }

  static void applyDrawData(SHContext *shContext, const FeatureCallbackContext &callbackContext, IParameterCollector &collector,
                            const SHVar &input) {
    checkType(input.valueType, SHType::Table, ":DrawData wire output");
    const SHTable &inputTable = input.payload.tableValue;

    ForEach(inputTable, [&](const char *k, SHVar v) {
      ParamVariant variant = paramToVariant(shContext, v);
      collector.setParam(k, variant);
    });
  }

  void applyDrawData(SHContext *context, Feature &feature, const SHVar &input) {
    auto isWire = [](const SHVar &input) {
      auto &seq = input.payload.seqValue;
      if (input.valueType != SHType::Seq) {
        return false;
      }

      for (size_t i = 0; i < seq.len; i++) {
        if (seq.elements[i].valueType != SHType::ShardRef) {
          return false;
        }
      }
      return true;
    };

    auto apply = [&](const SHVar &input) {
      if (!isWire(input))
        throw formatException(":DrawData expects a wire or sequence of wires");

      // TODO: Resolve how to capture these
      struct Captured {
        ShardsVar wire; // Is this safe to capture?
        ~Captured() { wire.cleanup(); }
      };
      auto captured = std::make_shared<Captured>();
      captured->wire = input;

      SHInstanceData instanceData{};
      instanceData.inputType = shards::CoreInfo::AnyTableType;
      instanceData.shared = _sharedCopy;

      SHComposeResult composeResult = captured->wire.compose(instanceData);
      assert(!composeResult.failed);

      captured->wire.warmup(context);

      feature.drawableParameterGenerators.emplace_back(
          [shContext = context, captured = captured](const FeatureCallbackContext &ctx, IParameterCollector &collector) {
            SHVar input{};
            SHVar output{};
            SHWireState result = captured->wire.activate(shContext, input, output);
            assert(result == SHWireState::Continue);

            applyDrawData(shContext, ctx, collector, output);
          });
    };

    if (isWire(input)) {
      apply(input);
    } else if (input.valueType == SHType::Seq) {
      const SHSeq &seq = input.payload.seqValue;
      for (size_t i = 0; i < seq.len; i++) {
        apply(seq.elements[i]);
      }
    } else {
      throw formatException(":DrawData expects a wire or sequence of wires");
    }
  }

  // Returns the field type, or std::monostate if not specified
  using ShaderFieldTypeVariant = std::variant<std::monostate, shader::FieldType, TextureType>;
  ShaderFieldTypeVariant getShaderFieldType(SHContext *context, const SHTable &inputTable) {
    shader::FieldType fieldType;
    bool isFieldTypeSet = false;

    SHVar typeVar;
    if (getFromTable(context, inputTable, "Type", typeVar)) {
      auto enumType = Type::Enum(typeVar.payload.enumVendorId, typeVar.payload.enumTypeId);
      if (enumType == Types::ShaderFieldBaseTypeEnumInfo::Type) {
        checkEnumType(typeVar, Types::ShaderFieldBaseTypeEnumInfo::Type, ":Type");
        fieldType.baseType = ShaderFieldBaseType(typeVar.payload.enumValue);
        isFieldTypeSet = true;
      } else if (enumType == Types::TextureDimensionEnumInfo::Type) {
        return TextureType(typeVar.payload.enumValue);
      } else {
        throw formatException("Invalid Type for shader Param, should be either TextureDimension... or ShaderFieldBaseType...");
      }
    } else {
      // Default type if not specified:
      fieldType.baseType = ShaderFieldBaseType::Float32;
    }

    SHVar dimVar;
    if (getFromTable(context, inputTable, "Dimension", dimVar)) {
      checkType(dimVar.valueType, SHType::Int, ":Dimension");
      fieldType.numComponents = size_t(typeVar.payload.intValue);
      isFieldTypeSet = true;
    } else {
      // Default size if not specified:
      fieldType.numComponents = 1;
    }

    if (isFieldTypeSet)
      return fieldType;

    return std::monostate();
  }

  void applyParam(SHContext *context, Feature &feature, const SHVar &input) {
    checkType(input.valueType, SHType::Table, ":Params Entry");
    const SHTable &inputTable = input.payload.tableValue;

    SHVar nameVar;
    SHString name{};
    if (getFromTable(context, inputTable, "Name", nameVar)) {
      checkType(nameVar.valueType, SHType::String, ":Params Name");
      name = nameVar.payload.stringValue;
    } else {
      throw formatException(":Params Entry requires a :Name");
    }

    SHVar defaultVar;
    ParamVariant defaultValue;
    if (getFromTable(context, inputTable, "Default", defaultVar)) {
      defaultValue = paramToVariant(context, defaultVar);
    }

    ShaderFieldTypeVariant typeVariant = getShaderFieldType(context, inputTable);
    std::visit(
        [&](auto &&arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, shader::FieldType>) {
            feature.shaderParams.emplace_back(name, arg);
          } else if constexpr (std::is_same_v<T, TextureType>) {
            feature.textureParams.emplace_back(name);
          } else {
            if (defaultValue.index() > 0) {
              // Derive field type from given default value
              auto fieldType = getParamVariantType(defaultValue);
              feature.shaderParams.emplace_back(name, fieldType, defaultValue);
            } else {
              throw formatException("Shader parameter \"{}\" should have a type or default value", name);
            }
          }
        },
        typeVariant);
  }

  void applyParams(SHContext *context, Feature &feature, const SHVar &input) {
    checkType(input.valueType, SHType::Seq, ":Params");
    const SHSeq &inputSeq = input.payload.seqValue;

    ForEach(inputSeq, [&](SHVar v) { applyParam(context, feature, v); });
  }

  void clear() {
    Feature &feature = *_featurePtr->get();
    feature.drawableParameterGenerators.clear();
    feature.shaderEntryPoints.clear();
    feature.shaderParams.clear();
    feature.viewParameterGenerators.clear();
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    Feature &feature = *_featurePtr->get();

    // Reset feature first
    clear();

    checkType(input.valueType, SHType::Table, "Input table");
    const SHTable &inputTable = input.payload.tableValue;

    SHVar shadersVar;
    if (getFromTable(context, inputTable, "Shaders", shadersVar))
      applyShaders(context, feature, shadersVar);

    SHVar stateVar;
    if (getFromTable(context, inputTable, "State", stateVar))
      applyState(context, feature.state, stateVar);

    SHVar drawDataVar;
    if (getFromTable(context, inputTable, "DrawData", drawDataVar))
      applyDrawData(context, feature, drawDataVar);

    SHVar paramsVar;
    if (getFromTable(context, inputTable, "Params", paramsVar))
      applyParams(context, feature, paramsVar);

    feature.generators.clear();
    feature.generators.emplace_back([=](IFeatureGenerateContext &ctx) { runGenerators(ctx); });

    return Types::FeatureObjectVar.Get(_featurePtr);
  }

  void runGenerators(IFeatureGenerateContext &ctx) {
    if (!_generatorsMesh)
      return;

    GraphicsRendererContext graphicsRendererContext{
        .renderer = &ctx.getRenderer(),
        .render = [&ctx = ctx](std::vector<ViewPtr> views,
                               const PipelineSteps &pipelineSteps) { ctx.render(views, pipelineSteps); },
    };
    _generatorsMesh->variables.emplace(GraphicsRendererContext::VariableName,
                                       Var::Object(&graphicsRendererContext, GraphicsRendererContext::Type));

    // Setup input table
    SHDrawQueue queue{ctx.getQueue()};
    SHView view{.view = ctx.getView()};
    TableVar input;
    input.get<Var>("Queue") = Var::Object(&queue, Types::DrawQueue);
    input.get<Var>("View") = Var::Object(&view, Types::View);

    // Schedule generator wires or update inputs
    for (auto &gen : _generated) {
      if (!_generatorsMesh->scheduled.contains(gen)) {
        _generatorsMesh->schedule(gen, input, false);
      } else {
        // Update inputs
        gen->currentInput = input;
      }
    }

    // Run one tick of the generator wires
    if (!_generatorsMesh->tick())
      throw formatException("Generator tick failed");

    // Fetch results and insert into parameter collector
    for (auto &gen : _generated) {
      if (gen->previousOutput.valueType != SHType::None) {
        SHTable outputTable = gen->previousOutput.payload.tableValue;
        auto &collector = ctx.getParameterCollector();
        ForEach(outputTable, [&](const SHString &k, const SHVar &v) {
          if (v.valueType == SHType::Object) {
            auto texture = *varAsObjectChecked<TexturePtr>(v, Types::Texture);
            collector.setTexture(k, texture);
          } else {
            ParamVariant variant = paramToVariant(gen->context, v);
            collector.setParam(k, variant);
          }
        });
      }
    }
  }
};

void registerFeatureShards() {
  REGISTER_ENUM(BuiltinFeatureShard::BuiltinFeatureIdEnumInfo);

  REGISTER_SHARD("GFX.BuiltinFeature", BuiltinFeatureShard);
  REGISTER_SHARD("GFX.Feature", FeatureShard);
}
} // namespace gfx
