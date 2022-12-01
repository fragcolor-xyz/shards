#ifndef SH_EXTRA_GFX_SHADER_CORE_BLOCKS
#define SH_EXTRA_GFX_SHADER_CORE_BLOCKS

// Required before shard headers
#include "../shards_types.hpp"
#include "magic_enum.hpp"
#include "number_types.hpp"
#include "shards/shared.hpp"
#include "translator.hpp"
#include "translator_utils.hpp"
#include "composition.hpp"
#include <nameof.hpp>

#include "shards/core.hpp"

namespace gfx {
namespace shader {
using shards::CoreInfo;

struct ConstTranslator {
  static void translate(shards::Const *shard, TranslationContext &context) {
    context.wgslTop = translateConst(shard->_value, context);
  }
};

// Generates global variables
template <typename TShard> struct SetTranslator {
  static void translate(TShard *shard, TranslationContext &context) {
    auto &varName = shard->_name;
    SPDLOG_LOGGER_INFO(context.logger, "gen(set/ref)> {}", varName);

    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Can not set: no value to set"));

    std::unique_ptr<IWGSLGenerated> wgslValue = context.takeWGSLTop();

    WGSLBlock reference = context.assignVariable(varName, shard->_global, false, std::move(wgslValue));
    context.setWGSLTop<WGSLBlock>(std::move(reference));
  }
};

struct GetTranslator {
  static void translateByName(const char *varName, TranslationContext &context) {
    SPDLOG_LOGGER_INFO(context.logger, "gen(get)> {}", varName);

    context.setWGSLTop<WGSLBlock>(context.reference(varName));
  }

  static void translate(shards::Get *shard, TranslationContext &context) { translateByName(shard->_name.c_str(), context); }
};

struct UpdateTranslator {
  static void translate(shards::Update *shard, TranslationContext &context) {
    auto &varName = shard->_name;
    SPDLOG_LOGGER_INFO(context.logger, "gen(upd)> {}", varName);

    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Can not update: no value to set"));

    std::unique_ptr<IWGSLGenerated> wgslValue = context.takeWGSLTop();

    WGSLBlock reference = context.assignVariable(varName, shard->_global, true, std::move(wgslValue));
    context.setWGSLTop<WGSLBlock>(std::move(reference));
  }
};

// Generates vector swizzles
struct TakeTranslator {
  static char getComponentName(SHInt index) {
    static constexpr auto numComponents = std::size(componentNames);
    if (index < 0 || index >= numComponents)
      throw std::out_of_range("Take component index");
    return componentNames[index];
  }

  static std::string generateSwizzle(shards::Take *shard) {
    std::string result;

    if (shard->_indices.valueType == SHType::Int) {
      result = getComponentName(shard->_indices.payload.intValue);
    } else if (shard->_indices.valueType == SHType::Seq) {
      SHSeq indices = shard->_indices.payload.seqValue;
      for (size_t i = 0; i < indices.len; i++) {
        auto &elem = indices.elements[i];
        if (elem.valueType != SHType::Int)
          throw ShaderComposeError("Take indices should be integers");
        int index = elem.payload.intValue;
        result += getComponentName(index);
      }
    } else {
      throw ShaderComposeError("Take index should be an integer or sequence of integers");
    }

    return result;
  }

  static void translate(shards::Take *shard, TranslationContext &context) {
    if (!shard->_vectorInputType || !shard->_vectorOutputType)
      throw ShaderComposeError(fmt::format("Take: only supports vector types inside shaders"));

    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Can not update: no value to set"));

    std::unique_ptr<IWGSLGenerated> wgslValue = context.takeWGSLTop();

    std::string swizzle = generateSwizzle(shard);

    auto &outVectorType = *shard->_vectorOutputType;
    FieldType outFieldType = getShaderBaseType(outVectorType.numberType);
    outFieldType.numComponents = outVectorType.dimension;

    SPDLOG_LOGGER_INFO(context.logger, "gen(take)> {}", swizzle);

    context.setWGSLTop<WGSLBlock>(outFieldType, blocks::makeCompoundBlock("(", wgslValue->toBlock(), ")." + swizzle));
  }
};

struct Literal {
  shards::ParamVar _value;

  SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  SHParametersInfo parameters() {
    static shards::Parameters params = {
        {"Source", SHCCSTR("The shader source to insert"), {CoreInfo::StringOrStringVar}},
    };
    return params;
  };

  void setParam(int index, const SHVar &value) { this->_value = value; }
  SHVar getParam(int index) { return this->_value; }

