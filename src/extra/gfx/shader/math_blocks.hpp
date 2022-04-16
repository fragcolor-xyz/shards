#pragma once

// Required before shard headers
#include "../shards_types.hpp"
#include "magic_enum.hpp"
#include "shards/shared.hpp"
#include "translator.hpp"
#include "translator_utils.hpp"

#include "shards/core.hpp"
#include "shards/math.hpp"
#include <nameof.hpp>

namespace gfx {
namespace shader {
struct OperatorAdd {
  static inline const char *wgsl = "+";
};

struct OperatorSubtract {
  static inline const char *wgsl = "-";
};

struct OperatorMultiply {
  static inline const char *wgsl = "*";
};

struct OperatorDivide {
  static inline const char *wgsl = "/";
};

struct OperatorMod {
  static inline const char *wgsl = "%";
};

// https://www.w3.org/TR/WGSL/#arithmetic-expr
// Binary operators are supported between vector/scalar types
template <typename TShard, typename TOp> struct BinaryOperatorTranslator {
  static void translate(TShard *shard, TranslationContext &context) {
    const char *operatorWgsl = TOp::wgsl;
    SPDLOG_LOGGER_INFO(&context.logger, "gen(bop)> {}", operatorWgsl);

    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Can not apply binary operator {} without input", operatorWgsl));

    std::unique_ptr<IWGSLGenerated> operandA = context.takeWGSLTop();
    std::unique_ptr<IWGSLGenerated> operandB;

    SHVar varB = shard->_operand;
    if (varB.valueType == SHType::ContextVar) {
      operandB = referenceGlobal(varB.payload.stringValue, context);
    } else {
      operandB = translateConst(varB, context);
    }

    // generate `A op B`
    context.setWGSLTop<WGSLBlock>(operandA->getType(),
                                  blocks::makeCompoundBlock(operandA->toBlock(), operatorWgsl, operandB->toBlock()));
  }
};

} // namespace shader
} // namespace gfx
