#define STB_DS_IMPLEMENTATION 1
#define CHAINBLOCKS_RUNTIME 1
#define DLL_EXPORT 1
#include "chainblocks.hpp"

namespace chainblocks
{
  std::unordered_map<std::string, CBBlockConstructor> BlocksRegister;
  std::unordered_map<std::tuple<int32_t, int32_t>, CBObjectInfo> ObjectTypesRegister;
  std::unordered_map<std::tuple<int32_t, int32_t>, CBEnumInfo> EnumTypesRegister;
  std::unordered_map<std::string, CBVar> GlobalVariables;
  std::unordered_map<std::string, CBOnRunLoopTick> RunLoopHooks;
  std::unordered_map<std::string, CBChain*> GlobalChains;
  thread_local CBChain* CurrentChain;

  void CBVar::releaseMemory()
  {
    if((valueType == String || valueType == ContextVar) && stringValue != nullptr)
    {
      delete[] stringValue;
      stringValue = nullptr;
    }
    else if(valueType == Seq && seqValue)
    {
      for(auto i = 0; i < stbds_arrlen(seqValue); i++)
      {
        seqValue[i].releaseMemory();
      }
      stbds_arrfree(seqValue);
      seqValue = nullptr;
    }
    else if(valueType == Table && tableValue)
    {
      for(auto i = 0; i < stbds_shlen(tableValue); i++)
      {
        delete[] tableValue[i].key;
        tableValue[i].value.releaseMemory();
      }
      stbds_shfree(tableValue);
      tableValue = nullptr;
    }
    else if(valueType == Image)
    {
      imageValue.dealloc();
    }
  }
};

#ifdef __cplusplus
extern "C" {
#endif

EXPORTED void __cdecl chainblocks_RegisterBlock(const char* fullName, CBBlockConstructor constructor)
{
  chainblocks::registerBlock(fullName, constructor);
}

EXPORTED void __cdecl chainblocks_RegisterObjectType(int32_t vendorId, int32_t typeId, CBObjectInfo info)
{
  chainblocks::registerObjectType(vendorId, typeId, info);
}

EXPORTED void __cdecl chainblocks_RegisterEnumType(int32_t vendorId, int32_t typeId, CBEnumInfo info)
{
  chainblocks::registerEnumType(vendorId, typeId, info);
}

EXPORTED void __cdecl chainblocks_RegisterRunLoopCallback(const char* eventName, CBOnRunLoopTick callback)
{
  chainblocks::registerRunLoopCallback(eventName, callback);
}

EXPORTED void __cdecl chainblocks_UnregisterRunLoopCallback(const char* eventName)
{
  chainblocks::unregisterRunLoopCallback(eventName);
}

EXPORTED CBVar* __cdecl chainblocks_ContextVariable(CBContext* context, const char* name)
{
  return chainblocks::contextVariable(context, name);
}

EXPORTED void __cdecl chainblocks_SetError(CBContext* context, const char* errorText)
{
  context->setError(errorText);
}

EXPORTED CBVar __cdecl chainblocks_Suspend(double seconds)
{
  return chainblocks::suspend(seconds);
}

#ifdef __cplusplus
};
#endif

