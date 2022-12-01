#include "linalg_shards.hpp"
#include "translate_wrapper.hpp"

namespace gfx::shader {

namespace LinAlg {
using namespace shards::Math::LinAlg;
}
void registerLinalgShards() {
  // Linalg blocks
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.MatMul", LinAlg::MatMul, OperatorMatMul);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.Dot", LinAlg::Dot, OperatorDot);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.Cross", LinAlg::Cross, OperatorCross);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Normalize", LinAlg::Normalize, OperatorNormalize);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Length", LinAlg::Length, OperatorLength);
}
} // namespace gfx::shader