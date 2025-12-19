/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

#include "types.hpp"
#include <spdlog/fmt/fmt.h>
#include <algorithm>

namespace shards {
namespace hdl {

uint16_t getBitWidth(const Type &type) {
  return std::visit(
      [](auto &&arg) -> uint16_t {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, BitWidth>) {
          return arg.bits;
        } else if constexpr (std::is_same_v<T, FixedPoint>) {
          return arg.totalBits();
        }
      },
      type);
}

bool isSigned(const Type &type) {
  return std::visit(
      [](auto &&arg) -> bool {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, BitWidth>) {
          return arg.isSigned;
        } else if constexpr (std::is_same_v<T, FixedPoint>) {
          return arg.isSigned;
        }
      },
      type);
}

std::string toVerilogType(const Type &type, bool isReg) {
  uint16_t bits = getBitWidth(type);
  bool signedType = isSigned(type);

  std::string kind = isReg ? "reg" : "wire";
  std::string signStr = signedType ? "signed " : "";

  if (bits == 1) {
    return fmt::format("{}{}", kind, signedType ? " signed" : "");
  } else {
    return fmt::format("{} {}[{}:0]", kind, signStr, bits - 1);
  }
}

std::string toVerilogDecl(const Type &type, const std::string &name, bool isInput, bool isReg) {
  uint16_t bits = getBitWidth(type);
  bool signedType = isSigned(type);

  std::string direction = isInput ? "input" : "output";
  std::string regStr = (!isInput && isReg) ? " reg" : "";
  std::string signStr = signedType ? " signed" : "";

  if (bits == 1) {
    return fmt::format("{}{}{} {}", direction, regStr, signStr, name);
  } else {
    return fmt::format("{}{}{} [{}:0] {}", direction, regStr, signStr, bits - 1, name);
  }
}

std::string toVerilogSignalDecl(const Type &type, const std::string &name, bool isReg) {
  return fmt::format("{} {};", toVerilogType(type, isReg), name);
}

std::string typeName(const Type &type) {
  return std::visit(
      [](auto &&arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, BitWidth>) {
          return fmt::format("@{}{}",  arg.isSigned ? "s" : "u", arg.bits);
        } else if constexpr (std::is_same_v<T, FixedPoint>) {
          return fmt::format("@fixed({} {}){}", arg.integerBits, arg.fractionalBits, arg.isSigned ? "" : " unsigned");
        }
      },
      type);
}

bool isCompatible(const Type &targetType, const Type &valueType) {
  // For now, types must match exactly
  // Later: could allow implicit widening, signed/unsigned conversion with warnings
  return targetType == valueType;
}

std::optional<Type> binaryOpResultType(const Type &lhs, const Type &rhs, char op) {
  // Get bit widths
  uint16_t lhsBits = getBitWidth(lhs);
  uint16_t rhsBits = getBitWidth(rhs);
  bool lhsSigned = isSigned(lhs);
  bool rhsSigned = isSigned(rhs);

  // For arithmetic ops, result width depends on operation
  switch (op) {
  case '+':
  case '-': {
    // Addition/subtraction: max width + 1 for carry
    uint16_t resultBits = std::max(lhsBits, rhsBits) + 1;
    bool resultSigned = lhsSigned || rhsSigned;
    return BitWidth(resultBits, resultSigned);
  }
  case '*': {
    // Multiplication: sum of widths
    uint16_t resultBits = lhsBits + rhsBits;
    bool resultSigned = lhsSigned || rhsSigned;
    return BitWidth(resultBits, resultSigned);
  }
  case '&':
  case '|':
  case '^': {
    // Bitwise ops: max width, preserve signedness
    uint16_t resultBits = std::max(lhsBits, rhsBits);
    bool resultSigned = lhsSigned && rhsSigned; // Only signed if both are signed
    return BitWidth(resultBits, resultSigned);
  }
  case '/':
  case '%': {
    // Division: width of dividend
    bool resultSigned = lhsSigned || rhsSigned;
    return BitWidth(lhsBits, resultSigned);
  }
  default:
    return std::nullopt;
  }
}

} // namespace hdl
} // namespace shards
