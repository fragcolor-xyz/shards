/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

#ifndef SHARDS_MODULE_HDL_VERILOG_EMITTER_HPP
#define SHARDS_MODULE_HDL_VERILOG_EMITTER_HPP

#include "blocks.hpp"
#include <string>
#include <sstream>

namespace shards {
namespace hdl {

// Emits Verilog code from IR blocks
class VerilogEmitter {
public:
  VerilogEmitter() = default;

  // Emit a complete module
  std::string emit(const Module &module);

  // Emit an expression (for debugging/testing)
  std::string emitExpr(const Block &block);

private:
  std::stringstream _out;
  int _indentLevel = 0;

  void indent();
  void newline();
  void emitLine(const std::string &line);

  void emitPorts(const std::vector<Port> &ports);
  void emitWires(const std::vector<Wire> &wires);
  void emitAssigns(const std::vector<Assign> &assigns);

  void emitBlock(const Block &block);
  void emitSignalRef(const SignalRef &ref);
  void emitLiteral(const Literal &lit);
  void emitBinaryExpr(const BinaryExpr &expr);
  void emitUnaryExpr(const UnaryExpr &expr);
  void emitSlice(const Slice &slice);
  void emitConcat(const Concat &concat);
  void emitTernary(const Ternary &ternary);
};

// Convenience function
std::string emitVerilog(const Module &module);

} // namespace hdl
} // namespace shards

#endif // SHARDS_MODULE_HDL_VERILOG_EMITTER_HPP
