#pragma once

#include "shared.hpp"
#include <string>

namespace chainblocks
{
  static CBParametersInfo msgParamsInfo;

  struct LoggingBase
  {
    CBTypesInfo inputTypes()
    {
      if(!anyInOutInfo)
      {
        CBTypeInfo anyType = { Any };
        stbds_arrpush(anyInOutInfo, anyType);
      }
      return anyInOutInfo;
    }
    
    CBTypesInfo outputTypes()
    {
      if(!anyInOutInfo)
      {
        CBTypeInfo anyType = { Any };
        stbds_arrpush(anyInOutInfo, anyType);
      }
      return anyInOutInfo;
    }
  };
  
  struct Log : public LoggingBase
  {
    CBVar activate(CBContext* context, CBVar input)
    {
      LOG(INFO) << input;
      return input;
    }
  };
  
  struct Msg : public LoggingBase
  {
    std::string msg;

    CBParametersInfo parameters()
    {
      if(!strInfo)
      {
        CBTypeInfo strType = { String };
        stbds_arrpush(strInfo, strType);
      }
      if(!msgParamsInfo)
      {
        CBParameterInfo msgInfo = { "Message", "The message to log.", strInfo };
        stbds_arrpush(msgParamsInfo, msgInfo);
      }
      return msgParamsInfo;
    }
    
    void setParam(int index, CBVar inValue)
    {
      switch(index)
      {
        case 0:
          msg = inValue.payload.stringValue;
          break;
        default:
          break;
      }
    }
    
    CBVar getParam(int index)
    {
      auto res = CBVar();
      switch(index)
      {
        case 0:
          res.valueType = String;
          res.payload.stringValue = msg.c_str();
          break;
        default:
          break;
      }
      return res;
    }
    
    CBVar activate(CBContext* context, CBVar input)
    {
      LOG(INFO) << msg;
      return input;
    }
  };
};

// Register Log
RUNTIME_CORE_BLOCK(chainblocks::Log, Log)
RUNTIME_BLOCK_inputTypes(Log)
RUNTIME_BLOCK_outputTypes(Log)
RUNTIME_BLOCK_activate(Log)
RUNTIME_CORE_BLOCK_END(Log)

// Register Msg
RUNTIME_CORE_BLOCK(chainblocks::Msg, Msg)
RUNTIME_BLOCK_inputTypes(Msg)
RUNTIME_BLOCK_outputTypes(Msg)
RUNTIME_BLOCK_parameters(Msg)
RUNTIME_BLOCK_setParam(Msg)
RUNTIME_BLOCK_getParam(Msg)
RUNTIME_BLOCK_activate(Msg)
RUNTIME_CORE_BLOCK_END(Msg)