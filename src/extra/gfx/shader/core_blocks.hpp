#ifndef CB_EXTRA_GFX_SHADER_CORE_BLOCKS
#define CB_EXTRA_GFX_SHADER_CORE_BLOCKS

// Required before shard headers
#include "../shards_types.hpp"
#include "magic_enum.hpp"
#include "number_types.hpp"
#include "shards/shared.hpp"
#include "translator.hpp"
#include "translator_utils.hpp"

#include "shards/casting.hpp"
#include "shards/core.hpp"
#include <nameof.hpp>

namespace gfx {
namespace shader {
using shards::CoreInfo;

struct ConstTranslator {
  static void translate(shards::Const *shard, TranslationContext &context) {
    context.wgslTop = translateConst(shard->_value, context);
  }
};

// Generates global variables
struct SetTranslator {
  static void translate(shards::Set *shard, TranslationContext &context) {
    auto varName = convertVariableName(shard->_name);
    SPDLOG_LOGGER_INFO(&context.logger, "gen(set)> {}", varName);

    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Can not set: no value to set"));

    std::unique_ptr<IWGSLGenerated> wgslValue;
    std::swap(wgslValue, context.wgslTop);

    FieldType fieldType = wgslValue->getType();

    // Store global variable type info
    auto globalIt = context.globals.find(varName);
    if (globalIt == context.globals.end()) {
      context.globals.insert_or_assign(varName, fieldType);
    } else {
      throw ShaderComposeError(fmt::format("Can not set: value already set"));
    }

    // Generate a shader source block containing the assignment
    context.addNew(blocks::makeBlock<blocks::WriteGlobal>(varName, fieldType, wgslValue->toBlock()));

    // Push reference to assigned value
    context.setWGSLTop<WGSLBlock>(fieldType, blocks::makeBlock<blocks::ReadGlobal>(varName));
  }
};

struct GetTranslator {
  static void translateByName(const char *varName, TranslationContext &context) {
    SPDLOG_LOGGER_INFO(&context.logger, "gen(get)> {}", varName);

    auto globalIt = context.globals.find(varName);
    if (globalIt == context.globals.end()) {
      throw ShaderComposeError(fmt::format("Can not get/ref: global does not exist in this scope"));
    }

    FieldType fieldType = globalIt->second;
    context.setWGSLTop<WGSLBlock>(fieldType, blocks::makeBlock<blocks::ReadGlobal>(varName));
  }

  static void translate(shards::Get *shard, TranslationContext &context) {
    auto varName = convertVariableName(shard->_name);
    translateByName(varName.c_str(), context);
  }
};

struct UpdateTranslator {
  static void translate(shards::Update *shard, TranslationContext &context) {
    auto varName = convertVariableName(shard->_name);
    SPDLOG_LOGGER_INFO(&context.logger, "gen(upd)> {}", varName);

    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Can not update: no value to set"));

    std::unique_ptr<IWGSLGenerated> wgslValue = context.takeWGSLTop();
    FieldType fieldType = wgslValue->getType();

    // Store global variable type info
    auto globalIt = context.globals.find(varName);
    if (globalIt == context.globals.end()) {
      throw ShaderComposeError(fmt::format("Can not update: global does not exist in this scope"));
    }

    if (fieldType != globalIt->second) {
      throw ShaderComposeError(fmt::format("Can not update: Type mismatch assigning {} to global of {}",
                                           getFieldWGSLTypeName(fieldType), getFieldWGSLTypeName(globalIt->second)));
    }

    // Generate a shader source block containing the assignment
    context.addNew(blocks::makeBlock<blocks::WriteGlobal>(varName, fieldType, wgslValue->toBlock()));

    // Push reference to assigned value
    context.setWGSLTop<WGSLBlock>(fieldType, blocks::makeBlock<blocks::ReadGlobal>(varName));
  }
};

// Generates vector swizzles
struct TakeTranslator {
  static void translate(shards::Take *shard, TranslationContext &context) {
    if (!shard->_vectorInputType || !shard->_vectorOutputType)
      throw ShaderComposeError(fmt::format("Take: only supports vector types inside shaders"));

    SPDLOG_LOGGER_INFO(&context.logger, "gen(take)>");
    throw ShaderComposeError("not implemented");
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
    SPDLOG_LOGGER_INFO(&context.logger, "gen(direct)> {}", source);

    context.addNew(blocks::makeBlock<blocks::Direct>(source));
    context.clearWGSLTop();
  }
};

static inline shards::Type fieldTypeToShardsType(const FieldType &type) {
  using shards::CoreInfo;
  if (type.baseType == ShaderFieldBaseType::Float32) {
    switch (type.numComponents) {
    case 1:
      return CoreInfo::FloatType;
    case 2:
      return CoreInfo::Float2Type;
    case 3:
      return CoreInfo::Float3Type;
    case 4:
      return CoreInfo::Float4Type;
    case FieldType::Float4x4Components:
      return CoreInfo::Float4x4Type;
    default:
      throw std::out_of_range(NAMEOF(FieldType::numComponents).str());
    }
  } else {
    switch (type.numComponents) {
    case 1:
      return CoreInfo::IntType;
    case 2:
      return CoreInfo::Int2Type;
    case 3:
      return CoreInfo::Int3Type;
    case 4:
      return CoreInfo::Int4Type;
    default:
      throw std::out_of_range(NAMEOF(FieldType::numComponents).str());
    }
  }
}

struct IOBase {
  std::string _name;
  FieldType _fieldType;