void to_json(json& j, const CBVar& var)
{
  auto valType = int(var.valueType);
  switch(var.valueType)
  {
    case None:
    {
      j = json{
        { "type", 0 },
        { "value", int(var.chainState) }
      };
      break;
    }
    case Object:
    {
      auto findIt = chainblocks::ObjectTypesRegister.find(std::make_tuple(var.objectVendorId, var.objectTypeId));
      if(findIt != chainblocks::ObjectTypesRegister.end() && findIt->second.serialize)
      {
        j = json{
          { "type", valType },
          { "vendorId", var.objectVendorId },
          { "typeId", var.objectTypeId },
          { "value", findIt->second.serialize(var.objectValue) }
        };
      }
      else
      {
        j = json{
          { "type", valType },
          { "vendorId", var.objectVendorId },
          { "typeId", var.objectTypeId },
          { "value", nullptr }
        };
      }
      break;
    }
    case Bool:
    {
      j = json{
        { "type", valType },
        { "value", var.boolValue }
      };
      break;
    }
    case Int:
    {
      j = json{
        { "type", valType },
        { "value", var.intValue }
      };
      break;
    }
    case Int2:
    {
      j = json{
        { "type", valType },
        { "value", { var.int2Value[0], var.int2Value[1] } }
      };
      break;
    }
    case Int3:
    {
      j = json{
        { "type", valType },
        { "value", { var.int3Value[0], var.int3Value[1], var.int3Value[2] } }
      };
      break;
    }
    case Int4:
    {
      j = json{
        { "type", valType },
        { "value", { var.int4Value[0], var.int4Value[1], var.int4Value[2], var.int4Value[3] } }
      };
      break;
    }
    case Float:
    {
      j = json{
        { "type", valType },
        { "value", var.intValue }
      };
      break;
    }
    case Float2:
    {
      j = json{
        { "type", valType },
        { "value", { var.float2Value[0], var.float2Value[1] } }
      };
      break;
    }
    case Float3:
    {
      j = json{
        { "type", valType },
        { "value", { var.float3Value[0], var.float3Value[1], var.float3Value[2] } }
      };
      break;
    }
    case Float4:
    {
      j = json{
        { "type", valType },
        { "value", { var.float4Value[0], var.float4Value[1], var.float4Value[2], var.float4Value[3] } }
      };
      break;
    }
    case ContextVar:
    case String:
    {
      j = json{
        { "type", valType },
        { "value", var.stringValue }
      };
      break;
    }
    case Color:
    {
      j = json{
        { "type", valType },
        { "value", { var.colorValue.r, var.colorValue.g, var.colorValue.b, var.colorValue.a } }
      };
      break;
    }
    case Image:
    {
      if(var.imageValue.data)
      {
        auto binsize = var.imageValue.width * var.imageValue.height * var.imageValue.channels;
        std::vector<uint8_t> buffer(binsize);
        memcpy(&buffer[0], var.imageValue.data, binsize);
        j = json{
          { "type", valType },
          { "width", var.imageValue.width },
          { "height", var.imageValue.height },
          { "channels", var.imageValue.channels },
          { "data", buffer }
        };
      }
      else
      {
        j = json{
          { "type", 0 },
          { "value", int(Continue) }
        };
      }
      break;
    }
    case Enum:
    {
      j = json{
        { "type", valType },
        { "value", int32_t(var.enumValue) },
        { "vendorId", var.enumVendorId },
        { "typeId", var.enumTypeId }
      };
      break;
    }
    case Seq:
    {
      std::vector<json> items;
      for(int i = 0; i < stbds_arrlen(var.seqValue); i++)
      {
        auto& v = var.seqValue[i];
        items.push_back(v);
      }
      j = json{
        { "type", valType },
        { "values", items }
      };
      break;
    }
    case Table:
    {
      std::vector<json> items;
      for(int i = 0; i < stbds_arrlen(var.tableValue); i++)
      {
        auto& v = var.tableValue[i];
        items.push_back(json{
          { "key", v.key },
          { "value", v.value }
        });
      }
      j = json{
        { "type", valType },
        { "values", items }
      };
      break;
    }
    case Chain:
    {
      if(var.chainValue && *var.chainValue)
      {
        j = json{
          { "type", valType },
          { "name", (*var.chainValue)->name }
        };
      }
      else
      {
        j = json{
          { "type", 0 },
          { "value", int(Continue) }
        };
      }
      break;
    }
    default:
    {
      j = json{
        { "type", 0 },
        { "value", int(Continue) }
      };
    }
  };
}

