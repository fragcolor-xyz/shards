#include "../chainblocks.hpp"
#include "../blocks_macros.hpp"

namespace chainblocks
{
  struct SetTableValue
  {
    CBVar* target = nullptr;
    std::string name;
    std::string key;
    
    void cleanup()
    {
      if(target)
        destroyVar(*target);
      target = nullptr;
    }
    
    CBTypesInfo inputTypes()
    {
      static CBTypeInfo info[] = {{ Any, true /*sequenced*/ }};
      return info;
    }
    
    CBTypesInfo outputTypes()
    {
      static CBTypeInfo info[] = {{ Any, true /*sequenced*/ }};
      return info;
    }
    
    CBParametersInfo parameters()
    {
      static CBTypeInfo strInfo[] = {{ String, false /*sequenced*/ }};
      static CBParameterInfo info[] = {
        { "Name", "The name of the table variable.", strInfo},
        { "Key", "The key of the value to write in the table.", strInfo}
      };
      return info;
    }
    
    void setParam(int index, CBVar value)
    {
      switch(index)
      {
        case 0:
          name = value.payload.stringValue;
          break;
        case 1:
          key = value.payload.stringValue;
          break;
        default:
          break;
      }
    }
    
    CBVar getParam(int index)
    {
      CBVar res;
      res.valueType = String;
      switch(index)
      {
        case 0:
          res.payload.stringValue = name.c_str();
          break;
        case 1:
          res.payload.stringValue = key.c_str();
          break;
        default:
          break;
      }
      return res;
    }
    
    CBVar activate(CBContext* context, CBVar input)
    {
      if(!target)
      {
        target = chainblocks::contextVariable(context, name.c_str());
      }
      
      if(target->valueType == None)
      {
        // Not initialized yet
        target->valueType = Table;
        target->payload.tableValue = nullptr;
        // Also set table default
        CBVar defaultVar;
        defaultVar.valueType = None;
        defaultVar.payload.chainState = Continue;
        CBNamedVar defaultNamed =  { defaultVar, nullptr };
        stbds_shdefaults(target->payload.tableValue, defaultNamed);
      }
      
      // Get the current value, if not in it will be default None anyway
      // If was in we actually clone on top of it so we recycle memory
      auto currentValue = stbds_shgets(target->payload.tableValue, key.c_str());
      chainblocks::cloneVar(currentValue.value, input);
      currentValue.key = key.c_str();
      stbds_shputs(target->payload.tableValue, currentValue);
      
      return input;
    }
  };
};

// Register SetTableValue
RUNTIME_CORE_BLOCK(chainblocks::SetTableValue, SetTableValue)
RUNTIME_BLOCK_cleanup(SetTableValue)
RUNTIME_BLOCK_inputTypes(SetTableValue)
RUNTIME_BLOCK_outputTypes(SetTableValue)
RUNTIME_BLOCK_parameters(SetTableValue)
RUNTIME_BLOCK_setParam(SetTableValue)
RUNTIME_BLOCK_getParam(SetTableValue)
RUNTIME_BLOCK_activate(SetTableValue)
RUNTIME_BLOCK_END(SetTableValue)
