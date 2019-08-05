#pragma once

#include "shared.hpp"
#include <string>
#include <sstream>
#include <boost/process.hpp>

namespace chainblocks
{
  namespace Process
  {
    struct Exec
    {
      // Spawns a child runs it, waits results and outputs them!
      std::string output;
      
      CBTypesInfo inputTypes()
      {
        if(!strInfo)
        {
          CBTypeInfo strType = { String };
          stbds_arrpush(strInfo, strType);
        }
        return strInfo;
      }
      
      CBTypesInfo outputTypes()
      {
        if(!strInfo)
        {
          CBTypeInfo strType = { String };
          stbds_arrpush(strInfo, strType);
        }
        return anyInOutInfo;
      }
      
      CBVar activate(CBContext* ctx, CBVar input)
      {
        output.clear();
        
        boost::process::ipstream opipe;
        boost::process::ipstream epipe;
        boost::process::child cmd(input.payload.stringValue, boost::process::std_out > opipe, boost::process::std_err > epipe);
        if(!opipe || !epipe)
        {
          throw CBException("Failed to open streams for child process.");
        }
        
        while(cmd.running())
        {
          chainblocks::suspend(0.0);
        }
        
        std::stringstream ss;
        ss << opipe.rdbuf();
        ss << epipe.rdbuf();
        output.assign(ss.str());
        
        CBVar res;
        res.valueType = String;
        res.payload.stringValue = output.c_str();
        
        return res;
      }
    };
  };

// Register Exec
RUNTIME_BLOCK(chainblocks::Process::Exec, Process, Exec)
RUNTIME_BLOCK_inputTypes(Exec)
RUNTIME_BLOCK_outputTypes(Exec)
RUNTIME_BLOCK_activate(Exec)
RUNTIME_BLOCK_END(Exec)
};