void from_json(const json& j, CBVar& var)
{
  auto valType = CBType(j.at("type").get<int>());
  switch (valType)
  {
    case None:
    {
      var.valueType = None;
      var.chainState = CBChainState(j.at("value").get<int>());
      break;
    }
    case Object:
    {
      auto vendorId = j.at("vendorId").get<int32_t>();
      auto typeId = j.at("typeId").get<int32_t>();
      if(!j.at("value").is_null())
      {
        auto value = j.at("value").get<std::string>();
        auto findIt = chainblocks::ObjectTypesRegister.find(std::make_tuple(vendorId, typeId));
        if(findIt != chainblocks::ObjectTypesRegister.end() && findIt->second.deserialize)
        {
          var.objectValue = findIt->second.deserialize(const_cast<CBString>(value.c_str()));
        }
        else
        {
          throw chainblocks::CBException("Failed to find a custom object deserializer.");
        }
      }
      else
      {
        var.objectValue = nullptr;
      }
      var.objectVendorId = vendorId;
      var.objectTypeId = typeId;
      break;
    }
    case Bool:
    {
      var.valueType = Bool;
      var.boolValue = j.at("value").get<bool>();
      break;
    }
    case Int:
    {
      var.valueType = Int;
      var.intValue = j.at("value").get<int64_t>();
      break;
    }
    case Int2:
    {
      var.valueType = Int2;
      var.int2Value[0] = j.at("value")[0].get<int64_t>();
      var.int2Value[1] = j.at("value")[1].get<int64_t>();
      break;
    }
    case Int3:
    {
      var.valueType = Int3;
      var.int3Value[0] = j.at("value")[0].get<int32_t>();
      var.int3Value[1] = j.at("value")[1].get<int32_t>();
      var.int3Value[2] = j.at("value")[2].get<int32_t>();
      break;
    }
    case Int4:
    {
      var.valueType = Int4;
      var.int4Value[0] = j.at("value")[0].get<int32_t>();
      var.int4Value[1] = j.at("value")[1].get<int32_t>();
      var.int4Value[2] = j.at("value")[2].get<int32_t>();
      var.int4Value[3] = j.at("value")[3].get<int32_t>();
      break;
    }
    case Float:
    {
      var.valueType = Float;
      var.intValue = j.at("value").get<double>();
      break;
    }
    case Float2:
    {
      var.valueType = Float2;
      var.float2Value[0] = j.at("value")[0].get<double>();
      var.float2Value[1] = j.at("value")[1].get<double>();
      break;
    }
    case Float3:
    {
      var.valueType = Float3;
      var.float3Value[0] = j.at("value")[0].get<float>();
      var.float3Value[1] = j.at("value")[1].get<float>();
      var.float3Value[2] = j.at("value")[2].get<float>();
      break;
    }
    case Float4:
    {
      var.valueType = Float4;
      var.float4Value[0] = j.at("value")[0].get<float>();
      var.float4Value[1] = j.at("value")[1].get<float>();
      var.float4Value[2] = j.at("value")[2].get<float>();
      var.float4Value[3] = j.at("value")[3].get<float>();
      break;
    }
    case ContextVar:
    {
      var.valueType = ContextVar;
      auto strVal = j.at("value").get<std::string>();
      var.stringValue = new char[strVal.length() + 1];
      strcpy((char*)var.stringValue, strVal.c_str());
      break;
    }
    case String:
    {
      var.valueType = String;
      auto strVal = j.at("value").get<std::string>();
      var.stringValue = new char[strVal.length() + 1];
      strcpy((char*)var.stringValue, strVal.c_str());
      break;
    }
    case Color:
    {
      var.valueType = Color;
      var.colorValue.r = j.at("value")[0].get<uint8_t>();
      var.colorValue.g = j.at("value")[1].get<uint8_t>();
      var.colorValue.b = j.at("value")[2].get<uint8_t>();
      var.colorValue.a = j.at("value")[3].get<uint8_t>();
      break;
    }
    case Image:
    {
      var.valueType = Image;
      var.imageValue.width = j.at("width").get<int32_t>();
      var.imageValue.height = j.at("height").get<int32_t>();
      var.imageValue.channels = j.at("channels").get<int32_t>();
      var.imageValue.data = nullptr;
      var.imageValue.alloc();
      auto buffer = j.at("data").get<std::vector<uint8_t>>();
      auto binsize = var.imageValue.width * var.imageValue.height * var.imageValue.channels;
      memcpy(var.imageValue.data, &buffer[0], binsize);
      break;
    }
    case Enum:
    {
      var.valueType = Enum;
      var.enumValue = CBEnum(j.at("value").get<int32_t>());
      var.enumVendorId = CBEnum(j.at("vendorId").get<int32_t>());
      var.enumTypeId = CBEnum(j.at("typeId").get<int32_t>());
      break;
    }
    case Seq:
    {
      var.valueType = Seq;
      auto items = j.at("values").get<std::vector<json>>();
      var.seqValue = nullptr;
      for(auto item : items)
      {
        stbds_arrpush(var.seqValue, item.get<CBVar>());
      }
      break;
    }
    case Table:
    {
      var.valueType = Seq;
      auto items = j.at("values").get<std::vector<json>>();
      var.tableValue = nullptr;
      stbds_shdefault(var.tableValue, CBVar());
      for(auto item : items)
      {
        auto key = item.at("key").get<std::string>();
        auto value = item.at("value").get<CBVar>();
        CBNamedVar named;
        named.key = new char[key.length() + 1];
        strcpy((char*)named.key, key.c_str());
        named.value = value;
        stbds_shputs(var.tableValue, named);
      }
      break;
    }
    case Chain:
    {
      var.valueType = Chain;
      auto chainName = j.at("name").get<std::string>();
      var.chainValue = &chainblocks::GlobalChains[chainName]; // might be null now, but might get filled after
      break;
    }
    default:
    {
      var = CBVar();
    }
  }
}

