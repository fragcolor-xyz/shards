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

      // try PATH first
      auto exePath = boost::filesystem::path(_moduleName);
      if (!boost::filesystem::exists(exePath)) {
        // fallback to searching PATH
        exePath = boost::process::search_path(_moduleName);
      }

      if (exePath.empty()) {
        throw ActivationError("Executable not found");
      }

      boost::process::child cmd(
          exePath, argsArray, boost::process::std_out > opipe,
          boost::process::std_err > epipe, boost::process::std_in < ipipe);

      if (!opipe || !epipe || !ipipe) {
        throw ActivationError("Failed to open streams for child process");
      }

      ipipe << input.payload.stringValue << std::endl;
      ipipe.pipe().close(); // send EOF

      if (cmd.wait_for(std::chrono::seconds(_timeout))) {
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
        } else {
          if (_errBuf.size() > 0) {
            // print anyway this stream too
            LOG(INFO) << "(stderr) " << _errBuf;
          }
          return Var(_outBuf);
        }
      } else {
        throw ActivationError("Timed out");
      }
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
  REGISTER_CBLOCK("Process.Run", Process::Run);
  REGISTER_CBLOCK("Process.StackTrace", Process::StackTrace);
}
}; // namespace chainblocks