  void warmup(SHContext *shContext) { _value.warmup(shContext); }
  void cleanup() { _value.cleanup(); }

  SHVar activate(SHContext *shContext, const SHVar &input) { return SHVar{}; }

  void translate(TranslationContext &context) {
    const SHString &source = _value.get().payload.stringValue;
    SPDLOG_LOGGER_INFO(context.logger, "gen(direct)> {}", source);

    context.addNew(blocks::makeBlock<blocks::Direct>(source));
    context.clearWGSLTop();
  }
};

struct IOBase {
  struct Type {
    shards::Types shardsTypes;
    FieldType shaderType;
  };

  std::string _name;
  Type _type;

  static inline shards::Parameters params = {
      {"Name", SHCCSTR("The name of the field to read/write"), {CoreInfo::StringType}},
  };

  SHParametersInfo parameters() { return params; };

  SHTypeInfo compose(const SHInstanceData &data) { return CoreInfo::NoneType; }

  void setParam(int index, const SHVar &value) {
    using shards::Var;
    switch (index) {
    case 0:
      this->_name = value.payload.stringValue;
      break;
    }
  }
  SHVar getParam(int index) {
    using shards::Var;
    switch (index) {
    case 0:
      return Var(_name);
    default:
      return Var::Empty;
    }
  }

  SHVar activate(SHContext *shContext, const SHVar &input) { return SHVar{}; }
};

template <typename T> struct BlockTypeResolver {};

template <> struct BlockTypeResolver<blocks::ReadInput> {
  static IOBase::Type resolveType(ShaderCompositionContext &ctx, const std::string &name) {
    auto &defs = ctx.generatorContext.getDefinitions().inputs;
    auto it = defs.find(name);
    if (it == defs.end())
      throw shards::ComposeError(fmt::format("Shader input \"{}\" not found", name));
    return IOBase::Type{shards::Types{fieldTypeToShardsType(it->second)}, it->second};
  }
};

template <> struct BlockTypeResolver<blocks::ReadGlobal> {
  static IOBase::Type resolveType(ShaderCompositionContext &ctx, const std::string &name) {
    auto &defs = ctx.generatorContext.getDefinitions().globals;
    auto it = defs.find(name);
    if (it == defs.end())
      throw shards::ComposeError(fmt::format("Shader global \"{}\" not found", name));
    return IOBase::Type{shards::Types{fieldTypeToShardsType(it->second)}, it->second};
  }
};

template <typename TShard> struct Read final : public IOBase {
  SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  SHTypesInfo outputTypes() { return _type.shardsTypes; }

  SHTypeInfo compose(const SHInstanceData &data) {
    auto &shaderCtx = ShaderCompositionContext::get();
    _type = BlockTypeResolver<TShard>::resolveType(shaderCtx, _name);
    return _type.shardsTypes._types[0];
  }

  void translate(TranslationContext &context) {
    SPDLOG_LOGGER_INFO(context.logger, "gen(read/{})> {}", NAMEOF_TYPE(TShard), _name);

    context.setWGSLTop<WGSLBlock>(_type.shaderType, blocks::makeBlock<TShard>(_name));
  }
};

// Override for reading a value from a named buffer
struct ReadBuffer final : public IOBase {
  std::string _bufferName = "object";

  SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  SHTypesInfo outputTypes() { return _type.shardsTypes; }

  SHTypeInfo compose(const SHInstanceData &data) {
    auto &shaderCtx = ShaderCompositionContext::get();

    // Find buffer
    auto &buffers = shaderCtx.generatorContext.getDefinitions().buffers;
    auto bufferIt = buffers.find(_bufferName);
    if (bufferIt == buffers.end())
      throw shards::ComposeError(fmt::format("Shader buffer \"{}\" does not exist", _bufferName));

    // Find field in buffer
    auto &buffer = bufferIt->second;
    const UniformLayout *field = buffer.findField(_name.c_str());
    if (!field)
      throw shards::ComposeError(fmt::format("Shader parameter \"{}\" does not exist in buffer \"{}\"", _name, _bufferName));

    _type.shaderType = field->type;
    _type.shardsTypes = shards::Types{fieldTypeToShardsType(field->type)};
    return _type.shardsTypes._types[0];
  }

  SHParametersInfo parameters() {
    using shards::CoreInfo;
    using shards::Parameters;
    static Parameters params(IOBase::params, {
                                                 // IOBase::params,
                                                 {"BufferName",
                                                  SHCCSTR("The name of the buffer to read from (object/view) (default: object)"),
                                                  {CoreInfo::StringType}},
                                             });
    return params;
  }

