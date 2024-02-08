#include "math_shards.hpp"
#include "translate_wrapper.hpp"
#include "translator_utils.hpp"
#include <gfx/shader/fmt.hpp>

#include <shards/modules/core/casting.hpp>

namespace gfx::shader {

template <typename TShard> struct ToNumberTranslator {
  static void translate(TShard *shard, TranslationContext &context) {
    using shards::NumberTypeLookup;
    using shards::NumberTypeTraits;
    using shards::VectorTypeLookup;
    using shards::VectorTypeTraits;

    // Already validated by shard compose
    const VectorTypeTraits *outputVectorType = shard->_outputVectorType;
    const NumberTypeTraits *outputNumberType = shard->_outputNumberType;

    NumType fieldType = getShaderBaseType(outputNumberType->type);
    fieldType.numComponents = outputVectorType->dimension;

    NumType unitFieldType = fieldType;
    unitFieldType.numComponents = 1;

    SPDLOG_LOGGER_TRACE(context.logger, "gen(cast/{})> ", outputVectorType->name);

    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Cast requires a value"));

    std::unique_ptr<IWGSLGenerated> wgslValue = context.takeWGSLTop();
    NumType sourceFieldType = std::get<NumType>(wgslValue->getType());

    blocks::BlockPtr sourceBlock = wgslValue->toBlock();
    std::unique_ptr<blocks::Compound> sourceComponentList;

    size_t numComponentsToCopy = std::min(fieldType.numComponents, sourceFieldType.numComponents);

    bool requireSourceSwizzle = sourceFieldType.numComponents > 1;

    // Convert list of casted components
    // Generates dstType(x, y, z), etc.
    //  - vec4<f32>(input.x, input.y, input.z)
    sourceComponentList = blocks::makeCompoundBlock();
    for (size_t i = 0; i < numComponentsToCopy; i++) {
      bool isLast = i >= (numComponentsToCopy - 1);
      blocks::BlockPtr inner = isLast ? std::move(sourceBlock) : sourceBlock->clone();

      std::string prefix = fmt::format("{}((", getWGSLTypeName(unitFieldType));
      std::string suffix{};
      if (requireSourceSwizzle)
        suffix = fmt::format((isLast ? ").{})" : ").{}), "), componentNames[i]);
      else
        suffix = fmt::format((isLast ? "))" : ")), "));

      sourceComponentList->children.emplace_back(
          blocks::makeCompoundBlock(std::move(prefix), std::move(inner), std::move(suffix)));
    }

    blocks::BlockPtr result;
    if (fieldType.numComponents == 1) {
      result = std::move(sourceComponentList);
    } else {
      // Generate constructor for target type
      std::string prefix = fmt::format("{}(", getWGSLTypeName(fieldType));
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

template <typename TShard> struct MakeVectorTranslator {
  static void translate(TShard *shard, TranslationContext &context) {
    using shards::NumberTypeLookup;
    using shards::NumberTypeTraits;
    using shards::VectorTypeLookup;
    using shards::VectorTypeTraits;

    // Already validated by shard compose
    const VectorTypeTraits *outputVectorType = shard->_outputVectorType;
    const NumberTypeTraits *outputNumberType = shard->_outputNumberType;

    NumType fieldType = getShaderBaseType(outputNumberType->type);
    fieldType.numComponents = outputVectorType->dimension;

    NumType unitFieldType = fieldType;
    unitFieldType.numComponents = 1;

    SPDLOG_LOGGER_TRACE(context.logger, "gen(make/{})> ", outputVectorType->name);

    std::vector<shards::ParamVar> &params = shard->params;
    std::unique_ptr<blocks::Compound> sourceComponentList;

    if (!shard->isBroadcast && shard->inputSize != params.size())
      throw ShaderComposeError(
          fmt::format("Unsupported vector constructor, {} from {} components", outputVectorType->name, shard->inputSize));

    // Convert list of casted components
    // Generates dstType(x, y, z), etc.
    //  - vec4<f32>(input.x, input.y, input.z)
    sourceComponentList = blocks::makeCompoundBlock();
    for (size_t i = 0; i < shard->inputSize; i++) {
      bool isLast = i == (shard->inputSize - 1);

      auto value = translateParamVar(params[i], context);
      if (value->getType() != Type(unitFieldType))
        throw ShaderComposeError(fmt::format("Invalid parameter {} to MakeVector", i));

      sourceComponentList->append(value->toBlock());
      if (!isLast)
        sourceComponentList->append(", ");
    }

    // Generate constructor for target type
    std::string prefix = fmt::format("{}(", getWGSLTypeName(fieldType));
    blocks::BlockPtr result = blocks::makeCompoundBlock(std::move(prefix), std::move(sourceComponentList), ")");

    context.setWGSLTopVar(fieldType, std::move(result));
  }
};

struct LerpTranslator {
  static void translate(shards::Math::Lerp *shard, TranslationContext &context) {
    auto expr = blocks::makeCompoundBlock();
    expr->append("mix(");

    auto first = translateParamVar(shard->_first, context);
    expr->append(first->toBlock(), ", ");
    auto second = translateParamVar(shard->_second, context);
    expr->append(second->toBlock(), ", ");

    auto input = context.takeWGSLTop();
    expr->append(input->toBlock(), ")");

    context.setWGSLTopVar(first->getType(), std::move(expr));
  }
};

template <SHType ToType> using ToNumber = shards::ToNumber<ToType>;
template <SHType ToType> using MakeVector = shards::MakeVector<ToType>;

void registerMathShards() {
  // Math blocks
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.Add", shards::Math::Add, OperatorAdd);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.Subtract", shards::Math::Subtract, OperatorSubtract);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.Multiply", shards::Math::Multiply, OperatorMultiply);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.Divide", shards::Math::Divide, OperatorDivide);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.Mod", shards::Math::Mod, OperatorMod);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Cos", shards::Math::Cos, OperatorCos);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Sin", shards::Math::Sin, OperatorSin);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Tan", shards::Math::Tan, OperatorTan);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Max", shards::Math::Max, OperatorMax);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Min", shards::Math::Min, OperatorMin);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.Pow", shards::Math::Pow, OperatorPow);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.RShift", shards::Math::RShift, OperatorRShift);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.LShift", shards::Math::RShift, OperatorLShift);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.Xor", shards::Math::RShift, OperatorXor);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.And", shards::Math::RShift, OperatorAnd);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.Or", shards::Math::RShift, OperatorOr);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Log", shards::Math::Log, OperatorLog);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Exp", shards::Math::Exp, OperatorExp);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Floor", shards::Math::Floor, OperatorFloor);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Ceil", shards::Math::Ceil, OperatorCeil);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Round", shards::Math::Round, OperatorRound);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Negate", shards::Math::Negate, OperatorNegate);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Abs", shards::Math::Abs, OperatorAbs);

  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Exp2", shards::Math::Exp2, OperatorExp2);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Log2", shards::Math::Log2, OperatorLog2);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Sqrt", shards::Math::Sqrt, OperatorSqrt);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Asin", shards::Math::Asin, OperatorAsin);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Acos", shards::Math::Acos, OperatorAcos);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Atan", shards::Math::Atan, OperatorAtan);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Sinh", shards::Math::Sinh, OperatorSinh);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Cosh", shards::Math::Cosh, OperatorCosh);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Tanh", shards::Math::Tanh, OperatorTanh);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Asinh", shards::Math::Asinh, OperatorAsinh);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Acosh", shards::Math::Acosh, OperatorAcosh);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Atanh", shards::Math::Atanh, OperatorAtanh);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Trunc", shards::Math::Trunc, OperatorTrunc);

  REGISTER_EXTERNAL_SHADER_SHARD(LerpTranslator, "Math.Lerp", shards::Math::Lerp);

  // Logic operators
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Is", shards::Is, OperatorIs);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "IsNot", shards::IsNot, OperatorIsNot);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "IsMore", shards::IsMore, OperatorIsMore);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "IsLess", shards::IsLess, OperatorIsLess);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "IsMoreEqual", shards::IsMoreEqual, OperatorIsMoreEqual);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "IsLessEqual", shards::IsLessEqual, OperatorIsLessEqual);

  // Casting blocks
  REGISTER_EXTERNAL_SHADER_SHARD_T1(ToNumberTranslator, "ToInt", ToNumber<SHType::Int>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(ToNumberTranslator, "ToInt2", ToNumber<SHType::Int2>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(ToNumberTranslator, "ToInt3", ToNumber<SHType::Int3>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(ToNumberTranslator, "ToInt4", ToNumber<SHType::Int4>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(ToNumberTranslator, "ToInt8", ToNumber<SHType::Int8>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(ToNumberTranslator, "ToInt16", ToNumber<SHType::Int16>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(ToNumberTranslator, "ToColor", ToNumber<SHType::Color>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(ToNumberTranslator, "ToFloat", ToNumber<SHType::Float>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(ToNumberTranslator, "ToFloat2", ToNumber<SHType::Float2>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(ToNumberTranslator, "ToFloat3", ToNumber<SHType::Float3>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(ToNumberTranslator, "ToFloat4", ToNumber<SHType::Float4>);

  REGISTER_EXTERNAL_SHADER_SHARD_T1(MakeVectorTranslator, "MakeInt2", MakeVector<SHType::Int2>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(MakeVectorTranslator, "MakeInt3", MakeVector<SHType::Int3>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(MakeVectorTranslator, "MakeInt4", MakeVector<SHType::Int4>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(MakeVectorTranslator, "MakeInt8", MakeVector<SHType::Int8>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(MakeVectorTranslator, "MakeInt16", MakeVector<SHType::Int16>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(MakeVectorTranslator, "MakeColor", MakeVector<SHType::Color>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(MakeVectorTranslator, "MakeFloat2", MakeVector<SHType::Float2>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(MakeVectorTranslator, "MakeFloat3", MakeVector<SHType::Float3>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(MakeVectorTranslator, "MakeFloat4", MakeVector<SHType::Float4>);
}

} // namespace gfx::shader