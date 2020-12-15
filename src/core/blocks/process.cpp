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
  static inline CBString keys[] = {"ExitCode", "StandardOutput",
                                   "StandardError"};
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
      if (suspend(ctx, 0) != CBChainState::Continue) {
        cmd.terminate();
        return Var::Empty;
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

    map["ExitCode"] = Var(cmd.exit_code());
    map["StandardOutput"] = Var(outBuf);
    map["StandardError"] = Var(errBuf);

    CBVar res{};
    res.valueType = Table;
    res.payload.tableValue.api = &Globals::TableInterface;
    res.payload.tableValue.opaque = &map;

    return res;
  }
};

struct Run {
  std::string _moduleName;
  ParamVar _arguments{};
  std::string _outBuf;
  std::string _errBuf;
  int64_t _timeout{30};

  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }
  static inline Parameters params{
      {"Executable",
       "The executable to run.",
       {CoreInfo::PathType, CoreInfo::StringType}},
      {"Arguments",
       "The arguments to pass to the executable.",
       {CoreInfo::NoneType, CoreInfo::StringSeqType,
        CoreInfo::StringVarSeqType}},
      {"Timeout",
       "The maximum time to wait for the executable to finish in seconds.",
       {CoreInfo::IntType}}};
  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _moduleName = value.payload.stringValue;
      break;
    case 1:
      _arguments = value;
      break;
    case 2:
      _timeout = value.payload.intValue;
      break;
    default:
      throw CBException("setParam out of range");
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_moduleName);
    case 1:
      return _arguments;
    case 2:
      return Var(_timeout);
    default:
      throw CBException("getParam out of range");
    }
  }

  void warmup(CBContext *context) { _arguments.warmup(context); }

  void cleanup() { _arguments.cleanup(); }

  CBVar activate(CBContext *context, const CBVar &input) {
    return awaitne(context, [&]() {
      // add any arguments we have
      std::vector<std::string> argsArray;
      auto argsVar = _arguments.get();
      if (argsVar.valueType == Seq) {
        IterableSeq args(argsVar.payload.seqValue);
        for (auto &arg : args) {
          argsArray.emplace_back(arg.payload.stringValue);
        }
      }

      boost::process::ipstream opipe;
      boost::process::ipstream epipe;
      boost::process::opstream ipipe;

      auto in = input.payload.stringLen > 0
                    ? std::string_view(input.payload.stringValue,
                                       input.payload.stringLen)
                    : std::string_view(input.payload.stringValue);
      ipipe.write(in.data(), in.length());

      // try PATH first
      auto exePath = boost::filesystem::path(_moduleName);
      if (!boost::filesystem::exists(exePath)) {
        // fallback to searching PATH
        exePath = boost::process::search_path(_moduleName);
      }

      if (exePath.empty()) {
        throw ActivationError("Executable not found.");
      }

      boost::process::child cmd(
          exePath, argsArray, boost::process::std_out > opipe,
          boost::process::std_err > epipe, boost::process::std_in < ipipe);
      if (!opipe || !epipe || !ipipe) {
        throw ActivationError("Failed to open streams for child process.");
      }

      cmd.wait_for(std::chrono::seconds(_timeout));

      _outBuf.clear();
      _errBuf.clear();
      std::stringstream ss;
      ss << opipe.rdbuf();
      _outBuf.assign(ss.str());
      ss.clear();
      ss << epipe.rdbuf();
      _errBuf.assign(ss.str());

      if (cmd.exit_code() != 0) {
        LOG(INFO) << _outBuf;
        LOG(ERROR) << _errBuf;
        std::string err("The process exited with a non-zero exit code: ");
        err += std::to_string(cmd.exit_code());
        throw ActivationError(err);
      }

      return Var(_outBuf);
    });
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
  REGISTER_CBLOCK("Process.Run", Process::Run);
  REGISTER_CBLOCK("Process.StackTrace", Process::StackTrace);
}
}; // namespace chainblocks
