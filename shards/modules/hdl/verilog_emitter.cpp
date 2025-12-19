/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

#include "verilog_emitter.hpp"
#include <spdlog/fmt/fmt.h>

namespace shards {
namespace hdl {

void VerilogEmitter::indent() {
  for (int i = 0; i < _indentLevel; ++i) {
    _out << "    ";
  }
}

void VerilogEmitter::newline() { _out << "\n"; }

void VerilogEmitter::emitLine(const std::string &line) {
  indent();
  _out << line;
  newline();
}

std::string VerilogEmitter::emit(const Module &module) {
  _out.str("");
  _out.clear();
  _indentLevel = 0;

  // Module header
  _out << "module " << module.name;

  // Port list
  if (!module.ports.empty()) {
    _out << "(";
    newline();
    _indentLevel++;
    emitPorts(module.ports);
    _indentLevel--;
    indent();
    _out << ");";
  } else {
    _out << "();";
  }
  newline();
  newline();

  _indentLevel++;

  // Wire declarations
  if (!module.wires.empty()) {
    emitWires(module.wires);
    newline();
  }

  // Assignments
  if (!module.assigns.empty()) {
    emitAssigns(module.assigns);
  }

  _indentLevel--;

  // Module footer
  emitLine("endmodule");

  return _out.str();
}

void VerilogEmitter::emitPorts(const std::vector<Port> &ports) {
  for (size_t i = 0; i < ports.size(); ++i) {
    const auto &port = ports[i];
    indent();

    bool isInput = (port.direction == PortDirection::Input);
    _out << toVerilogDecl(port.type, port.name, isInput, false);

    if (i < ports.size() - 1) {
      _out << ",";
    }
    newline();
  }
}

void VerilogEmitter::emitWires(const std::vector<Wire> &wires) {
  for (const auto &wire : wires) {
    indent();
    _out << toVerilogSignalDecl(wire.type, wire.name, false);
    newline();
  }
}

void VerilogEmitter::emitAssigns(const std::vector<Assign> &assigns) {
  for (const auto &assign : assigns) {
    indent();
    _out << "assign " << assign.target << " = ";
    emitBlock(*assign.value);
    _out << ";";
    newline();
  }
}

std::string VerilogEmitter::emitExpr(const Block &block) {
  _out.str("");
  _out.clear();
  emitBlock(block);
  return _out.str();
}

void VerilogEmitter::emitBlock(const Block &block) {
  if (auto *ref = dynamic_cast<const SignalRef *>(&block)) {
    emitSignalRef(*ref);
  } else if (auto *lit = dynamic_cast<const Literal *>(&block)) {
    emitLiteral(*lit);
  } else if (auto *binary = dynamic_cast<const BinaryExpr *>(&block)) {
    emitBinaryExpr(*binary);
  } else if (auto *unary = dynamic_cast<const UnaryExpr *>(&block)) {
    emitUnaryExpr(*unary);
  } else if (auto *slice = dynamic_cast<const Slice *>(&block)) {
    emitSlice(*slice);
  } else if (auto *concat = dynamic_cast<const Concat *>(&block)) {
    emitConcat(*concat);
  } else if (auto *ternary = dynamic_cast<const Ternary *>(&block)) {
    emitTernary(*ternary);
  }
}

void VerilogEmitter::emitSignalRef(const SignalRef &ref) { _out << ref.name; }

void VerilogEmitter::emitLiteral(const Literal &lit) {
  uint16_t bits = getBitWidth(lit.type);
  bool signedVal = isSigned(lit.type);

  // Format: N'sb... for signed, N'b... for unsigned, or just N'd... for decimal
  if (lit.value >= 0) {
    if (signedVal) {
      _out << fmt::format("{}'sd{}", bits, lit.value);
    } else {
      _out << fmt::format("{}'d{}", bits, lit.value);
    }
  } else {
    // Negative value - use signed format
    _out << fmt::format("{}'sd{}", bits, lit.value);
  }
}

void VerilogEmitter::emitBinaryExpr(const BinaryExpr &expr) {
  _out << "(";
  emitBlock(*expr.lhs);
  _out << " " << binaryOpSymbol(expr.op) << " ";
  emitBlock(*expr.rhs);
  _out << ")";
}

void VerilogEmitter::emitUnaryExpr(const UnaryExpr &expr) {
  _out << unaryOpSymbol(expr.op);
  _out << "(";
  emitBlock(*expr.operand);
  _out << ")";
}

void VerilogEmitter::emitSlice(const Slice &slice) {
  emitBlock(*slice.signal);
  if (slice.high == slice.low) {
    _out << fmt::format("[{}]", slice.high);
  } else {
    _out << fmt::format("[{}:{}]", slice.high, slice.low);
  }
}

void VerilogEmitter::emitConcat(const Concat &concat) {
  _out << "{";
  for (size_t i = 0; i < concat.parts.size(); ++i) {
    emitBlock(*concat.parts[i]);
    if (i < concat.parts.size() - 1) {
      _out << ", ";
    }
  }
  _out << "}";
}

void VerilogEmitter::emitTernary(const Ternary &ternary) {
  _out << "(";
  emitBlock(*ternary.condition);
  _out << " ? ";
  emitBlock(*ternary.thenExpr);
  _out << " : ";
  emitBlock(*ternary.elseExpr);
  _out << ")";
}

std::string emitVerilog(const Module &module) {
  VerilogEmitter emitter;
  return emitter.emit(module);
}

} // namespace hdl
} // namespace shards
