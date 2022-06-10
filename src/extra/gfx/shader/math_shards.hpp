#ifndef SH_EXTRA_GFX_SHADER_MATH_BLOCKS
#define SH_EXTRA_GFX_SHADER_MATH_BLOCKS

// Required before shard headers
#include "../shards_types.hpp"
#include "magic_enum.hpp"
#include "shards/shared.hpp"
#include "translator.hpp"
#include "translator_utils.hpp"
#include <gfx/shader/fmt.hpp>

#include "shards/core.hpp"
#include "shards/math.hpp"
#include <nameof.hpp>

namespace gfx {
namespace shader {
SH_HAS_MEMBER_TEST(validateTypes);
SH_HAS_MEMBER_TEST(call);
SH_HAS_MEMBER_TEST(op);
SH_HAS_MEMBER_TEST(prefixOp);
SH_HAS_MEMBER_TEST(postfixOp);

template <typename TShard, typename TOp> struct BinaryOperatorTranslator {
  static void translate(TShard *shard, TranslationContext &context) {
    SPDLOG_LOGGER_INFO(&context.logger, "gen(bop)>");

    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Can not apply binary operator without input"));

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

    FieldType resultType;
    if constexpr (has_validateTypes<TOp>::value) {
      resultType = TOp::validateTypes(typeA, typeB);
    } else {
      resultType = typeA;
      if (typeA != typeB)
        throw ShaderComposeError(fmt::format("Incompatible operand types left:{}, right:{}", typeA, typeB));
    }

    if constexpr (has_call<TOp>::value) {
      // generate `call(A, B)`
      context.setWGSLTop<WGSLBlock>(
          resultType, blocks::makeCompoundBlock(TOp::call, "(", operandA->toBlock(), ", ", operandB->toBlock(), ")"));
    } else if constexpr (has_op<TOp>::value) {
      // generate `A op B`
      context.setWGSLTop<WGSLBlock>(resultType, blocks::makeCompoundBlock(operandA->toBlock(), TOp::op, operandB->toBlock()));
    } else {
      throw std::logic_error("Operator implementation needs to be a call or an operator");
    }
  }
};

template <typename TShard, typename TOp> struct UnaryOperatorTranslator {
  static void translate(TShard *shard, TranslationContext &context) {
    SPDLOG_LOGGER_INFO(&context.logger, "gen(unop)>");

    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Can not apply unary operator without input"));

    std::unique_ptr<IWGSLGenerated> operandA = context.takeWGSLTop();
    FieldType typeA = operandA->getType();

    FieldType resultType;
    if constexpr (has_validateTypes<TOp>::value) {
      resultType = TOp::validateTypes(typeA);
    } else {
      resultType = typeA;
    }

    if constexpr (has_call<TOp>::value) {
      // generate `call(A)`
      context.setWGSLTop<WGSLBlock>(resultType, blocks::makeCompoundBlock(TOp::call, "(", operandA->toBlock(), ")"));
    } else if constexpr (has_prefixOp<TOp>::value) {
      // generate `op A`
      context.setWGSLTop<WGSLBlock>(resultType, blocks::makeCompoundBlock(TOp::op, operandA->toBlock()));
    } else if constexpr (has_postfixOp<TOp>::value) {
      // generate `A op`
      context.setWGSLTop<WGSLBlock>(resultType, blocks::makeCompoundBlock(operandA->toBlock(), TOp::op));
    } else {
      throw std::logic_error("Operator implementation needs to be a call or an operator");
    }
  }
};

struct OperatorAdd {
  static inline const char *op = "+";
};

struct OperatorSubtract {
  static inline const char *op = "-";
};

struct OperatorMultiply {
  static inline const char *op = "*";
  static FieldType validateTypes(FieldType a, FieldType b) {
    if (a.baseType == b.baseType) {
      if (a.numComponents == 1 || b.numComponents == 1) {
        FieldType vecType = a.numComponents == 1 ? b : a;
        return vecType;
      } else if (a.numComponents == b.numComponents) {
        return a;
      }
    }

    throw ShaderComposeError(fmt::format("Operand mismatch lhs != rhs, left:{}, right:{}", a, b));
  }
};

struct OperatorDivide {
  static inline const char *op = "/";
};

struct OperatorMod {
  static inline const char *op = "%";
};

struct OperatorCos {
  static inline const char *call = "cos";
};

struct OperatorSin {
  static inline const char *call = "sin";
};

struct OperatorTan {
  static inline const char *call = "tan";
};

struct OperatorMax {
  static inline const char *call = "max";
};

struct OperatorMin {
  static inline const char *call = "min";
};

struct OperatorPow {
  static inline const char *call = "pow";
};

struct OperatorLog {
  static inline const char *call = "log";
};

struct OperatorExp {
  static inline const char *call = "exp";
};

struct OperatorFloor {
  static inline const char *call = "floor";
};

struct OperatorCeil {
  static inline const char *call = "ceil";
};

struct OperatorRound {
  static inline const char *call = "round";
};

} // namespace shader
} // namespace gfx

#endif // SH_EXTRA_GFX_SHADER_MATH_BLOCKS
