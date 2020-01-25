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
  static inline TypeInfo tableType = TypeInfo::MultiTypeTable(
      std::tuple<CBTypeInfo, const char *>{CBTypeInfo(SharedTypes::intInfo),
                                           "error_code"},
      std::tuple<CBTypeInfo, const char *>{CBTypeInfo(SharedTypes::strInfo),
                                           "std_out"},
      std::tuple<CBTypeInfo, const char *>{CBTypeInfo(SharedTypes::strInfo),
                                           "std_err"});
  static inline TypesInfo outType = TypesInfo(tableType);

  // Spawns a child runs it, waits results and outputs them!
  std::string outBuf;
  std::string errBuf;
  CBTable outputTable;

  void destroy() { stbds_shfree(outputTable); }

  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::strInfo); }

  static CBTypesInfo outputTypes() { return CBTypesInfo(outType); }

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

    stbds_shput(outputTable, "error_code", Var(cmd.exit_code()));
    stbds_shput(outputTable, "std_out", Var(outBuf));
    stbds_shput(outputTable, "std_err", Var(errBuf));

    return Var(outputTable);
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
