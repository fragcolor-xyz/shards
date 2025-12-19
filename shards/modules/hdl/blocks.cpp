/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

#include "blocks.hpp"

namespace shards {
namespace hdl {

const char *binaryOpSymbol(BinaryOp op) {
  switch (op) {
  case BinaryOp::Add:
    return "+";
  case BinaryOp::Sub:
    return "-";
  case BinaryOp::Mul:
    return "*";
  case BinaryOp::Div:
    return "/";
  case BinaryOp::Mod:
    return "%";
  case BinaryOp::And:
    return "&";
  case BinaryOp::Or:
    return "|";
  case BinaryOp::Xor:
    return "^";
  case BinaryOp::LShift:
    return "<<";
  case BinaryOp::RShift:
    return ">>";
  case BinaryOp::Eq:
    return "==";
  case BinaryOp::Neq:
    return "!=";
  case BinaryOp::Lt:
    return "<";
  case BinaryOp::Gt:
    return ">";
  case BinaryOp::Lte:
    return "<=";
  case BinaryOp::Gte:
    return ">=";
  default:
    return "?";
  }
}

const char *unaryOpSymbol(UnaryOp op) {
  switch (op) {
  case UnaryOp::Not:
    return "~";
  case UnaryOp::Neg:
    return "-";
  case UnaryOp::And:
    return "&";
  case UnaryOp::Or:
    return "|";
  case UnaryOp::Xor:
    return "^";
  default:
    return "?";
  }
}

} // namespace hdl
} // namespace shards
