#pragma once

#include "shared.hpp"

namespace chainblocks
{
  static ParamsInfo tableParamsInfo = ParamsInfo(
    ParamsInfo::Param("Name", "The name of the table variable.", CBTypesInfo(strInfo)),
    ParamsInfo::Param("Key", "The key of the value to write in the table.", CBTypesInfo(strInfo))
  );

  struct SetTableValue
  {
    CBVar* target = nullptr;
    std::string name;
    std::string key;
    CBVar currentValue;
    
    ExposedInfo tableExposedInfo;
    TypesInfo tableTypeInfo;
    TypesInfo contentInfo;
    
    void cleanup()
    {
      if(target && target->valueType == Table)
      {
        // Destroy the value we are holding..
        destroyVar(currentValue);
        // Remove from table
        stbds_shdel(target->payload.tableValue, key.c_str());
        // Finally free the table if has no values
        if(stbds_shlen(target->payload.tableValue) == 0)
        {
          stbds_shfree(target->payload.tableValue);
          target->payload.tableValue = nullptr;
        }
      }
      
      target = nullptr;
    }
    
    CBTypesInfo inputTypes()
    {
      return CBTypesInfo(anyInfo);
    }
    
    CBTypesInfo outputTypes()
    {
      return CBTypesInfo(anyInfo);
    }
    
    CBParametersInfo parameters()
    {
      return CBParametersInfo(tableParamsInfo);
    }
    
    CBExposedTypesInfo exposedVariables()
    {
      return CBExposedTypesInfo(tableExposedInfo);
    }
    
    CBTypeInfo inferTypes(CBTypeInfo inputType, CBExposedTypesInfo consumableVariables)
    {
      contentInfo = TypesInfo(inputType);
      tableTypeInfo = TypesInfo(CBType::Table, CBTypesInfo(contentInfo));
      tableExposedInfo = ExposedInfo(ExposedInfo::Variable(name.c_str(), "The exposed table.", CBTypeInfo(tableTypeInfo) ));
      return inputType;
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
        CBNamedVar defaultNamed =  { nullptr, defaultVar };
        stbds_shdefaults(target->payload.tableValue, defaultNamed);
      }
      
      // Get the current value, if not in it will be default None anyway
      // If was in we actually clone on top of it so we recycle memory
      currentValue = stbds_shget(target->payload.tableValue, key.c_str());
      chainblocks::cloneVar(currentValue, input);
      stbds_shput(target->payload.tableValue, key.c_str(), currentValue);
      
      return input;
    }
  };

  struct GetTableValue
  {
    CBVar* target = nullptr;
    std::string name;
    std::string key;

    CBExposedTypesInfo tableExposedInfo = nullptr;
    
    void cleanup()
    {
      target = nullptr;
    }
    
    CBTypesInfo inputTypes()
    {
      return CBTypesInfo(noneInfo);
    }
    
    CBTypesInfo outputTypes()
    {
      return CBTypesInfo(anyInfo);
    }
    
    CBParametersInfo parameters()
    {
      return CBParametersInfo(tableParamsInfo);
    }

    CBExposedTypesInfo consumedVariables()
    {
      if(!tableExposedInfo)
      {
        CBTypeInfo atableType = { Table };
        CBExposedTypeInfo nameInfo = { name.c_str(), "The exposed table.", atableType };
        stbds_arrpush(tableExposedInfo, nameInfo);
      }
      return tableExposedInfo;
    }
    
    CBTypeInfo inferTypes(CBTypeInfo inputType, CBExposedTypesInfo consumableVariables)
    {
      for(auto i = 0; i < stbds_arrlen(consumableVariables); i++)
      {
        if(consumableVariables[i].name == name && consumableVariables[i].exposedType.tableTypes)
        {
          return consumableVariables[i].exposedType.tableTypes[0]; // TODO taking only the first for now
        }
      }
      assert(false);
      return CBTypeInfo();
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
      
      auto res = CBVar();
      
      if(target->valueType == Table)
      {
        res = stbds_shget(target->payload.tableValue, key.c_str());
      }
      
      return res;
    }
  };
};

// Register SetTableValue
RUNTIME_CORE_BLOCK(chainblocks::SetTableValue, SetTableValue)
RUNTIME_BLOCK_cleanup(SetTableValue)
RUNTIME_BLOCK_inputTypes(SetTableValue)
RUNTIME_BLOCK_outputTypes(SetTableValue)
RUNTIME_BLOCK_parameters(SetTableValue)
RUNTIME_BLOCK_exposedVariables(SetTableValue)
RUNTIME_BLOCK_inferTypes(SetTableValue)
RUNTIME_BLOCK_setParam(SetTableValue)
RUNTIME_BLOCK_getParam(SetTableValue)
RUNTIME_BLOCK_activate(SetTableValue)
RUNTIME_CORE_BLOCK_END(SetTableValue)

// Register GetTableValue
RUNTIME_CORE_BLOCK(chainblocks::GetTableValue, GetTableValue)
RUNTIME_BLOCK_cleanup(GetTableValue)
RUNTIME_BLOCK_inputTypes(GetTableValue)
RUNTIME_BLOCK_outputTypes(GetTableValue)
RUNTIME_BLOCK_parameters(GetTableValue)
RUNTIME_BLOCK_consumedVariables(GetTableValue)
RUNTIME_BLOCK_inferTypes(GetTableValue)
RUNTIME_BLOCK_setParam(GetTableValue)
RUNTIME_BLOCK_getParam(GetTableValue)
RUNTIME_BLOCK_activate(GetTableValue)
RUNTIME_CORE_BLOCK_END(GetTableValue)
