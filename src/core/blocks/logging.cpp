/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#include "shared.hpp"
#include <string>

namespace chainblocks {
struct LoggingBase {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }
};

struct Log : public LoggingBase {
  static inline ParamsInfo msgParamsInfo = ParamsInfo(
      ParamsInfo::Param("Prefix", "The prefix message to the value to log.",
                        CoreInfo::StringType));

  std::string msg;

  static CBParametersInfo parameters() {
    return CBParametersInfo(msgParamsInfo);
  }

  void setParam(int index, CBVar inValue) {
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
    if (msg.size() > 0) {
      LOG(INFO) << "{" << context->current->name << "} " << msg << ": "
                << input;
    } else {
      LOG(INFO) << "{" << context->current->name << "} " << input;
    }
    return input;
  }
};

struct Msg : public LoggingBase {
  static inline ParamsInfo msgParamsInfo = ParamsInfo(ParamsInfo::Param(
      "Message", "The message to log.", CoreInfo::StringType));

  std::string msg;

  static CBParametersInfo parameters() {
    return CBParametersInfo(msgParamsInfo);
  }

  void setParam(int index, CBVar inValue) {
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
    LOG(INFO) << "{" << context->current->name << "} " << msg;
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
