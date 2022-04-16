#pragma once

// Required before shard headers
#include "shards.h"
#include "magic_enum.hpp"
#include "runtime.hpp"
#include "shader_translator.hpp"

#include "shards/core.hpp"
#include "shards/math.hpp"
#include <shards.hpp>
#include <common_types.hpp>
#include <foundation.hpp>

namespace gfx {
namespace shader {
using shards::CoreInfo;
using shards::Math::BinaryBase;
using shards::Math::UnaryBase;

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

template <typename TShard, typename TOp> struct BinaryOperatorTranslator {
  static void translate(TShard *shard, TranslationContext &context) { context.logger.info("gen(bop)> {} + "); }
};

struct ConstTranslator {
  static void translate(shards::Const *shard, TranslationContext &context) {
    SHVarPayload &pl = shard->_value.payload;
    SHType valueType = shard->_value.valueType;
    switch (valueType) {
    case SHType::Int:
      context.logger.info("gen(const/i)> {} = ", pl.intValue);
      break;
    case SHType::Float:
      context.logger.info("gen(const/f)> {} = ", pl.floatValue);
      break;
    default:
      throw ShaderComposeError(
          fmt::format("Unsupported value type for Const inside shader: {}", magic_enum::enum_name(valueType)));
    }
  }
};

// Generates global variables
struct SetTranslator {
  static void translate(shards::Set *shard, TranslationContext &context) {
    context.logger.info("gen(set)> {} = ", shard->_name);
  }
};

struct GetTranslator {
  static void translate(shards::Get *shard, TranslationContext &context) {
    context.logger.info("gen(get)> {}", shard->_name);
  }
};

struct RefTranslator {
  static void translate(shards::Ref *shard, TranslationContext &context) {
    context.logger.info("gen(ref)> {}", shard->_name);
  }
};

struct UpdateTranslator {
  static void translate(shards::Update *shard, TranslationContext &context) {
    context.logger.info("gen(upd)> {}", shard->_name);
  }
};

// Generates vector swizzles
struct TakeTranslator {
  static void translate(shards::Take *shard, TranslationContext &context) {
    if (!shard->_vectorInputType || !shard->_vectorOutputType)
      throw ShaderComposeError(fmt::format("Take: only supports vector types inside shaders"));

    context.logger.info("gen(take)> TODO");
  }
};

struct Literal {
  SHVar value;

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHParametersInfo parameters() { return SHParametersInfo{}; }

  void setParam(int index, const SHVar &value) { this->value = value; }
  SHVar getParam(int index) { return this->value; }

  SHVar activate(SHContext *shContext, const SHVar &input) { return SHVar{}; }

  void translate(TranslationContext &context) { context.logger.info("gen> {}", value.payload.stringValue); }
};

struct Input {
  SHVar value;

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHParametersInfo parameters() { return SHParametersInfo{}; }

  void setParam(int index, const SHVar &value) { this->value = value; }
  SHVar getParam(int index) { return this->value; }

  SHVar activate(SHContext *shContext, const SHVar &input) { return SHVar{}; }

  void translate(TranslationContext &context) { context.logger.info("gen(in)> {}", value.payload.stringValue); }
};

struct Output {
  SHVar value;

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHParametersInfo parameters() { return SHParametersInfo{}; }

  void setParam(int index, const SHVar &value) { this->value = value; }
  SHVar getParam(int index) { return this->value; }

  SHVar activate(SHContext *shContext, const SHVar &input) { return SHVar{}; }

  void translate(TranslationContext &context) { context.logger.info("gen(out)> {}", value.payload.stringValue); }
};

} // namespace shader
} // namespace gfx
