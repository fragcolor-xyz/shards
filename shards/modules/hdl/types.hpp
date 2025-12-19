/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

#ifndef SHARDS_MODULE_HDL_TYPES_HPP
#define SHARDS_MODULE_HDL_TYPES_HPP

#include <cstdint>
#include <string>
#include <variant>
#include <optional>
#include <stdexcept>

namespace shards {
namespace hdl {

// Represents a bit-width type (e.g., @u8, @s16)
struct BitWidth {
  uint16_t bits;
  bool isSigned;

  BitWidth(uint16_t bits = 1, bool isSigned = false) : bits(bits), isSigned(isSigned) {}

  bool operator==(const BitWidth &other) const { return bits == other.bits && isSigned == other.isSigned; }
  bool operator!=(const BitWidth &other) const { return !(*this == other); }
};

// Represents a fixed-point type (e.g., @fixed(8 8))
struct FixedPoint {
  uint16_t integerBits;
  uint16_t fractionalBits;
  bool isSigned;

  FixedPoint(uint16_t intBits = 16, uint16_t fracBits = 16, bool isSigned = true)
      : integerBits(intBits), fractionalBits(fracBits), isSigned(isSigned) {}

  uint16_t totalBits() const { return integerBits + fractionalBits; }

  bool operator==(const FixedPoint &other) const {
    return integerBits == other.integerBits && fractionalBits == other.fractionalBits && isSigned == other.isSigned;
  }
  bool operator!=(const FixedPoint &other) const { return !(*this == other); }
};

// The HDL type system - distinct from shader types
using Type = std::variant<BitWidth, FixedPoint>;

// Common type aliases
inline BitWidth U1() { return BitWidth(1, false); }
inline BitWidth U8() { return BitWidth(8, false); }
inline BitWidth U16() { return BitWidth(16, false); }
inline BitWidth U32() { return BitWidth(32, false); }
inline BitWidth U64() { return BitWidth(64, false); }
inline BitWidth S8() { return BitWidth(8, true); }
inline BitWidth S16() { return BitWidth(16, true); }
inline BitWidth S32() { return BitWidth(32, true); }
inline BitWidth S64() { return BitWidth(64, true); }
inline BitWidth UN(uint16_t n) { return BitWidth(n, false); }
inline BitWidth SN(uint16_t n) { return BitWidth(n, true); }

// Get total bit width of a type
uint16_t getBitWidth(const Type &type);

// Check if type is signed
bool isSigned(const Type &type);

// Format type as Verilog declaration (without name)
// e.g., "wire [7:0]", "wire signed [15:0]", "reg [31:0]"
std::string toVerilogType(const Type &type, bool isReg = false);

// Format type as Verilog port/signal declaration
// e.g., "input [7:0] a", "output reg [31:0] sum"
std::string toVerilogDecl(const Type &type, const std::string &name, bool isInput, bool isReg = false);

// Format type as Verilog wire/reg declaration
// e.g., "wire [7:0] temp;", "reg signed [15:0] counter;"
std::string toVerilogSignalDecl(const Type &type, const std::string &name, bool isReg = false);

// Get type name for error messages/debugging
std::string typeName(const Type &type);

// Type compatibility - can valueType be assigned to targetType?
bool isCompatible(const Type &targetType, const Type &valueType);

// Compute result type for binary operations
// Returns nullopt if types are incompatible
std::optional<Type> binaryOpResultType(const Type &lhs, const Type &rhs, char op);

// Type error
struct TypeError : public std::runtime_error {
  TypeError(const std::string &msg) : std::runtime_error(msg) {}
};

} // namespace hdl
} // namespace shards

#endif // SHARDS_MODULE_HDL_TYPES_HPP
