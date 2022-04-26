/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "shared.hpp"
#include <string>

namespace chainblocks {
struct LoggingBase {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }
};

struct Log : public LoggingBase {
  static inline ParamsInfo msgParamsInfo =
      ParamsInfo(ParamsInfo::Param("Prefix", CBCCSTR("The message to prefix to the logged output."), CoreInfo::StringType));

  std::string msg;

  static CBParametersInfo parameters() { return CBParametersInfo(msgParamsInfo); }

  void setParam(int index, const CBVar &inValue) {
    switch (index) {
    case 0:
      msg = inValue.payload.stringValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    auto res = CBVar();
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

  CBVar activate(CBContext *context, const CBVar &input) {
    auto current = context->chainStack.back();
    if (msg.size() > 0) {
      CBLOG_INFO("[{}] {}: {}", current->name, msg, input);
    } else {
      CBLOG_INFO("[{}] {}", current->name, input);
    }
    return input;
  }
};

struct Msg : public LoggingBase {
  static inline ParamsInfo msgParamsInfo = ParamsInfo(
      ParamsInfo::Param("Message", CBCCSTR("The message to display on the user's screen or console."), CoreInfo::StringType));

  std::string msg;

  static CBParametersInfo parameters() { return CBParametersInfo(msgParamsInfo); }

  void setParam(int index, const CBVar &inValue) {
    switch (index) {
    case 0:
      msg = inValue.payload.stringValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    auto res = CBVar();
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

  CBVar activate(CBContext *context, const CBVar &input) {
    auto current = context->chainStack.back();
    CBLOG_INFO("[{}] {}", current->name, msg);
    return input;
  }
};

// Register Log
RUNTIME_CORE_BLOCK(Log);
RUNTIME_BLOCK_inputTypes(Log);
RUNTIME_BLOCK_outputTypes(Log);
RUNTIME_BLOCK_parameters(Log);
RUNTIME_BLOCK_setParam(Log);
RUNTIME_BLOCK_getParam(Log);
RUNTIME_BLOCK_activate(Log);
RUNTIME_BLOCK_END(Log);

// Register Msg
RUNTIME_CORE_BLOCK(Msg);
RUNTIME_BLOCK_inputTypes(Msg);
RUNTIME_BLOCK_outputTypes(Msg);
RUNTIME_BLOCK_parameters(Msg);
RUNTIME_BLOCK_setParam(Msg);
RUNTIME_BLOCK_getParam(Msg);
RUNTIME_BLOCK_activate(Msg);
RUNTIME_BLOCK_END(Msg);

void registerLoggingBlocks() {
  REGISTER_CORE_BLOCK(Log);
  REGISTER_CORE_BLOCK(Msg);
}
}; // namespace chainblocks
