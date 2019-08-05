#pragma once

#include "shared.hpp"
#include <string>
#include <sstream>
#include <boost/process.hpp>

namespace chainblocks
{
  namespace Process
  {
    static CBTypesInfo execTableInfos;
    static CBTypeInfo execTableInfo = { Table };

    struct Exec
    {
      // Spawns a child runs it, waits results and outputs them!
      std::string outBuf;
      std::string errBuf;
      CBTable outputTable;
      
      void destroy()
      {
        stbds_shfree(outputTable);
      }
      
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
        if(!execTableInfos)
        {
          CBTypeInfo intType = { Int };
          CBTypeInfo strType = { String };
          stbds_arrpush(execTableInfo.tableTypes, intType);
          stbds_arrpush(execTableInfo.tableTypes, strType);
          
          stbds_arrpush(execTableInfos, execTableInfo);
        }
        return execTableInfos;
      }
      
      CBVar activate(CBContext* ctx, CBVar input)
      {
        boost::process::ipstream opipe;
        boost::process::ipstream epipe;
        boost::process::child cmd(input.payload.stringValue, boost::process::std_out > opipe, boost::process::std_err > epipe);
        if(!opipe || !epipe)
        {
          throw CBException("Failed to open streams for child process.");
        }
        
        while(cmd.running())
        {
          chainblocks::suspend(0.0); // TODO terminate if we interrupt!!! / Run on thread
        }
        
        // Let's try to be efficient in terms of reusing memory, altho maybe the SS is not necessary, could not figure
        outBuf.clear();
        errBuf.clear();
        std::stringstream ss;
        ss << opipe.rdbuf();
        outBuf.assign(ss.str());
        ss.clear();
        ss << epipe.rdbuf();
        errBuf.assign(ss.str());
        
        stbds_shput(outputTable, "error_code", Var(cmd.exit_code()));
        stbds_shput(outputTable, "std_out", Var(outBuf));
        stbds_shput(outputTable, "std_err", Var(errBuf));
        
        return Var(outputTable);
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