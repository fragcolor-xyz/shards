#ifndef SH_EXTRA_GFX_SHADER_LINALG_BLOCKS
#define SH_EXTRA_GFX_SHADER_LINALG_BLOCKS

#include <shards/modules/core/linalg.hpp>
#include "math_shards.hpp"
#include <gfx/shader/fmt.hpp>
#include <spdlog/fmt/fmt.h>

namespace gfx {
namespace shader {

struct OperatorMatMul {
  static inline const char *op = "*";
  static NumType validateTypes(NumType a, NumType b) {
    if (a.matrixDimension <= 1) {
      throw ShaderComposeError(fmt::format("Left hand side should be a matrix"));
    }

    if (b.matrixDimension == 1) {
      // square mat x vec
      if (a.matrixDimension != a.numComponents || a.matrixDimension != b.numComponents) {
        throw ShaderComposeError(
            fmt::format("Incompatible matrix/vector sizes {}x{} * {}", a.numComponents, a.matrixDimension, b.numComponents));
      }
    } else {
      // square mat x mat
      if (a.matrixDimension != b.matrixDimension || a.numComponents != b.numComponents) {
        throw ShaderComposeError(fmt::format("Incompatible matrix sizes {}x{} * {}x{}", a.numComponents, a.matrixDimension,
                                             b.numComponents, b.matrixDimension));
      }
    }

    return b;
  }
};

struct OperatorDot {
  static inline const char *call = "dot";
  static NumType validateTypes(NumType a, NumType b) {
    if (a != b) {
      throw ShaderComposeError(fmt::format("Operand mismatch lhs!=rhs, left:{}, right:{}", a, b));
    }
    if (!isFloatType(a.baseType)) {
      throw ShaderComposeError(fmt::format("Dot product only supported on floating point types"));
    }
    return NumType(a.baseType, 1);
  }
};

struct OperatorCross {
  static inline const char *call = "cross";
  static NumType validateTypes(NumType a, NumType b) {
    if (a != b) {
      throw ShaderComposeError(fmt::format("Operand mismatch lhs!=rhs, left:{}, right:{}", a, b));
    }
    if (!isFloatType(a.baseType)) {
      throw ShaderComposeError(fmt::format("Dot product only supported on floating point types"));
    }
    return a;
  }
};

struct OperatorNormalize {
  static inline const char *call = "normalize";
};

struct OperatorLength {
  static inline const char *call = "length";
  static NumType validateTypes(NumType a) { return NumType(a.baseType, 1); }
};

} // namespace shader
} // namespace gfx

#endif // SH_EXTRA_GFX_SHADER_LINALG_BLOCKS
