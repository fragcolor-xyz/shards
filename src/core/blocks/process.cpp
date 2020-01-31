/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#ifdef _WIN32
#include "winsock2.h"
#endif

#include "shared.hpp"

// workaround for a boost bug..
#ifndef __kernel_entry
#define __kernel_entry
#endif
#include <boost/process.hpp>
#include <sstream>
#include <string>

namespace chainblocks {
namespace Process {

struct Exec {
  static inline CBString keys[] = {"error_code", "std_out", "std_err"};
  static inline CBTypeInfo types[] = {CoreInfo::IntType, CoreInfo::StringType,
                                      CoreInfo::StringType};
  static inline Types tableType{
      {CBType::Table,
       {.table = {.keys = {keys, 3, 0}, .types = {types, 3, 0}}}}};

  // Spawns a child runs it, waits results and outputs them!
  std::string outBuf;
  std::string errBuf;
  CBMap map;

  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }

  static CBTypesInfo outputTypes() { return tableType; }

  CBVar activate(CBContext *ctx, CBVar input) {
    boost::process::ipstream opipe;
    boost::process::ipstream epipe;
    boost::process::child cmd(input.payload.stringValue,
                              boost::process::std_out > opipe,
                              boost::process::std_err > epipe);
    if (!opipe || !epipe) {
      throw CBException("Failed to open streams for child process.");
    }

    while (cmd.running()) {
      auto chainState = chainblocks::suspend(ctx, 0);
      if (chainState.payload.chainState != Continue) {
        cmd.terminate();
        return chainState;
      }
    }

    // Let's try to be efficient in terms of reusing memory, altho maybe the SS
    // is not necessary, could not figure
    outBuf.clear();
    errBuf.clear();
    std::stringstream ss;
    ss << opipe.rdbuf();
    outBuf.assign(ss.str());
    ss.clear();
    ss << epipe.rdbuf();
    errBuf.assign(ss.str());

    map["error_code"] = Var(cmd.exit_code());
    map["std_out"] = Var(outBuf);
    map["std_err"] = Var(errBuf);

    CBVar res{};
    res.valueType = Table;
    res.payload.tableValue.interface = &Globals::TableInterface;
    res.payload.tableValue.opaque = &map;

    return res;
  }
};

// Register Exec
RUNTIME_BLOCK(Process, Exec);
RUNTIME_BLOCK_inputTypes(Exec);
RUNTIME_BLOCK_outputTypes(Exec);
RUNTIME_BLOCK_activate(Exec);
RUNTIME_BLOCK_END(Exec);
}; // namespace Process

void registerProcessBlocks() { REGISTER_BLOCK(Process, Exec); }
}; // namespace chainblocks
