/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#include "shared.hpp"
#include <string>

namespace chainblocks {
static ParamsInfo msgParamsInfo = ParamsInfo(ParamsInfo::Param(
    "Message", "The message to log.", CBTypesInfo(SharedTypes::strInfo)));

struct LoggingBase {
  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }

  static CBTypesInfo outputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }
};

struct Log : public LoggingBase {
  static CBVar activate(CBContext *context, const CBVar &input) {
    LOG(INFO) << "{" << context->chain->name << "} " << input;
    return input;
  }
};

struct Msg : public LoggingBase {
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
    LOG(INFO) << "{" << context->chain->name << "} " << msg;
    return input;
  }
};

// Register Log
RUNTIME_CORE_BLOCK(Log);
RUNTIME_BLOCK_inputTypes(Log);
RUNTIME_BLOCK_outputTypes(Log);
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
