#ifndef SH_EXTRA_GFX_SHADER_CORE_BLOCKS
#define SH_EXTRA_GFX_SHADER_CORE_BLOCKS

// Required before shard headers
#include "../shards_types.hpp"
#include "../drawable_utils.hpp"
#include "composition.hpp"
#include "translator.hpp"
#include "wgsl.hpp"
#include <shards/core/foundation.hpp>
#include "gfx/enums.hpp"
#include "gfx/error_utils.hpp"
#include <gfx/shader/fmt.hpp>
#include <gfx/shader/block.hpp>
#include <gfx/shader/blocks.hpp>
#include <gfx/shader/generator.hpp>
#include <gfx/shader/types.hpp>
#include <gfx/shader/struct_layout.hpp>
#include "magic_enum.hpp"
#include <shards/number_types.hpp>
#include <shards/core/runtime.hpp>
#include <shards/shards.h>
#include <shards/core/shared.hpp>
#include "translator.hpp"
#include "translator_utils.hpp"
#include "composition.hpp"
#include <nameof.hpp>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <shards/core/params.hpp>
#include <shards/modules/core/core.hpp>

namespace gfx::shader {
enum class ShaderLiteralType {
  Inline,
  Header,
};
}

DECL_ENUM_INFO(gfx::shader::ShaderLiteralType, ShaderLiteralType, '_slt');
ENUM_HELP(gfx::shader::ShaderLiteralType, gfx::shader::ShaderLiteralType::Inline,
          SHCCSTR("Insert shader code directly into current scope"));
ENUM_HELP(gfx::shader::ShaderLiteralType, gfx::shader::ShaderLiteralType::Header,
          SHCCSTR("Insert shader code into header, where it is defined before all shards shader code"));

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
    SPDLOG_LOGGER_TRACE(context.logger, "gen(set/ref)> {}", varName);

    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Can not set: no value to set"));

    std::unique_ptr<IWGSLGenerated> wgslValue = context.takeWGSLTop();

    bool storeMutable{};
    if constexpr (std::is_same_v<TShard, shards::Ref>) {
      storeMutable = false;
    } else {
      if (!std::get_if<NumType>(&wgslValue->getType())) {
        throw ShaderComposeError(fmt::format("Type {} can not be stored as a mutable variable", wgslValue->getType()));
      }
      storeMutable = true;
    }

    WGSLBlock reference = context.assignVariable(varName, shard->_global, false, storeMutable, std::move(wgslValue));
    context.setWGSLTop<WGSLBlock>(std::move(reference));
  }
};

struct GetTranslator {
  static void translateByName(const std::string &varName, TranslationContext &context) {
    SPDLOG_LOGGER_TRACE(context.logger, "gen(get)> {}", varName);

    auto &compositionContext = ShaderCompositionContext::get();

    if (auto composeVar = compositionContext.getComposeTimeConstant(varName)) {
      context.wgslTop = translateConst(*composeVar, context);
    } else {
      context.setWGSLTop<WGSLBlock>(context.reference(varName));
    }
  }

  static void translate(shards::Get *shard, TranslationContext &context) { translateByName(shard->_name, context); }
};

struct UpdateTranslator {
  static void translate(shards::Update *shard, TranslationContext &context) {
    auto &varName = shard->_name;
    SPDLOG_LOGGER_TRACE(context.logger, "gen(upd)> {}", varName);

    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Can not update: no value to set"));

    std::unique_ptr<IWGSLGenerated> wgslValue = context.takeWGSLTop();

    WGSLBlock reference = context.assignVariable(varName, shard->_global, true, true, std::move(wgslValue));
    context.setWGSLTop<WGSLBlock>(std::move(reference));
  }
};