  static inline shards::Parameters params = {
      {"Name", SHCCSTR("The name of the field to read/write"), {CoreInfo::StringType}},
      {"Type", SHCCSTR("The expected type (default: Float32)"), {Types::ShaderFieldBaseType}},
      {"Dimension",
       SHCCSTR("The expected dimension of the type. 1 for scalars. 2,3,4 for vectors. (default: 1)"),
       {CoreInfo::IntType}},
  };

  SHParametersInfo parameters() { return params; };

  void setParam(int index, const SHVar &value) {
    using shards::Var;
    switch (index) {
    case 0:
      this->_name = value.payload.stringValue;
      break;
    case 1:
      _fieldType.baseType = ShaderFieldBaseType(value.payload.enumValue);
      break;
    case 2:
      _fieldType.numComponents = value.payload.intValue;
      break;
    }
  }
  SHVar getParam(int index) {
    using shards::Var;
    switch (index) {
    case 0:
      return Var(_name);
    case 1:
      return Var::Enum(_fieldType.baseType, VendorId, Types::ShaderFieldBaseTypeTypeId);
    case 2:
      return Var(int(_fieldType.numComponents));
    default:
      return Var::Empty;
    }
  }

  SHVar activate(SHContext *shContext, const SHVar &input) { return SHVar{}; }
};

template <typename TShard> struct Read final : public IOBase {
  SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  SHTypesInfo outputTypes() { return fieldTypeToShardsType(_fieldType); }

  void translate(TranslationContext &context) {
    SPDLOG_LOGGER_INFO(&context.logger, "gen(read/{})> {}", NAMEOF_TYPE(TShard), _name);

    context.setWGSLTop<WGSLBlock>(_fieldType, blocks::makeBlock<TShard>(_name));
  }
};

// Override for reading a value from a named buffer
struct ReadBuffer final : public IOBase {
  std::string _bufferName = "object";

  SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  SHTypeInfo compose(const SHInstanceData &data) { return fieldTypeToShardsType(_fieldType); }

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
    if (index == 3) {
      _bufferName = value.payload.stringValue;
    }
    IOBase::setParam(index, value);
  }

  SHVar getParam(int index) {
    using shards::Var;
    if (index == 3) {
      return Var(_bufferName);
    }
    return IOBase::getParam(index);
  }

  void translate(TranslationContext &context) {
    SPDLOG_LOGGER_INFO(&context.logger, "gen(read/{})> {}.{}", NAMEOF_TYPE(blocks::ReadBuffer), _bufferName, _name);

    context.setWGSLTop<WGSLBlock>(_fieldType, blocks::makeBlock<blocks::ReadBuffer>(_name, _fieldType, _bufferName));
  }
};

template <typename TShard> struct Write : public IOBase {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }

  void translate(TranslationContext &context) {
    SPDLOG_LOGGER_INFO(&context.logger, "gen(write/{})> {}", NAMEOF_TYPE(TShard), _name);

    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Can not write: value is required"));

    std::unique_ptr<IWGSLGenerated> wgslValue = context.takeWGSLTop();
    FieldType fieldType = wgslValue->getType();

    context.addNew(blocks::makeBlock<TShard>(_name, fieldType, wgslValue->toBlock()));
  }
};

