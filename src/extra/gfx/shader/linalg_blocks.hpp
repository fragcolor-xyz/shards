#ifndef SH_EXTRA_GFX_SHADER_LINALG_BLOCKS
#define SH_EXTRA_GFX_SHADER_LINALG_BLOCKS

#include "shards/linalg.hpp"
#include "math_blocks.hpp"
#include <spdlog/fmt/fmt.h>

namespace gfx {
namespace shader {

struct OperatorMatMul {
  static inline const char *op = "*";
  static FieldType validateTypes(FieldType a, FieldType b) {
    if (a.numComponents != FieldType::Float4x4Components) {
      throw ShaderComposeError(fmt::format("Left hand side should be a matrix"));
    }

    if (!(b.numComponents == 4 || b.numComponents == FieldType::Float4x4Components)) {
      throw ShaderComposeError(fmt::format("Right hand side should be a Float4/Float4x4"));
    }

    return b;
  }
};

struct OperatorDot {
  static inline const char *call = "dot";
  static FieldType validateTypes(FieldType a, FieldType b) {
    if (a != b) {
      throw ShaderComposeError(fmt::format("Operand mismatch lhs!=rhs, left:{}, right:{}", a, b));
    }
    if (!isFloatType(a.baseType)) {
      throw ShaderComposeError(fmt::format("Dot product only supported on floating point types"));
    }
    return FieldType(a.baseType, 1);
  }
};

struct OperatorCross {
  static inline const char *call = "cross";
  static FieldType validateTypes(FieldType a, FieldType b) {
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
  static FieldType validateTypes(FieldType a) { return FieldType(a.baseType, 1); }
};

} // namespace shader
} // namespace gfx

#endif // SH_EXTRA_GFX_SHADER_LINALG_BLOCKS