// Generates vector swizzles
struct TakeTranslator {
  static char getComponentName(SHInt index) {
    static constexpr auto numComponents = SHInt(std::size(componentNames));
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
    NumType outFieldType = getShaderBaseType(outVectorType.numberType);
    outFieldType.numComponents = outVectorType.dimension;

    SPDLOG_LOGGER_TRACE(context.logger, "gen(take)> {}", swizzle);

    context.setWGSLTop<WGSLBlock>(outFieldType, blocks::makeCompoundBlock("(", wgslValue->toBlock(), ")." + swizzle));
  }
};

struct Literal {
  SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  SHTypesInfo outputTypes() {
    static shards::Types types{CoreInfo::AnyType, CoreInfo::NoneType};
    return types;
  }

  static inline shards::Types FormatSeqValueTypes{CoreInfo::StringType, shards::Type::VariableOf(CoreInfo::AnyType)};
  static inline shards::Type FormatSeqType = shards::Type::SeqOf(FormatSeqValueTypes);

  PARAM(shards::OwnedVar, _source, "Source", "The WGSL source code to insert", {CoreInfo::StringOrStringVar, {FormatSeqType}});
  PARAM_VAR(_type, "Type", "Where to insert the code.", {ShaderLiteralTypeEnumInfo::Type});
  PARAM_VAR(_outType, "OutputType", "The type that this code is expected to output. (default: none)",
            {ShardsTypes::ShaderFieldBaseTypeEnumInfo::Type});
  PARAM_VAR(_outDimension, "OutputDimension", "The dimension that this code is expected to output. (default: 4)",
            {CoreInfo::IntType});
  PARAM_VAR(_outMatrixDimension, "OutputMatrixDimension",
            "The matrix dimension that this code is expected to output. (default: 1)", {CoreInfo::IntType});
  PARAM_IMPL(PARAM_IMPL_FOR(_source), PARAM_IMPL_FOR(_type), PARAM_IMPL_FOR(_outType), PARAM_IMPL_FOR(_outDimension),
             PARAM_IMPL_FOR(_outMatrixDimension));

  // Constant for unset OutputType parameter
  static inline const SHEnum InvalidType = SHEnum(~0);

  shards::Type _outputType{};

  Literal() {
    _outType.payload.enumValue = InvalidType;
    _outDimension.payload.intValue = 4;
    _outMatrixDimension.payload.intValue = 1;
  }

  std::optional<Type> getOutputType() const {
    int dimension = 4;
    int matrixDimension = 1;
    ShaderFieldBaseType baseType = ShaderFieldBaseType::Float32;
    bool isSet{};

    if (!_outDimension->isNone()) {
      dimension = int(*_outDimension);
      isSet = true;
    }
    if (!_outMatrixDimension->isNone()) {
      matrixDimension = int(*_outMatrixDimension);
      isSet = true;
    }
    if (!_outType->isNone()) {
      baseType = ShaderFieldBaseType(_outType.payload.enumValue);
      isSet = true;
    }

    if (isSet) {
      return NumType(baseType, dimension, matrixDimension);
    }
    return std::nullopt;
  }

  SHTypeInfo compose(SHInstanceData &data) {
    auto type = ShaderLiteralType(_type.payload.enumValue);

    auto outputFieldType = getOutputType();

    if (outputFieldType.has_value()) {
      if (type == ShaderLiteralType::Header) {
        throw shards::ComposeError("Output type can not be set for Header type shader literals");
      }
      _outputType = fieldTypeToShardsType(outputFieldType.value());
    } else {
      _outputType = shards::CoreInfo::NoneType;
    }

    if (_source.valueType == SHType::None)
      throw shards::ComposeError("Source is required");

    return _outputType;
  }

  void warmup(SHContext *shContext) { PARAM_WARMUP(shContext); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *shContext, const SHVar &input) { return SHVar{}; }

  std::optional<std::string_view> getComposeTimeConstant(ShaderCompositionContext &compositionContext, const std::string &key) {
    if (auto constValue = compositionContext.getComposeTimeConstant(key)) {
      if (constValue->valueType != SHType::String) {
        throw formatError("Composition variable {}, has an invalid value {} (Only string is allowed)", key, *constValue);
      }
      return SHSTRVIEW((*constValue));
    }
    return std::nullopt;
  }