template <typename TShard> struct ToNumberTranslator {
  static constexpr const char componentNames[] = {'x', 'y', 'z', 'w'};

  static ShaderFieldBaseType getShaderBaseType(shards::NumberType numberType) {
    using shards::NumberType;
    switch (numberType) {
    case NumberType::Float32:
    case NumberType::Float64:
      return ShaderFieldBaseType::Float32;
    case NumberType::Int64:
    case NumberType::Int32:
    case NumberType::Int16:
    case NumberType::Int8:
    case NumberType::UInt8:
      return ShaderFieldBaseType::Int32;
    default:
      throw std::out_of_range(std::string(NAMEOF_TYPE(shards::NumberType)));
    }
  }

  static void translate(TShard *shard, TranslationContext &context) {
    using shards::NumberTypeLookup;
    using shards::NumberTypeTraits;
    using shards::VectorTypeLookup;
    using shards::VectorTypeTraits;

    // Already validated by shard compose
    const VectorTypeTraits *outputVectorType = shard->_outputVectorType;
    const NumberTypeTraits *outputNumberType = shard->_outputNumberType;

    FieldType fieldType = getShaderBaseType(outputNumberType->type);
    fieldType.numComponents = outputVectorType->dimension;

    FieldType unitFieldType = fieldType;
    unitFieldType.numComponents = 1;

    SPDLOG_LOGGER_INFO(&context.logger, "gen(cast/{})> ", outputVectorType->name);

    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Cast requires a value"));

    std::unique_ptr<IWGSLGenerated> wgslValue = context.takeWGSLTop();
    FieldType sourceFieldType = wgslValue->getType();

    blocks::BlockPtr sourceBlock = wgslValue->toBlock();
    std::unique_ptr<blocks::Compound> sourceComponentList;

    size_t numComponentsToCopy = std::min(fieldType.numComponents, sourceFieldType.numComponents);

    // Convert list of casted components
    // Generates dstType(x, y, z), etc.
    //  - vec4<f32>(input.x, input.y, input.z)
    sourceComponentList = blocks::makeCompoundBlock();
    for (size_t i = 0; i < numComponentsToCopy; i++) {
      bool isLast = i >= (numComponentsToCopy - 1);
      blocks::BlockPtr inner = isLast ? std::move(sourceBlock) : sourceBlock->clone();

      std::string prefix = fmt::format("{}((", getFieldWGSLTypeName(unitFieldType));
      std::string suffix = fmt::format((isLast ? ").{})" : ").{}), "), componentNames[i]);
      sourceComponentList->children.emplace_back(
          blocks::makeCompoundBlock(std::move(prefix), std::move(inner), std::move(suffix)));
    }

    blocks::BlockPtr result;
    if (fieldType.numComponents == 1) {
      result = std::move(sourceComponentList);
    } else {
      // Generate constructor for target type
      std::string prefix = fmt::format("{}(", getFieldWGSLTypeName(fieldType));
      std::unique_ptr<blocks::Compound> compound = blocks::makeCompoundBlock(std::move(prefix), std::move(sourceComponentList));

      // Append zeros to fill vector type
      for (size_t i = sourceFieldType.numComponents; i < fieldType.numComponents; i++) {
        bool isLast = i >= (fieldType.numComponents - 1);
        std::string fmt;
        fmt.reserve(16);

        // Additional seperator for between original components
        if (i == sourceFieldType.numComponents)
          fmt += std::string(", ");

        fmt += isFloatType(fieldType.baseType) ? "{:f}" : "{}";
        if (!isLast)
          fmt += ", ";

        compound->children.emplace_back(blocks::makeBlock<blocks::Direct>(fmt::format(fmt, 0.)));
      }

      compound->children.emplace_back(blocks::makeBlock<blocks::Direct>(")"));

      result = std::move(compound);
    }

    context.setWGSLTop<WGSLBlock>(fieldType, std::move(result));
  }
};

} // namespace shader
} // namespace gfx

#endif // CB_EXTRA_GFX_SHADER_CORE_BLOCKS
