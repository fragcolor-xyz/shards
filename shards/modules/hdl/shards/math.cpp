/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

// Translation handlers for existing Math shards - NOT new shards!
// These handlers translate Math.Add, Math.Sub, etc. to Verilog

#include "../hdl.hpp"
#include <shards/core/shared.hpp>

namespace shards {
namespace hdl {

// Helper to resolve operand from parameter (literal or variable reference)
inline std::pair<BlockPtr, Type> resolveOperand(SHVar operandVar, HDLContext &context, const std::optional<Type> &lhsType) {
  BlockPtr rhs;
  Type rhsType;

  if (operandVar.valueType == SHType::Int) {
    // Literal integer
    rhsType = lhsType.value_or(U32());
    rhs = makeLiteral(operandVar.payload.intValue, rhsType);
  } else if (operandVar.valueType == SHType::ContextVar) {
    // Variable reference - look up the signal
    std::string name = operandVar.payload.stringValue;
    auto signalOpt = context.findSignal(name);
    if (!signalOpt) {
      throw HDLError(fmt::format("Signal '{}' not found", name));
    }
    rhsType = signalOpt->type;
    rhs = makeSignalRef(name, rhsType);
  } else {
    throw HDLError("Operand must be an integer or variable reference for HDL synthesis");
  }

  return {std::move(rhs), rhsType};
}

// Handler for Math.Add - translates existing shard to Verilog
struct MathAddHandler : public IHDLHandler {
  void translate(Shard *shard, HDLContext &context) override {
    SHVar operandVar = shard->getParam(shard, 0);

    if (!context.hasCurrentValue()) {
      throw HDLError("Math.Add requires an input value");
    }

    auto lhsType = context.getCurrentType();
    auto lhs = context.takeCurrentValue();

    auto [rhs, rhsType] = resolveOperand(operandVar, context, lhsType);

    auto resultType = binaryOpResultType(lhsType.value_or(U32()), rhsType, '+');
    if (!resultType) {
      throw HDLError("Incompatible types for Math.Add");
    }

    auto expr = makeBinaryExpr(BinaryOp::Add, std::move(lhs), std::move(rhs), *resultType);
    context.setCurrentValue(std::move(expr), *resultType);
  }
};

// Handler for Math.Subtract
struct MathSubtractHandler : public IHDLHandler {
  void translate(Shard *shard, HDLContext &context) override {
    SHVar operandVar = shard->getParam(shard, 0);

    if (!context.hasCurrentValue()) {
      throw HDLError("Math.Subtract requires an input value");
    }

    auto lhsType = context.getCurrentType();
    auto lhs = context.takeCurrentValue();

    auto [rhs, rhsType] = resolveOperand(operandVar, context, lhsType);

    auto resultType = binaryOpResultType(lhsType.value_or(U32()), rhsType, '-');
    if (!resultType) {
      throw HDLError("Incompatible types for Math.Subtract");
    }

    auto expr = makeBinaryExpr(BinaryOp::Sub, std::move(lhs), std::move(rhs), *resultType);
    context.setCurrentValue(std::move(expr), *resultType);
  }
};

// Handler for Math.Multiply
struct MathMultiplyHandler : public IHDLHandler {
  void translate(Shard *shard, HDLContext &context) override {
    SHVar operandVar = shard->getParam(shard, 0);

    if (!context.hasCurrentValue()) {
      throw HDLError("Math.Multiply requires an input value");
    }

    auto lhsType = context.getCurrentType();
    auto lhs = context.takeCurrentValue();

    auto [rhs, rhsType] = resolveOperand(operandVar, context, lhsType);

    auto resultType = binaryOpResultType(lhsType.value_or(U32()), rhsType, '*');
    if (!resultType) {
      throw HDLError("Incompatible types for Math.Multiply");
    }

    auto expr = makeBinaryExpr(BinaryOp::Mul, std::move(lhs), std::move(rhs), *resultType);
    context.setCurrentValue(std::move(expr), *resultType);
  }
};

// Handler for Math.And (bitwise)
struct MathAndHandler : public IHDLHandler {
  void translate(Shard *shard, HDLContext &context) override {
    SHVar operandVar = shard->getParam(shard, 0);

    if (!context.hasCurrentValue()) {
      throw HDLError("Math.And requires an input value");
    }

    auto lhsType = context.getCurrentType();
    auto lhs = context.takeCurrentValue();

    auto [rhs, rhsType] = resolveOperand(operandVar, context, lhsType);

    auto resultType = binaryOpResultType(lhsType.value_or(U32()), rhsType, '&');
    if (!resultType) {
      throw HDLError("Incompatible types for Math.And");
    }

    auto expr = makeBinaryExpr(BinaryOp::And, std::move(lhs), std::move(rhs), *resultType);
    context.setCurrentValue(std::move(expr), *resultType);
  }
};

// Handler for Math.Or (bitwise)
struct MathOrHandler : public IHDLHandler {
  void translate(Shard *shard, HDLContext &context) override {
    SHVar operandVar = shard->getParam(shard, 0);

    if (!context.hasCurrentValue()) {
      throw HDLError("Math.Or requires an input value");
    }

    auto lhsType = context.getCurrentType();
    auto lhs = context.takeCurrentValue();

    auto [rhs, rhsType] = resolveOperand(operandVar, context, lhsType);

    auto resultType = binaryOpResultType(lhsType.value_or(U32()), rhsType, '|');
    if (!resultType) {
      throw HDLError("Incompatible types for Math.Or");
    }

    auto expr = makeBinaryExpr(BinaryOp::Or, std::move(lhs), std::move(rhs), *resultType);
    context.setCurrentValue(std::move(expr), *resultType);
  }
};

// Handler for Math.Xor (bitwise)
struct MathXorHandler : public IHDLHandler {
  void translate(Shard *shard, HDLContext &context) override {
    SHVar operandVar = shard->getParam(shard, 0);

    if (!context.hasCurrentValue()) {
      throw HDLError("Math.Xor requires an input value");
    }

    auto lhsType = context.getCurrentType();
    auto lhs = context.takeCurrentValue();

    auto [rhs, rhsType] = resolveOperand(operandVar, context, lhsType);

    auto resultType = binaryOpResultType(lhsType.value_or(U32()), rhsType, '^');
    if (!resultType) {
      throw HDLError("Incompatible types for Math.Xor");
    }

    auto expr = makeBinaryExpr(BinaryOp::Xor, std::move(lhs), std::move(rhs), *resultType);
    context.setCurrentValue(std::move(expr), *resultType);
  }
};

// Static handler instances
static MathAddHandler mathAddHandler;
static MathSubtractHandler mathSubtractHandler;
static MathMultiplyHandler mathMultiplyHandler;
static MathAndHandler mathAndHandler;
static MathOrHandler mathOrHandler;
static MathXorHandler mathXorHandler;

void registerHDLMathShards() {
  auto &registry = getHDLRegistry();

  // Register handlers for EXISTING shards - not new ones!
  registry.registerHandler("Math.Add", &mathAddHandler);
  registry.registerHandler("Math.Subtract", &mathSubtractHandler);
  registry.registerHandler("Math.Multiply", &mathMultiplyHandler);
  registry.registerHandler("Math.And", &mathAndHandler);
  registry.registerHandler("Math.Or", &mathOrHandler);
  registry.registerHandler("Math.Xor", &mathXorHandler);
}

} // namespace hdl
} // namespace shards