  void generateSourceElement(ShaderCompositionContext &compositionContext, TranslationContext &context, blocks::Compound &output,
                             const SHVar &elem) {
    if (elem.valueType == SHType::String) {
      output.append(SHSTRVIEW(elem));
    } else if (elem.valueType == SHType::ContextVar) {
      std::string key = SHSTRING_PREFER_SHSTRVIEW(elem);
      if (auto constValue = getComposeTimeConstant(compositionContext, key)) {
        output.append(*constValue);
      } else {
        WGSLBlock ref = context.reference(key);
        output.append(std::move(ref.block));
      }
    } else {
      throw std::runtime_error(fmt::format("Invalid shader literal source type {}", elem));
    }
  }

  BlockPtr generateBlock(TranslationContext &context) {
    auto &compositionContext = ShaderCompositionContext::get();

    if (_source.valueType == SHType::String) {
      return blocks::makeBlock<blocks::Direct>(SHSTRVIEW(_source));
    } else if (_source.valueType == SHType::ContextVar) {
      auto key = SHSTRING_PREFER_SHSTRVIEW(_source);
      if (auto constValue = getComposeTimeConstant(compositionContext, key)) {
        return blocks::makeBlock<blocks::Direct>(*constValue);
      } else {
        throw formatError("Composition variable {} not found", key);
      }
    } else if (_source.valueType == SHType::Seq) {
      auto compound = blocks::makeBlock<blocks::Compound>();
      const SHSeq &formatSeq = _source.payload.seqValue;
      for (uint32_t i = 0; i < formatSeq.len; i++) {
        generateSourceElement(compositionContext, context, *compound.get(), formatSeq.elements[i]);
      }
      return compound;
    } else {
      throw std::runtime_error(fmt::format("Invalid shader literal source type {}", _source));
    }
  }

  void translate(TranslationContext &context) {
    auto type = ShaderLiteralType(_type.payload.enumValue);

    bool isDynamic = _source.valueType != SHType::String;
    SPDLOG_LOGGER_TRACE(context.logger, "gen(literal/{})> {}", magic_enum::enum_name(type),
                       isDynamic ? "dynamic" : SHSTRVIEW(_source));

    auto outputFieldType = getOutputType();
    blocks::BlockPtr block;
    if (type == ShaderLiteralType::Inline) {
      block = generateBlock(context);
    } else if (type == ShaderLiteralType::Header) {
      outputFieldType = std::nullopt;
      block = blocks::makeBlock<blocks::Header>(generateBlock(context));
    }

    if (outputFieldType.has_value()) {
      context.setWGSLTopVar(outputFieldType.value(), std::move(block));
    } else {
      context.addNew(std::move(block));
      context.clearWGSLTop();
    }
  }
};

struct IOBase {
  struct Type {
    shards::Types shardsTypes;
    shader::Type shaderType;
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
      this->_name = SHSTRVIEW(value);
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
    SPDLOG_LOGGER_TRACE(context.logger, "gen(read/{})> {}", NAMEOF_TYPE(TShard), _name);

    context.setWGSLTop<WGSLBlock>(_type.shaderType, blocks::makeBlock<TShard>(_name));
  }
};

// Override for reading a value from a named buffer
struct ReadBuffer final : public IOBase {
  std::string _bufferName;
  std::string _resolvedBufferName;

  SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  SHTypesInfo outputTypes() { return _type.shardsTypes; }

