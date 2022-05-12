#ifndef SH_EXTRA_GFX_SHADER_LINALG_BLOCKS
#define SH_EXTRA_GFX_SHADER_LINALG_BLOCKS

// Required before shard headers
#include "shards/shared.hpp"

#include "shards/linalg.hpp"
#include "translator.hpp"
#include "translator_utils.hpp"
#include <gfx/shader/fmt.hpp>
#include <nameof.hpp>
#include <stdexcept>

namespace gfx {
namespace shader {

// https://www.w3.org/TR/WGSL/#arithmetic-expr
// Matrix multiplications are implemented using the * operator in WGSL
struct MatMulTranslator {
  static void translate(shards::Math::LinAlg::MatMul *shard, TranslationContext &context) {
    SPDLOG_LOGGER_INFO(&context.logger, "gen(mmul)>");

    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Can not apply matrix multiply without input"));

    std::unique_ptr<IWGSLGenerated> operandA = context.takeWGSLTop();
    std::unique_ptr<IWGSLGenerated> operandB;

    // Assert lhs=float4x4
    if (operandA->getType().numComponents != FieldType::Float4x4Components) {
      throw ShaderComposeError(fmt::format("Input to MatMul should be a matrix"));
    }

    SHVar varB = shard->_operand;
    if (varB.valueType == SHType::ContextVar) {
      operandB = referenceGlobal(varB.payload.stringValue, context);
    } else {
      operandB = translateConst(varB, context);
    }

    // Assert rhs=float4x4|float4
    FieldType typeB = operandB->getType();
    if (!(typeB.numComponents == 4 || typeB.numComponents == FieldType::Float4x4Components)) {
      throw ShaderComposeError(fmt::format("Param to MatMul should be a Float4/Float4x4"));
    }

    // Use rhs type as a result since lhs will always be a float4x4
    context.setWGSLTop<WGSLBlock>(typeB, blocks::makeCompoundBlock(operandA->toBlock(), "*", operandB->toBlock()));
  }
};

struct VectorDotTranslator {
  static void translate(shards::Math::LinAlg::Dot *shard, TranslationContext &context) {
    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Can not apply Dot product without input"));

    std::unique_ptr<IWGSLGenerated> operandA = context.takeWGSLTop();
    std::unique_ptr<IWGSLGenerated> operandB;

    SHVar varB = shard->_operand;
    if (varB.valueType == SHType::ContextVar) {
      operandB = referenceGlobal(varB.payload.stringValue, context);
    } else {
      operandB = translateConst(varB, context);
    }

    FieldType typeA = operandA->getType();
    FieldType typeB = operandB->getType();
    if (typeA != typeB) {
      throw ShaderComposeError(fmt::format("Dot product operand missmatch (left:{}, right:{})", typeA, typeB));
    }

    if (isFloatType(typeA.baseType)) {
      throw ShaderComposeError(fmt::format("Dot product only supported on floating point types"));
    }

    context.setWGSLTop<WGSLBlock>(FieldType(typeA.baseType, 1),
                                  blocks::makeCompoundBlock("dot(", operandA->toBlock(), ", ", operandB->toBlock(), ")"));
  }
};

struct VectorCrossTranslator {
  static void translate(shards::Math::LinAlg::Cross *shard, TranslationContext &context) {
    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Can not apply Cross product without input"));

    std::unique_ptr<IWGSLGenerated> operandA = context.takeWGSLTop();
    std::unique_ptr<IWGSLGenerated> operandB;

    SHVar varB = shard->_operand;
    if (varB.valueType == SHType::ContextVar) {
      operandB = referenceGlobal(varB.payload.stringValue, context);
    } else {
      operandB = translateConst(varB, context);
    }

    FieldType typeA = operandA->getType();
    FieldType typeB = operandB->getType();
    if (typeA != typeB) {
      throw ShaderComposeError(fmt::format("Cross product operand missmatch (left:{}, right:{})", typeA, typeB));
    }

    if (isFloatType(typeA.baseType)) {
      throw ShaderComposeError(fmt::format("Cross product only supported on floating point types"));
    }

    context.setWGSLTop<WGSLBlock>(typeA,
                                  blocks::makeCompoundBlock("cross(", operandA->toBlock(), ", ", operandB->toBlock(), ")"));
  }
};

struct VectorNormalizeTranslator {
  static void translate(shards::Math::LinAlg::Normalize *shard, TranslationContext &context) {
    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Can not apply Normalize without input"));

    std::unique_ptr<IWGSLGenerated> operandA = context.takeWGSLTop();
    FieldType inputType = operandA->getType();
    if (isFloatType(inputType.baseType)) {
      throw ShaderComposeError(fmt::format("Normalize only supported on floating point types"));
    }

    context.setWGSLTop<WGSLBlock>(inputType, blocks::makeCompoundBlock("normalize(", operandA->toBlock(), ")"));
  }
};

struct VectorLengthTranslator {
  static void translate(shards::Math::LinAlg::Length *shard, TranslationContext &context) {
    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Can not apply Length without input"));

    std::unique_ptr<IWGSLGenerated> operandA = context.takeWGSLTop();
    FieldType inputType = operandA->getType();
    if (isFloatType(inputType.baseType)) {
      throw ShaderComposeError(fmt::format("Length only supported on floating point types"));
    }

    context.setWGSLTop<WGSLBlock>(FieldType(inputType.baseType, 1),
                                  blocks::makeCompoundBlock("length(", operandA->toBlock(), ")"));
  }
};

} // namespace shader
} // namespace gfx

#endif // SH_EXTRA_GFX_SHADER_LINALG_BLOCKS
