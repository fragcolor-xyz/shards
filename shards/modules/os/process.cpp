/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "shards/core/module.hpp"
#ifdef _WIN32
#include "winsock2.h"
#endif

#include <shards/core/shared.hpp>

// workaround for a boost bug..
#ifndef __kernel_entry
#define __kernel_entry
#endif

#pragma clang diagnostic push
// Disable warning inside boost process posix implementation
#pragma clang diagnostic ignored "-Wc++11-narrowing"
#include <boost/asio.hpp>
#include <boost/process.hpp>
#include <boost/stacktrace.hpp>
#include <sstream>
#include <string>
#pragma clang diagnostic pop

#include <shards/core/async.hpp>

namespace shards {
namespace Process {
struct Run {
  std::string _moduleName;
  ParamVar _arguments{};
  std::array<SHExposedTypeInfo, 1> _requiring;
  OwnedVar _input;
  std::string _innerBuf;
  OwnedVar _outputBuf;
  std::string _innerErrorBuf;
  std::string _errorBuf;
  int64_t _timeout{30};

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }
  static inline Parameters params{
      {"Executable", SHCCSTR("The executable to run."), {CoreInfo::PathType, CoreInfo::StringType}},
      {"Arguments",
       SHCCSTR("The arguments to pass to the executable."),
       {CoreInfo::NoneType, CoreInfo::StringSeqType, CoreInfo::StringVarSeqType}},
      {"Timeout", SHCCSTR("The maximum time to wait for the executable to finish in seconds."), {CoreInfo::IntType}}};
  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _moduleName = SHSTRVIEW(value);
      break;
    case 1:
      _arguments = value;
      break;
    case 2:
      _timeout = value.payload.intValue;
      break;
    default:
      throw SHException("setParam out of range");
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_moduleName);
    case 1:
      return _arguments;
    case 2:
      return Var(_timeout);
    default:
      throw SHException("getParam out of range");
    }
  }

  SHExposedTypesInfo requiredVariables() {
    if (_arguments.isVariable()) {
      _requiring[0].name = _arguments.variableName();
      _requiring[0].help = SHCCSTR("The required variable containing the arguments for the command to run.");
      _requiring[0].exposedType = CoreInfo::StringSeqType;
      return {_requiring.data(), 1, 0};
    } else {
      return {};
    }
  }

  void warmup(SHContext *context) { _arguments.warmup(context); }

  void cleanup(SHContext *context) { _arguments.cleanup(); }

  SHVar activate(SHContext *context, const SHVar &input) {
    std::optional<boost::process::child *> pCmd;
    _input = input;
    _outputBuf = maybeAwaitne(
        context,
        [&]() {
          // add any arguments we have
          std::vector<std::string> argsArray;
          auto argsVar = _arguments.get();
          if (argsVar.valueType == SHType::Seq) {
            for (auto &arg : argsVar) {
              if (arg.payload.stringLen > 0) {
                argsArray.emplace_back(arg.payload.stringValue, arg.payload.stringLen);
              } else {
                // if really empty likely it's an error (also windows will fail
                // converting to utf16) if not maybe the string just didn't have
                // len set
                if (strlen(arg.payload.stringValue) == 0) {
                  throw ActivationError("Empty argument passed, this most likely is a mistake.");
                } else {
                  argsArray.emplace_back(arg.payload.stringValue); // ParamVar so should be same
                }
              }
            }
          }

          // use async asio to avoid deadlocks
          boost::asio::io_service ios;
          std::future<std::string> ostr;
          std::future<std::string> estr;
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

          exePath = exePath.make_preferred();

          boost::process::child cmd(exePath, argsArray, boost::process::std_out > ostr, boost::process::std_err > estr,
                                    boost::process::std_in < ipipe, ios);

          pCmd = &cmd;

          if (!ipipe) {
            throw ActivationError("Failed to open streams for child process");
          }

          ipipe << SHSTRVIEW(_input) << std::endl;
          ipipe.pipe().close(); // send EOF

          SHLOG_TRACE("Process started");

          auto timeout = std::chrono::seconds(_timeout);
          auto endTime = std::chrono::system_clock::now() + timeout;
          ios.run_for(timeout);

          SHLOG_TRACE("Process finished");

          if (cmd.running()) {
            SHLOG_TRACE("Process still running after service wait");
            if (std::chrono::system_clock::now() > endTime) {
              cmd.terminate();
              throw ActivationError("Process timed out");
            } else {
              // give a further 1 second to terminate
              if (!cmd.wait_for(std::chrono::seconds(1))) {
                cmd.terminate();
              }
            }
          }

          // we still need to wait termination
          _innerBuf = ostr.get();
          _innerErrorBuf = estr.get();

          if (cmd.exit_code() != 0) {
            if (!_innerBuf.empty())
              SHLOG_INFO(_innerBuf);
            if (!_innerErrorBuf.empty())
              SHLOG_ERROR(_innerErrorBuf);
            std::string err("The process exited with a non-zero exit code: ");
            err += std::to_string(cmd.exit_code());
            throw ActivationError(err);
          } else {
            if (_innerErrorBuf.size() > 0) {
              // print anyway this stream too
              SHLOG_INFO("(stderr) {}", _innerErrorBuf);
            }
            SHLOG_TRACE("Process finished successfully");
            return Var(_innerBuf);
          }
        },
        [&] {
          if (pCmd) {
            (*pCmd)->terminate();
            SHLOG_DEBUG("Process terminated");
          }
        });
    return _outputBuf;
  }
};

struct StackTrace {
  std::string _output;
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  SHVar activate(SHContext *ctx, SHVar input) {
    std::stringstream ss;
    ss << boost::stacktrace::stacktrace();
    _output = ss.str();
    return Var(_output);
  }
};
}; // namespace Process

SHARDS_REGISTER_FN(process) {
  REGISTER_SHARD("Process.Run", Process::Run);
  REGISTER_SHARD("Process.StackTrace", Process::StackTrace);
}
}; // namespace shards