  const auto &findBufferContainingParam(const std::string &fieldName) {
    auto &shaderCtx = ShaderCompositionContext::get();
    for (auto &b : shaderCtx.generatorContext.getDefinitions().buffers) {
      auto field = b.second.findField(fieldName.c_str());
      if (field)
        return b;
    }
    throw shards::ComposeError(fmt::format("Failed to find shader parameter \"{}\" in any buffer", fieldName));
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    auto &shaderCtx = ShaderCompositionContext::get();

    const BufferDefinition *buffer{};
    if (_bufferName.empty()) {
      auto &[name, def] = findBufferContainingParam(_name);
      _resolvedBufferName = name;
      buffer = &def;
    } else {
      // Find buffer
      auto &buffers = shaderCtx.generatorContext.getDefinitions().buffers;
      auto bufferIt = buffers.find(_bufferName);
      if (bufferIt == buffers.end())
        throw shards::ComposeError(fmt::format("Shader buffer \"{}\" does not exist", _bufferName));
      buffer = &bufferIt->second;
      _resolvedBufferName = _bufferName;
    }

    // Find field in buffer
    const StructField *field = buffer->findField(_name.c_str());
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
      _bufferName = SHSTRVIEW(value);
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
    SPDLOG_LOGGER_TRACE(context.logger, "gen(read/{})> {}.{}", NAMEOF_TYPE(blocks::ReadBuffer), _resolvedBufferName, _name);

    NumType fieldType = std::get<NumType>(_type.shaderType);
    context.setWGSLTop<WGSLBlock>(_type.shaderType, blocks::makeBlock<blocks::ReadBuffer>(_name, fieldType, _resolvedBufferName));
  }
};

template <typename TShard> struct Write : public IOBase {
  std::optional<NumType> _castIntoType;

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }

  static inline bool isCompatibleOutputType(NumType dstType, NumType srcType, bool &needCast) {
    // Allow implicit integer type conversions
    // for example when attachment is uint8/int8 but shader only uses shards types (int32)
    if (isIntegerType(dstType.baseType) && isIntegerType(srcType.baseType)) {
      needCast = true;
      return srcType.numComponents == dstType.numComponents;
    }
    return dstType == srcType;
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    auto &shaderCtx = ShaderCompositionContext::get();

    if constexpr (std::is_same_v<TShard, blocks::WriteOutput>) {
      auto inTypeOpt = deriveShaderFieldType(data.inputType);
      if (!inTypeOpt)
        throw formatException("Invalid output type {}", data.inputType);

      const NumType *outputType{};
      {
        auto &outputs = shaderCtx.generatorContext.getDefinitions().outputs;
        auto outputIt = outputs.find(_name);
        if (outputIt != outputs.end()) {
          outputType = &outputIt->second;
        } else {
          outputType = shaderCtx.generatorContext.getOrCreateDynamicOutput(_name.c_str(), std::get<NumType>(inTypeOpt.value()));
          if (!outputType) {
            throw formatException("Output \"{}\" does not exist", _name);
          }
        }
      }

      NumType inType = std::get<NumType>(inTypeOpt.value());

      bool needCast{};
      if (!isCompatibleOutputType(*outputType, inType, needCast))
        throw formatException("Output {} ({}) can not be assigned from type {}", _name, *outputType, data.inputType);

      if (needCast)
        _castIntoType = *outputType;
    }

    return CoreInfo::NoneType;
  }

  void translate(TranslationContext &context) {
    SPDLOG_LOGGER_TRACE(context.logger, "gen(write/{})> {}", NAMEOF_TYPE(TShard), _name);

    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Can not write: value is required"));

    std::unique_ptr<IWGSLGenerated> wgslValue = context.takeWGSLTop();

    if (_castIntoType) {
      context.addNew(blocks::makeBlock<TShard>(
          _name, _castIntoType.value(),
          blocks::makeCompoundBlock(getWGSLTypeName(_castIntoType.value()), "(", wgslValue->toBlock(), ")")));
    } else {
      NumType fieldType = std::get<NumType>(wgslValue->getType());
      context.addNew(blocks::makeBlock<TShard>(_name, fieldType, wgslValue->toBlock()));
    }
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

  shards::OwnedVar _name;

  void setParam(int index, const SHVar &value) { _name = value; }
  SHVar getParam(int index) { return _name; }

  void warmup(SHContext *shContext) {}
  void cleanup(SHContext *context) {}

  SHVar activate(SHContext *shContext, const SHVar &input) { return SHVar{}; }

  SHTypeInfo compose(SHInstanceData &data) {
    auto name = _name.payload.stringValue; // null term ok
    auto &shaderCtx = ShaderCompositionContext::get();
    auto &textures = shaderCtx.generatorContext.getDefinitions().textures;
    auto it = textures.find(name);
    if (it == textures.end()) {
      throw shards::ComposeError(fmt::format("Shader texture \"{}\" not found", name));
    }
    if (it->second.type.dimension != TextureDimension::D2) {
      throw formatException("SampleTexture does not support texture for type [{}]",
                            magic_enum::enum_name(it->second.type.dimension));
    }

    return outputTypes().elements[0];
  }

  void translate(TranslationContext &context) {
    const SHString &textureName = _name.payload.stringValue; // null term ok
    SPDLOG_LOGGER_TRACE(context.logger, "gen(sample)> {}", textureName);

    context.setWGSLTopVar(Types::Float4, blocks::makeBlock<blocks::SampleTexture>(textureName));
  }
};