  void setParam(int index, const SHVar &value) {
    if (index == 1) {
      _bufferName = value.payload.stringValue;
    }
    IOBase::setParam(index, value);
  }

  SHVar getParam(int index) {
    using shards::Var;
    if (index == 1) {
      return Var(_bufferName);
    }
    return IOBase::getParam(index);
  }

  void translate(TranslationContext &context) {
    SPDLOG_LOGGER_INFO(context.logger, "gen(read/{})> {}.{}", NAMEOF_TYPE(blocks::ReadBuffer), _bufferName, _name);

    context.setWGSLTop<WGSLBlock>(_type.shaderType, blocks::makeBlock<blocks::ReadBuffer>(_name, _type.shaderType, _bufferName));
  }
};

template <typename TShard> struct Write : public IOBase {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }

  void translate(TranslationContext &context) {
    SPDLOG_LOGGER_INFO(context.logger, "gen(write/{})> {}", NAMEOF_TYPE(TShard), _name);

    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Can not write: value is required"));

    std::unique_ptr<IWGSLGenerated> wgslValue = context.takeWGSLTop();
    FieldType fieldType = wgslValue->getType();

    context.addNew(blocks::makeBlock<TShard>(_name, fieldType, wgslValue->toBlock()));
  }
};

struct SampleTexture {
  static inline shards::Parameters params{
      {"Name", SHCCSTR("Name of the texture"), {CoreInfo::StringOrStringVar}},
  };

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::Float4Type; }
  static SHOptionalString help() { return SHCCSTR("Samples a named texture with default texture coordinates"); }
  SHParametersInfo parameters() { return params; };

  shards::Var _name;

  void setParam(int index, const SHVar &value) { _name = value; }
  SHVar getParam(int index) { return _name; }

  void warmup(SHContext *shContext) {}
  void cleanup() {}

  SHVar activate(SHContext *shContext, const SHVar &input) { return SHVar{}; }

  void translate(TranslationContext &context) {
    const SHString &textureName = _name.payload.stringValue;
    SPDLOG_LOGGER_INFO(context.logger, "gen(sample)> {}", textureName);

    context.setWGSLTopVar(FieldTypes::Float4, blocks::makeBlock<blocks::SampleTexture>(textureName));
  }
};

struct SampleTextureUV : public SampleTexture {
  static inline shards::Types uvTypes{CoreInfo::Float4Type, CoreInfo::Float3Type, CoreInfo::Float2Type, CoreInfo::FloatType};

  static SHTypesInfo inputTypes() { return uvTypes; }
  static SHTypesInfo outputTypes() { return CoreInfo::Float4Type; }

  static SHOptionalString help() { return SHCCSTR("Samples a named texture with the passed in texture coordinates"); }

  SHParametersInfo parameters() { return SampleTexture::params; };

  void translate(TranslationContext &context) {
    const SHString &textureName = _name.payload.stringValue;
    SPDLOG_LOGGER_INFO(context.logger, "gen(sample/uv)> {}", textureName);

    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Can not sample texture: coordinate is required"));

    std::unique_ptr<IWGSLGenerated> wgslValue = context.takeWGSLTop();
    FieldType fieldType = wgslValue->getType();

    auto block = blocks::makeCompoundBlock();
    const std::string &varName = context.getUniqueVariableName();
    if (fieldType.numComponents < 2) {
      block->appendLine(fmt::format("let {} = vec2<f32>(", varName), wgslValue->toBlock(), ", 0.0)");
    } else {
      block->appendLine(fmt::format("let {} = (", varName), wgslValue->toBlock(), ").xy");
    }

    context.addNew(std::move(block));
    context.setWGSLTopVar(FieldTypes::Float4, blocks::makeBlock<blocks::SampleTexture>(textureName, varName));
  }
};

struct LinearizeDepth {
  static SHTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }

  void warmup(SHContext *shContext) {}
  void cleanup() {}

  SHVar activate(SHContext *shContext, const SHVar &input) { return SHVar{}; }

  void translate(TranslationContext &context) {
    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Depth input required"));

    std::unique_ptr<IWGSLGenerated> wgslValue = context.takeWGSLTop();

    context.setWGSLTopVar(FieldTypes::Float, blocks::makeBlock<blocks::LinearizeDepth>(wgslValue->toBlock()));
  }
};

} // namespace shader
} // namespace gfx

#endif // SH_EXTRA_GFX_SHADER_CORE_BLOCKS
