/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

#ifndef SHARDS_MODULE_HDL_BLOCKS_HPP
#define SHARDS_MODULE_HDL_BLOCKS_HPP

#include "types.hpp"
#include <memory>
#include <string>
#include <vector>

namespace shards {
namespace hdl {

// Forward declarations
struct Block;
using BlockPtr = std::unique_ptr<Block>;

// Port direction
enum class PortDirection { Input, Output };

// Binary operators
enum class BinaryOp {
  Add,      // +
  Sub,      // -
  Mul,      // *
  Div,      // /
  Mod,      // %
  And,      // &
  Or,       // |
  Xor,      // ^
  LShift,   // <<
  RShift,   // >>
  Eq,       // ==
  Neq,      // !=
  Lt,       // <
  Gt,       // >
  Lte,      // <=
  Gte,      // >=
};

// Unary operators
enum class UnaryOp {
  Not,      // ~
  Neg,      // -
  And,      // &x (reduction)
  Or,       // |x (reduction)
  Xor,      // ^x (reduction)
};

// Get operator symbol for Verilog output
const char *binaryOpSymbol(BinaryOp op);
const char *unaryOpSymbol(UnaryOp op);

// Base class for all IR blocks
struct Block {
  virtual ~Block() = default;
  virtual BlockPtr clone() const = 0;
};

// Represents a signal/variable reference
struct SignalRef : public Block {
  std::string name;
  Type type;

  SignalRef(const std::string &name, const Type &type) : name(name), type(type) {}
  BlockPtr clone() const override { return std::make_unique<SignalRef>(name, type); }
};

// Represents a literal value
struct Literal : public Block {
  int64_t value;
  Type type;

  Literal(int64_t value, const Type &type) : value(value), type(type) {}
  BlockPtr clone() const override { return std::make_unique<Literal>(value, type); }
};

// Represents a binary operation
struct BinaryExpr : public Block {
  BinaryOp op;
  BlockPtr lhs;
  BlockPtr rhs;
  Type resultType;

  BinaryExpr(BinaryOp op, BlockPtr lhs, BlockPtr rhs, const Type &resultType)
      : op(op), lhs(std::move(lhs)), rhs(std::move(rhs)), resultType(resultType) {}

  BlockPtr clone() const override { return std::make_unique<BinaryExpr>(op, lhs->clone(), rhs->clone(), resultType); }
};

// Represents a unary operation
struct UnaryExpr : public Block {
  UnaryOp op;
  BlockPtr operand;
  Type resultType;

  UnaryExpr(UnaryOp op, BlockPtr operand, const Type &resultType)
      : op(op), operand(std::move(operand)), resultType(resultType) {}

  BlockPtr clone() const override { return std::make_unique<UnaryExpr>(op, operand->clone(), resultType); }
};

// Represents bit slicing [high:low]
struct Slice : public Block {
  BlockPtr signal;
  uint16_t high;
  uint16_t low;
  Type resultType;

  Slice(BlockPtr signal, uint16_t high, uint16_t low)
      : signal(std::move(signal)), high(high), low(low), resultType(BitWidth(high - low + 1, false)) {}

  BlockPtr clone() const override { return std::make_unique<Slice>(signal->clone(), high, low); }
};

// Represents concatenation {a, b, c}
struct Concat : public Block {
  std::vector<BlockPtr> parts;
  Type resultType;

  Concat() : resultType(BitWidth(0, false)) {}
  Concat(std::vector<BlockPtr> parts, const Type &resultType) : parts(std::move(parts)), resultType(resultType) {}

  void addPart(BlockPtr part) { parts.push_back(std::move(part)); }

  BlockPtr clone() const override {
    std::vector<BlockPtr> clonedParts;
    for (const auto &p : parts) {
      clonedParts.push_back(p->clone());
    }
    return std::make_unique<Concat>(std::move(clonedParts), resultType);
  }
};

// Represents a ternary/mux: cond ? then : else
struct Ternary : public Block {
  BlockPtr condition;
  BlockPtr thenExpr;
  BlockPtr elseExpr;
  Type resultType;

  Ternary(BlockPtr cond, BlockPtr thenE, BlockPtr elseE, const Type &resultType)
      : condition(std::move(cond)), thenExpr(std::move(thenE)), elseExpr(std::move(elseE)), resultType(resultType) {}

  BlockPtr clone() const override {
    return std::make_unique<Ternary>(condition->clone(), thenExpr->clone(), elseExpr->clone(), resultType);
  }
};

// Represents a port declaration
struct Port {
  std::string name;
  Type type;
  PortDirection direction;

  Port(const std::string &name, const Type &type, PortDirection dir) : name(name), type(type), direction(dir) {}
};

// Represents an internal wire/signal declaration
struct Wire {
  std::string name;
  Type type;
  BlockPtr initializer; // Optional: assign value

  Wire(const std::string &name, const Type &type, BlockPtr init = nullptr)
      : name(name), type(type), initializer(std::move(init)) {}
};

// Represents a continuous assignment: assign x = expr;
struct Assign {
  std::string target;
  BlockPtr value;

  Assign(const std::string &target, BlockPtr value) : target(target), value(std::move(value)) {}
};

// Represents a complete module
struct Module {
  std::string name;
  std::vector<Port> ports;
  std::vector<Wire> wires;
  std::vector<Assign> assigns;

  Module(const std::string &name) : name(name) {}

  void addPort(const std::string &name, const Type &type, PortDirection dir) { ports.emplace_back(name, type, dir); }

  void addWire(const std::string &name, const Type &type, BlockPtr init = nullptr) {
    wires.emplace_back(name, type, std::move(init));
  }

  void addAssign(const std::string &target, BlockPtr value) { assigns.emplace_back(target, std::move(value)); }
};

// Helper functions for creating blocks
inline BlockPtr makeSignalRef(const std::string &name, const Type &type) { return std::make_unique<SignalRef>(name, type); }

inline BlockPtr makeLiteral(int64_t value, const Type &type) { return std::make_unique<Literal>(value, type); }

inline BlockPtr makeBinaryExpr(BinaryOp op, BlockPtr lhs, BlockPtr rhs, const Type &resultType) {
  return std::make_unique<BinaryExpr>(op, std::move(lhs), std::move(rhs), resultType);
}

inline BlockPtr makeUnaryExpr(UnaryOp op, BlockPtr operand, const Type &resultType) {
  return std::make_unique<UnaryExpr>(op, std::move(operand), resultType);
}

inline BlockPtr makeTernary(BlockPtr cond, BlockPtr thenE, BlockPtr elseE, const Type &resultType) {
  return std::make_unique<Ternary>(std::move(cond), std::move(thenE), std::move(elseE), resultType);
}

} // namespace hdl
} // namespace shards

#endif // SHARDS_MODULE_HDL_BLOCKS_HPP