struct SampleTextureCoord : public SampleTexture {
  static inline shards::Types uvTypes{CoreInfo::Float4Type, CoreInfo::Float3Type, CoreInfo::Float2Type, CoreInfo::FloatType};

  static SHTypesInfo inputTypes() { return uvTypes; }
  static SHTypesInfo outputTypes() { return CoreInfo::Float4Type; }

  static SHOptionalString help() { return SHCCSTR("Samples a named texture with the passed in texture coordinates"); }

  SHParametersInfo parameters() { return SampleTexture::params; };

  SHTypeInfo getExpectedCoordinateType(const TextureDefinition &def) {
    switch (def.type.dimension) {
    case gfx::TextureDimension::D1:
      return CoreInfo::FloatType;
    case gfx::TextureDimension::D2:
      return CoreInfo::Float2Type;
    case gfx::TextureDimension::Cube:
      return CoreInfo::Float3Type;
    default:
      throw std::out_of_range("SampleTextureCoord(TextureType)");
    }
  }

  SHTypeInfo compose(SHInstanceData &data) {
    auto name = _name.payload.stringValue; // should be safe null term wise
    auto &shaderCtx = ShaderCompositionContext::get();
    auto &textures = shaderCtx.generatorContext.getDefinitions().textures;
    auto it = textures.find(name);
    if (it == textures.end()) {
      throw shards::ComposeError(fmt::format("Shader texture \"{}\" not found", name));
    }

    SHTypeInfo expectedInputType = getExpectedCoordinateType(it->second);
    if (data.inputType != expectedInputType) {
      throw formatException("SampleTextureCoord does not accept input type {}, should be {}", data.inputType, expectedInputType);
    }

    return outputTypes().elements[0];
  }

  void translate(TranslationContext &context) {
    const SHString &textureName = _name.payload.stringValue; // null term ok
    SPDLOG_LOGGER_TRACE(context.logger, "gen(sample/uv)> {}", textureName);

    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Can not sample texture: coordinate is required"));

    std::unique_ptr<IWGSLGenerated> wgslValue = context.takeWGSLTop();

    auto &varName = context.assignTempVar(wgslValue->toBlock());
    context.setWGSLTopVar(Types::Float4, blocks::makeBlock<blocks::SampleTexture>(textureName, varName));
  }
};

