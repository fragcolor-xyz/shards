/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>

#include <stdlib.h>

#ifdef _WIN32
#include "winsock2.h"
#define SHAlloc SHAlloc1
#define SHFree SHFree1
#include <windows.h>
#include <ShlObj.h>
#undef SHAlloc
#undef SHFree
#endif

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
  std::array<SHExposedTypeInfo, 1> _requiring;
  std::string _outBuf;
  std::string _errBuf;
  std::optional<boost::process::child *> _cmd;

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  PARAM_PARAMVAR(_executable, "Executable", "The executable to run.",
                 {CoreInfo::PathType, CoreInfo::PathVarType, CoreInfo::StringType, CoreInfo::StringVarType});
  PARAM_PARAMVAR(_arguments, "Arguments", "The arguments to pass to the executable.",
                 {CoreInfo::NoneType, CoreInfo::StringSeqType, CoreInfo::StringVarSeqType});
  PARAM_VAR(_timeout, "Timeout", "The maximum time to wait for the executable to finish in seconds.",
            {CoreInfo::NoneType, CoreInfo::IntType});
  PARAM_IMPL(PARAM_IMPL_FOR(_executable), PARAM_IMPL_FOR(_arguments), PARAM_IMPL_FOR(_timeout));

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar run(std::string moduleName, std::vector<std::string> argsArray, const SHVar &input) {
    // use async asio to avoid deadlocks
    boost::asio::io_service ios;
    std::future<std::string> ostr;
    std::future<std::string> estr;
    boost::process::opstream ipipe;

    // try PATH first
    auto exePath = boost::filesystem::path(moduleName);
    if (!boost::filesystem::exists(exePath)) {
      // fallback to searching PATH
      exePath = boost::process::search_path(moduleName);
    }

    if (exePath.empty()) {
      throw ActivationError("Executable not found");
    }

    exePath = exePath.make_preferred();

    boost::process::child cmd(exePath, argsArray, boost::process::std_out > ostr, boost::process::std_err > estr,
                              boost::process::std_in < ipipe, ios);

    _cmd = &cmd;

    if (!ipipe) {
      throw ActivationError("Failed to open streams for child process");
    }

    ipipe << SHSTRVIEW(input) << std::endl;
    ipipe.pipe().close(); // send EOF

    SHLOG_TRACE("Process started");

    auto timeout = std::chrono::seconds(_timeout->isNone() ? 30 : (int)*_timeout);
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
    _outBuf = ostr.get();
    _errBuf = estr.get();

    if (cmd.exit_code() != 0) {
      SHLOG_INFO(_outBuf);
      SHLOG_ERROR(_errBuf);
      std::string err("The process exited with a non-zero exit code: ");
      err += std::to_string(cmd.exit_code());
      throw ActivationError(err);
    } else {
      if (_errBuf.size() > 0) {
        // print anyway this stream too
        SHLOG_INFO("(stderr) {}", _errBuf);
      }
      SHLOG_TRACE("Process finished successfully");
      return Var(_outBuf);
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    return awaitne(
        context,
        [&]() {
          auto moduleName = (std::string)(Var &)_executable.get();

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
          return run(moduleName, argsArray, input);
        },
        [&] {
          SHLOG_DEBUG("Process terminated");
          if (_cmd) {
            (*_cmd)->terminate();
          }
        });
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

struct Exe {
  std::string _buf;

  static SHTypesInfo inputTypes() { return shards::CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::StringType; }
  static SHOptionalString help() { return SHCCSTR("Gives the current executable path."); }

  PARAM_IMPL();

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) { return outputTypes().elements[0]; }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    _buf.clear();
#ifdef _WIN32
    _buf.resize(_MAX_PATH);
    _buf.resize(GetModuleFileNameA(nullptr, _buf.data(), _MAX_PATH));
#elif defined __ANDROID__
#if __ANDROID_API__ >= 21
    _buf = getprogname();
#endif
#elif defined __linux__ && defined _GNU_SOURCE
    _buf = program_invocation_short_name;
#elif defined __APPLE__ || defined BSD
    _buf = getprogname();
#endif
    return Var(_buf);
  }
};
} // namespace Process

SHARDS_REGISTER_FN(process) {
  REGISTER_SHARD("Process.Run", Process::Run);
  REGISTER_SHARD("Process.StackTrace", Process::StackTrace);
  REGISTER_SHARD("Process.Exe", Process::Exe);
}
} // namespace shards
// namespace shards