void to_json(json& j, const CBChainPtr& chain)
{
  std::vector<json> blocks;
  for(auto blk : chain->blocks)
  {
    std::vector<json> params;
    auto paramsDesc = blk->parameters(blk);
    for(int i = 0; i < stbds_arrlen(paramsDesc); i++)
    {
      auto& desc = paramsDesc[i];
      auto value = blk->getParam(blk, i);

      json param_obj = {
        { "name", desc.name },
        { "value", value }
      };
      
      params.push_back(param_obj);
    }

    json block_obj = {
      { "name", blk->name(blk) },
      { "params", params }
    };

    blocks.push_back(block_obj);
  }

  j = {
    { "blocks", blocks },
    { "name", chain->name },
    { "version", 0.1 },
  };
}

void from_json(const json& j, CBChainPtr& chain)
{
  auto chainName = j.at("name").get<std::string>();
  chain = new CBChain(chainName.c_str());
  chainblocks::GlobalChains["chainName"] = chain;

  auto jblocks = j.at("blocks");
  for(auto jblock : jblocks)
  {
    auto blkname = jblock.at("name").get<std::string>();
    auto blk = chainblocks::createBlock(blkname.c_str());
    if(!blk)
    {
      auto errmsg = "Failed to create block of type: " + std::string("blkname");
      throw chainblocks::CBException(errmsg.c_str());
    }
  
    // Setup
    blk->setup(blk);
    
    // Set params
    auto jparams = jblock.at("params");
    auto blkParams = blk->parameters(blk);
    for(auto jparam : jparams)
    {
      auto paramName = jparam.at("name").get<std::string>();
      auto value = jparam.at("value").get<CBVar>();
      for(auto i = 0; i < stbds_arrlen(blkParams); i++)
      {
        auto& paramInfo = blkParams[i];
        if(paramName == paramInfo.name)
        {
          blk->setParam(blk, i, value);
          break;
        }
      }
      // Assume block copied memory internally so we can clean up here!!!
      value.releaseMemory();
    }

    // From now on this chain owns the block
    chain->addBlock(blk);
  }
}

#ifdef TESTING
  static CBChain mainChain("MainChain");

  int main()
  {
    {
      CBRuntimeBlock blk = {};
    }
    
    {
      auto ublk = std::make_unique<CBRuntimeBlock>();
    }
    
    chainblocks::CurrentChain = &mainChain;
    chainblocks::start(chainblocks::CurrentChain, true);
    chainblocks::tick(chainblocks::CurrentChain);
    chainblocks::tick(chainblocks::CurrentChain);
    chainblocks::stop(chainblocks::CurrentChain);

    chainblocks::GlobalVariables.insert(std::make_pair("TestVar", CBVar(Restart)));
    auto tv = chainblocks::globalVariable(u8"TestVar");
    if(tv->valueType == None && tv->chainState == Restart)
    {
      printf("Map ok!\n");
    }

    auto tv2 = chainblocks::globalVariable("TestVar2");
    if(tv2->valueType == None && tv2->chainState == Continue)
    {
      printf("Map ok2!\n");
    }

    printf("Size: %d\n", sizeof(CBVar));
  }
#endif