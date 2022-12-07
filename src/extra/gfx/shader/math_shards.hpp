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
SH_HAS_MEMBER_TEST(generate);
SH_HAS_MEMBER_TEST(call);
SH_HAS_MEMBER_TEST(op);
SH_HAS_MEMBER_TEST(prefixOp);
SH_HAS_MEMBER_TEST(postfixOp);

template <typename TShard, typename TOp> struct BinaryOperatorTranslator {
  static void translate(TShard *shard, TranslationContext &context) {
    SPDLOG_LOGGER_INFO(context.logger, "gen(bop)>");

    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Can not apply binary operator without input"));

    std::unique_ptr<IWGSLGenerated> operandA = context.takeWGSLTop();
    std::unique_ptr<IWGSLGenerated> operandB;

    SHVar varB = shard->_operand;
    if (varB.valueType == SHType::ContextVar) {
      operandB = std::make_unique<WGSLBlock>(context.reference(varB.payload.stringValue));
    } else {
      operandB = translateConst(varB, context);
    }

    FieldType typeA = operandA->getType();
    FieldType typeB = operandB->getType();

    if constexpr (has_generate<TOp>::value) {
      TOp::generate(std::move(operandA), std::move(operandB), context);
    } else {
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
        // store in temp var to avoid precedence issues
        context.setWGSLTopVar(resultType, blocks::makeCompoundBlock(operandA->toBlock(), TOp::op, operandB->toBlock()));
      } else {
        throw std::logic_error("Operator implementation needs to be a call or an operator");
      }
    }
  }
};

template <typename TShard, typename TOp> struct UnaryOperatorTranslator {
  static void translate(TShard *shard, TranslationContext &context) {
    SPDLOG_LOGGER_INFO(context.logger, "gen(unop)>");

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

inline FieldType validateTypesVectorBroadcast(FieldType a, FieldType b) {
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

struct OperatorAdd {
  static inline const char *op = "+";
  static inline FieldType validateTypes(FieldType a, FieldType b) { return validateTypesVectorBroadcast(a, b); }
};

struct OperatorSubtract {
  static inline const char *op = "-";
  static inline FieldType validateTypes(FieldType a, FieldType b) { return validateTypesVectorBroadcast(a, b); }
};

struct OperatorMultiply {
  static inline const char *op = "*";
  static inline FieldType validateTypes(FieldType a, FieldType b) { return validateTypesVectorBroadcast(a, b); }
};

struct OperatorDivide {
  static inline const char *op = "/";
  static inline FieldType validateTypes(FieldType a, FieldType b) { return validateTypesVectorBroadcast(a, b); }
};

struct OperatorMod {
  static inline const char *op = "%";
  static inline FieldType validateTypes(FieldType a, FieldType b) { return validateTypesVectorBroadcast(a, b); }
};

template <const char *Op>
inline void generateShift(std::unique_ptr<IWGSLGenerated> &&a, std::unique_ptr<IWGSLGenerated> &&b, TranslationContext &context) {
  auto typeA = a->getType();
  auto typeB = b->getType();
  std::string prefix;
  std::string suffix;

  // https://www.w3.org/TR/WGSL/#bit-expr
  // If a is a scalar, b needs to be u32
  // If a is a vector, b needs to be vec<u32> of that same dimension
  if (typeB.numComponents != typeA.numComponents) {
    prefix = fmt::format("{}(u32(", getFieldWGSLTypeName(FieldType(ShaderFieldBaseType::UInt32, typeA.numComponents)));
    suffix = "))";
  } else {
    prefix = "u32(";
    suffix = ")";
  }

  context.setWGSLTop<WGSLBlock>(a->getType(),
                                blocks::makeCompoundBlock("(", a->toBlock(), Op, prefix, b->toBlock(), suffix, ")"));
}

struct OperatorLShift {
  static inline void generate(std::unique_ptr<IWGSLGenerated> &&a, std::unique_ptr<IWGSLGenerated> &&b,
                              TranslationContext &context) {
    static const char op[] = "<<";
    generateShift<op>(std::move(a), std::move(b), context);
  }
};

struct OperatorRShift {
  static inline void generate(std::unique_ptr<IWGSLGenerated> &&a, std::unique_ptr<IWGSLGenerated> &&b,
                              TranslationContext &context) {
    static const char op[] = ">>";
    generateShift<op>(std::move(a), std::move(b), context);
  }
};

struct OperatorXor {
  static inline const char *op = "^";
  static inline FieldType validateTypes(FieldType a, FieldType b) { return validateTypesVectorBroadcast(a, b); }
};

struct OperatorAnd {
  static inline const char *op = "&";
  static inline FieldType validateTypes(FieldType a, FieldType b) { return validateTypesVectorBroadcast(a, b); }
};

struct OperatorOr {
  static inline const char *op = "|";
  static inline FieldType validateTypes(FieldType a, FieldType b) { return validateTypesVectorBroadcast(a, b); }
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

struct OperatorNegate {
  static inline bool prefixOp = true;
  static inline const char *op = "-";
};

struct OperatorAbs {
  static inline const char *call = "abs";
};

inline FieldType validateTypesComparison(FieldType a, FieldType b) {
  if (a != b)
    throw ShaderComposeError(fmt::format("Invalid types to compare: {} & {}", a, b));
  return FieldTypes::Bool;
}

struct OperatorIs {
  static inline const char *op = "==";
  static inline FieldType validateTypes(FieldType a, FieldType b) { return validateTypesComparison(a, b); }
};

struct OperatorIsNot {
  static inline const char *op = "!=";
  static inline FieldType validateTypes(FieldType a, FieldType b) { return validateTypesComparison(a, b); }
};

struct OperatorIsMore {
  static inline const char *op = ">";
  static inline FieldType validateTypes(FieldType a, FieldType b) { return validateTypesComparison(a, b); }
};

struct OperatorIsLess {
  static inline const char *op = "<";
  static inline FieldType validateTypes(FieldType a, FieldType b) { return validateTypesComparison(a, b); }
};

struct OperatorIsMoreEqual {
  static inline const char *op = ">=";
  static inline FieldType validateTypes(FieldType a, FieldType b) { return validateTypesComparison(a, b); }
};

struct OperatorIsLessEqual {
  static inline const char *op = "<=";
  static inline FieldType validateTypes(FieldType a, FieldType b) { return validateTypesComparison(a, b); }
};

} // namespace shader
} // namespace gfx

#endif // SH_EXTRA_GFX_SHADER_MATH_BLOCKS