struct RefTexture {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return ShardsTypes::TextureTypes; }

  static SHOptionalString help() { return SHCCSTR("Returns a reference to the texture object for a named texture."); }

  SHParametersInfo parameters() { return SampleTexture::params; }

  shards::OwnedVar _name;
  TextureType _textureType;

  void setParam(int index, const SHVar &value) { _name = value; }
  SHVar getParam(int index) { return _name; }

  SHTypeInfo compose(SHInstanceData &data) {
    auto name = _name.payload.stringValue; // null term ok
    auto &shaderCtx = ShaderCompositionContext::get();
    auto &textures = shaderCtx.generatorContext.getDefinitions().textures;
    auto it = textures.find(name);
    if (it == textures.end()) {
      throw shards::ComposeError(fmt::format("Shader texture \"{}\" not found", name));
    }
    _textureType = it->second.type;

    return fieldTypeToShardsType(it->second.type);
  }

  void translate(TranslationContext &context) {
    const SHString &textureName = _name.payload.stringValue; // null term ok
    SPDLOG_LOGGER_TRACE(context.logger, "gen(ref/texture)> {}", textureName);

    auto block = std::make_unique<blocks::Custom>([=](IGeneratorContext &ctx) { ctx.texture(textureName); });
    context.setWGSLTopVar(_textureType, std::move(block));
  }

  SHVar activate(SHContext *shContext, const SHVar &input) { return SHVar{}; }
};

struct RefSampler {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return ShardsTypes::Sampler; }

  static SHOptionalString help() { return SHCCSTR("Returns a reference to the default sampler object for a named texture."); }

  SHParametersInfo parameters() { return SampleTexture::params; }

  shards::OwnedVar _name;
  SamplerType _samplerType;

  void setParam(int index, const SHVar &value) { _name = value; }
  SHVar getParam(int index) { return _name; }

  SHTypeInfo compose(SHInstanceData &data) {
    auto name = _name.payload.stringValue; // null term ok
    auto &shaderCtx = ShaderCompositionContext::get();
    auto &textures = shaderCtx.generatorContext.getDefinitions().textures;
    auto it = textures.find(name);
    if (it == textures.end()) {
      throw shards::ComposeError(fmt::format("Shader texture \"{}\" not found", name));
    }
    _samplerType = SamplerType{};
    return ShardsTypes::Sampler;
  }

  void translate(TranslationContext &context) {
    const SHString &textureName = _name.payload.stringValue; // null term ok
    SPDLOG_LOGGER_TRACE(context.logger, "gen(ref/sampler)> {}", textureName);

    auto block = std::make_unique<blocks::Custom>([=](IGeneratorContext &ctx) { ctx.textureDefaultSampler(textureName); });
    context.setWGSLTopVar(_samplerType, std::move(block));
  }

  SHVar activate(SHContext *shContext, const SHVar &input) { return SHVar{}; }
};

struct RefBuffer {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return ShardsTypes::Buffer; }

  static SHOptionalString help() { return SHCCSTR("Returns a reference to the default sampler object for a named texture."); }

  PARAM_VAR(_name, "Name", "The name of the buffer", {CoreInfo::StringType});
  PARAM_VAR(_pointer, "Pointer", "Reference as pointer", {CoreInfo::NoneType, CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_name), PARAM_IMPL_FOR(_pointer));

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    auto name = _name.payload.stringValue; // null term ok
    auto &shaderCtx = ShaderCompositionContext::get();
    auto &buffers = shaderCtx.generatorContext.getDefinitions().buffers;
    auto it = buffers.find(name);
    if (it == buffers.end()) {
      throw shards::ComposeError(fmt::format("Shader texture \"{}\" not found", name));
    }
    return ShardsTypes::Buffer;
  }

  void translate(TranslationContext &context) {
    const SHString &bufferName = _name.payload.stringValue; // null term ok
    SPDLOG_LOGGER_INFO(context.logger, "gen(ref/buffer)> {}", bufferName);

    bool isRef = _pointer->isNone() || (bool)*_pointer;
    auto block = std::make_unique<blocks::Custom>([=](IGeneratorContext &ctx) {
      if(isRef) {
        ctx.write("&");
      }
      ctx.refBuffer(bufferName);
    });
    context.setWGSLTopVar(StructType{}, std::move(block));
  }

  SHVar activate(SHContext *shContext, const SHVar &input) { return SHVar{}; }
};

