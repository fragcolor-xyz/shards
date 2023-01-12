#include "../gfx.hpp"
#include "buffer_vars.hpp"
#include "drawable_utils.hpp"
#include "shader/translator.hpp"
#include "shards_utils.hpp"
#include <gfx/context.hpp>
#include <gfx/features/base_color.hpp>
#include <gfx/features/debug_color.hpp>
#include <gfx/features/transform.hpp>
#include <gfx/features/wireframe.hpp>
#include <gfx/features/velocity.hpp>
#include <linalg_shim.hpp>
#include <magic_enum.hpp>
#include <shards/shared.hpp>

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
  static SHTypesInfo inputTypes() { return CoreInfo::AnyTableType; }
  static SHTypesInfo outputTypes() { return Types::Feature; }

  static SHParametersInfo parameters() { return Parameters{}; }

  IterableExposedInfo _sharedCopy;

  void setParam(int index, const SHVar &value) {
    switch (index) {
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    default:
      return Var::Empty;
    }
  }

  void cleanup() {}
  void warmup(SHContext *context) {}
  SHTypeInfo compose(const SHInstanceData &data) {
    // Capture variables for callbacks
    // TODO: Refactor IterableArray
    const IterableExposedInfo _hack(data.shared);
    _sharedCopy = _hack;

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
    if (getFromTable(context, inputTable, "flipFrontFace", flipFrontFaceVar)) {
      state.set_flipFrontFace(flipFrontFaceVar.payload.boolValue);
    }

    SHVar cullingVar;
    if (getFromTable(context, inputTable, "culling", cullingVar)) {
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

  static void applyDrawData(const FeatureCallbackContext &ctx, IDrawDataCollector &collector, const SHVar &input) {
    checkType(input.valueType, SHType::Table, ":DrawData wire output");
    const SHTable &inputTable = input.payload.tableValue;

    ContextUserData *userData = ctx.context.userData.get<ContextUserData>();
    assert(userData && userData->shardsContext);

    ForEach(inputTable, [&](const char *k, SHVar v) {
      ParamVariant variant = paramToVariant(userData->shardsContext, v);
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

      feature.drawData.emplace_back([captured = captured](const FeatureCallbackContext &ctx, IDrawDataCollector &collector) {
        ContextUserData *contextUserData = ctx.context.userData.get<ContextUserData>();
        SHContext *SHContext = contextUserData->shardsContext;

        SHVar input{};
        SHVar output{};
        SHWireState result = captured->wire.activate(SHContext, input, output);
        assert(result == SHWireState::Continue);

        applyDrawData(ctx, collector, output);
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

  void applyShaderFieldType(SHContext *context, shader::FieldType &fieldType, const SHTable &inputTable) {
    SHVar typeVar;
    if (getFromTable(context, inputTable, "Type", typeVar)) {
      checkEnumType(typeVar, Types::ShaderFieldBaseTypeEnumInfo::Type, ":Type");
      fieldType.baseType = ShaderFieldBaseType(typeVar.payload.enumValue);
    } else {
      // Default type if not specified:
      fieldType.baseType = ShaderFieldBaseType::Float32;
    }

    SHVar dimVar;
    if (getFromTable(context, inputTable, "Dimension", dimVar)) {
      checkType(dimVar.valueType, SHType::Int, ":Dimension");
      fieldType.numComponents = size_t(typeVar.payload.intValue);
    } else {
      // Default size if not specified:
      fieldType.numComponents = 1;
    }
  }

  void applyParam(SHContext *context, Feature &feature, const SHVar &input) {
    NamedShaderParam &param = feature.shaderParams.emplace_back();

    checkType(input.valueType, SHType::Table, ":Params Entry");
    const SHTable &inputTable = input.payload.tableValue;

    SHVar nameVar;
    if (getFromTable(context, inputTable, "Name", nameVar)) {
      checkType(nameVar.valueType, SHType::String, ":Params Name");
      param.name = nameVar.payload.stringValue;
    } else {
      throw formatException(":Params Entry requires a :Name");
    }

    applyShaderFieldType(context, param.type, inputTable);

    SHVar defaultVar;
    if (getFromTable(context, inputTable, "Default", defaultVar)) {
      param.defaultValue = paramToVariant(context, defaultVar);

      // Derive type from default value
      param.type = getParamVariantType(param.defaultValue);
    }

    // TODO: Also handle texture params here
    // TODO: Parse ShaderParamFlags here too once we have functional ones
  }

  void applyParams(SHContext *context, Feature &feature, const SHVar &input) {
    checkType(input.valueType, SHType::Seq, ":Params");
    const SHSeq &inputSeq = input.payload.seqValue;

    ForEach(inputSeq, [&](SHVar v) { applyParam(context, feature, v); });
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    FeaturePtr *varPtr = Types::FeatureObjectVar.New();
    *varPtr = std::make_shared<Feature>();

    Feature &feature = *varPtr->get();

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

    return Types::FeatureObjectVar.Get(varPtr);
  }
};

void registerFeatureShards() {
  REGISTER_ENUM(BuiltinFeatureShard::BuiltinFeatureIdEnumInfo);

  REGISTER_SHARD("GFX.BuiltinFeature", BuiltinFeatureShard);
  REGISTER_SHARD("GFX.Feature", FeatureShard);
}
} // namespace gfx
