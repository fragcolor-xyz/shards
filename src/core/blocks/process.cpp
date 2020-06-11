/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#ifdef _WIN32
#include "winsock2.h"
#endif

#include "shared.hpp"

// workaround for a boost bug..
#ifndef __kernel_entry
#define __kernel_entry
#endif
#include <boost/process.hpp>
#include <boost/stacktrace.hpp>
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
      throw ActivationError("Failed to open streams for child process.");
    }

    while (cmd.running()) {
      try {
        chainblocks::suspend(ctx, 0);
      } catch (...) {
        cmd.terminate();
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
    res.payload.tableValue.api = &Globals::TableInterface;
    res.payload.tableValue.opaque = &map;

    return res;
  }
};

struct StackTrace {
  std::string _output;
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }

  CBVar activate(CBContext *ctx, CBVar input) {
    std::stringstream ss;
    ss << boost::stacktrace::stacktrace();
    _output = ss.str();
    return Var(_output);
  }
};
}; // namespace Process

void registerProcessBlocks() {
  REGISTER_CBLOCK("Process.Exec", Process::Exec);
  REGISTER_CBLOCK("Process.StackTrace", Process::StackTrace);
}
}; // namespace chainblocks
