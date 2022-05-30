/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "shared.hpp"
#include <string>

namespace shards {
struct LoggingBase {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
};

struct Log : public LoggingBase {
  static inline ParamsInfo msgParamsInfo =
      ParamsInfo(ParamsInfo::Param("Prefix", SHCCSTR("The message to prefix to the logged output."), CoreInfo::StringType));

  std::string msg;

  static SHParametersInfo parameters() { return SHParametersInfo(msgParamsInfo); }

  static SHOptionalString help() {
    return SHCCSTR(
        "Logs the output of a shard or the value of a variable to the console (along with an optional prefix string).");
  }

  void setParam(int index, const SHVar &inValue) {
    switch (index) {
    case 0:
      msg = inValue.payload.stringValue;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    auto res = SHVar();
    switch (index) {
    case 0:
      res.valueType = String;
      res.payload.stringValue = msg.c_str();
      break;
    default:
      break;
    }
    return res;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto current = context->wireStack.back();
    if (msg.size() > 0) {
      SHLOG_INFO("[{}] {}: {}", current->name, msg, input);
    } else {
      SHLOG_INFO("[{}] {}", current->name, input);
    }
    return input;
  }
};

struct Msg : public LoggingBase {
  static inline ParamsInfo msgParamsInfo = ParamsInfo(
      ParamsInfo::Param("Message", SHCCSTR("The message to display on the user's screen or console."), CoreInfo::StringType));

  std::string msg;

  static SHParametersInfo parameters() { return SHParametersInfo(msgParamsInfo); }

  static SHOptionalString help() {
    return SHCCSTR("Displays the passed message string or the passed variable's value to the user via standard output.");
  }

  void setParam(int index, const SHVar &inValue) {
    switch (index) {
    case 0:
      msg = inValue.payload.stringValue;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    auto res = SHVar();
    switch (index) {
    case 0:
      res.valueType = String;
      res.payload.stringValue = msg.c_str();
      break;
    default:
      break;
    }
    return res;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto current = context->wireStack.back();
    SHLOG_INFO("[{}] {}", current->name, msg);
    return input;
  }
};

// Register Log
RUNTIME_CORE_SHARD(Log);
RUNTIME_SHARD_help(Log);
RUNTIME_SHARD_inputTypes(Log);
RUNTIME_SHARD_outputTypes(Log);
RUNTIME_SHARD_parameters(Log);
RUNTIME_SHARD_setParam(Log);
RUNTIME_SHARD_getParam(Log);
RUNTIME_SHARD_activate(Log);
RUNTIME_SHARD_END(Log);

// Register Msg
RUNTIME_CORE_SHARD(Msg);
RUNTIME_SHARD_help(Msg);
RUNTIME_SHARD_inputTypes(Msg);
RUNTIME_SHARD_outputTypes(Msg);
RUNTIME_SHARD_parameters(Msg);
RUNTIME_SHARD_setParam(Msg);
RUNTIME_SHARD_getParam(Msg);
RUNTIME_SHARD_activate(Msg);
RUNTIME_SHARD_END(Msg);

void registerLoggingShards() {
  REGISTER_CORE_SHARD(Log);
  REGISTER_CORE_SHARD(Msg);
}
}; // namespace shards