struct LinearizeDepth {
  static SHTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }

  void warmup(SHContext *shContext) {}
  void cleanup(SHContext *context) {}

  SHVar activate(SHContext *shContext, const SHVar &input) { return SHVar{}; }

  void translate(TranslationContext &context) {
    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Depth input required"));

    std::unique_ptr<IWGSLGenerated> wgslValue = context.takeWGSLTop();

    context.setWGSLTopVar(Types::Float, blocks::makeBlock<blocks::LinearizeDepth>(wgslValue->toBlock()));
  }
};

struct WithInput {
  PARAM_VAR(_name, "Name", "The name of the input", {CoreInfo::StringType});
  PARAM(shards::ShardsVar, _then, "Then", "The shards to execute if the input exists", {CoreInfo::ShardsOrNone});
  PARAM(shards::ShardsVar, _else, "Else", "The shards to execute if the input does not exist", {CoreInfo::ShardsOrNone});

  PARAM_IMPL(PARAM_IMPL_FOR(_name), PARAM_IMPL_FOR(_then), PARAM_IMPL_FOR(_else));

  WithInput() { _name = shards::Var(""); }

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    ShaderCompositionContext &shaderCompositionContext = ShaderCompositionContext::get();
    bool hasInput = shaderCompositionContext.generatorContext.hasInput(SHSTRING_PREFER_SHSTRVIEW(*_name).c_str());
    if (hasInput) {
      _then.compose(data);
    } else {
      _else.compose(data);
    }
    return CoreInfo::NoneType;
  }

  void warmup(SHContext *shContext) { PARAM_WARMUP(shContext); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *shContext, const SHVar &input) { return SHVar{}; }

  void translate(TranslationContext &context) {
    ShaderCompositionContext &shaderCompositionContext = ShaderCompositionContext::get();
    bool hasInput = shaderCompositionContext.generatorContext.hasInput(SHSTRING_PREFER_SHSTRVIEW(*_name).c_str());

    if (hasInput) {
      if (_then)
        processShardsVar(_then, context);
    } else {
      if (_else)
        processShardsVar(_else, context);
    }
  }
};

struct WithTexture {
  PARAM_VAR(_name, "Name", "The name of the texture", {CoreInfo::StringType});
  PARAM(shards::ShardsVar, _then, "Then", "The shards to execute if the texture exists", {CoreInfo::ShardsOrNone});
  PARAM(shards::ShardsVar, _else, "Else", "The shards to execute if the texture does not exist", {CoreInfo::ShardsOrNone});

  PARAM_IMPL(PARAM_IMPL_FOR(_name), PARAM_IMPL_FOR(_then), PARAM_IMPL_FOR(_else));

  WithTexture() { _name = shards::Var(""); }

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    ShaderCompositionContext &shaderCompositionContext = ShaderCompositionContext::get();
    bool hasTexture = shaderCompositionContext.generatorContext.hasTexture(SHSTRING_PREFER_SHSTRVIEW(*_name).c_str());
    if (hasTexture) {
      _then.compose(data);
    } else {
      _else.compose(data);
    }
    return CoreInfo::NoneType;
  }

  void warmup(SHContext *shContext) { PARAM_WARMUP(shContext); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *shContext, const SHVar &input) { return SHVar{}; }

  void translate(TranslationContext &context) {
    ShaderCompositionContext &shaderCompositionContext = ShaderCompositionContext::get();
    bool hasTexture = shaderCompositionContext.generatorContext.hasTexture(SHSTRING_PREFER_SHSTRVIEW(*_name).c_str());

    if (hasTexture) {
      if (_then)
        processShardsVar(_then, context);
    } else {
      if (_else)
        processShardsVar(_else, context);
    }
  }
};

} // namespace shader
} // namespace gfx

#endif // SH_EXTRA_GFX_SHADER_CORE_BLOCKS
