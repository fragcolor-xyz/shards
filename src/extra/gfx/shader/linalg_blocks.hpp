// Required before shard headers
#include "shards/shared.hpp"
#include "translator.hpp"
#include "translator_utils.hpp"
#include <nameof.hpp>

#include "shards/linalg.hpp"

namespace gfx {
namespace shader {
using shards::CoreInfo;
using shards::Math::LinAlg::MatMul;

// https://www.w3.org/TR/WGSL/#arithmetic-expr
// Matrix multiplications are implemented using the * operator in WGSL
struct MatMulTranslator {
  static void translate(MatMul *shard, TranslationContext &context) {
    SPDLOG_LOGGER_INFO(&context.logger, "gen(mmul)>");

    if (!context.wgslTop)
      throw ShaderComposeError(fmt::format("Can not apply matrix multiply without input"));

    std::unique_ptr<IWGSLGenerated> operandA = context.takeWGSLTop();
    std::unique_ptr<IWGSLGenerated> operandB;

    SHVar varB = shard->_operand;
    if (varB.valueType == SHType::ContextVar) {
      operandB = referenceGlobal(varB.payload.stringValue, context);
    } else {
      operandB = translateConst(varB, context);
    }

    // generate `A op B`
    context.setWGSLTop<WGSLBlock>(operandA->getType(), blocks::makeCompoundBlock(operandA->toBlock(), "*", operandB->toBlock()));
  }
};

} // namespace shader
} // namespace gfx