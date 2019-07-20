#define STB_DS_IMPLEMENTATION 1
#define CHAINBLOCKS_RUNTIME 1
#define DLL_EXPORT 1
#include "runtime.hpp"

namespace chainblocks
{
  phmap::node_hash_map<std::string, CBBlockConstructor> BlocksRegister;
  phmap::node_hash_map<std::tuple<int32_t, int32_t>, CBObjectInfo> ObjectTypesRegister;
  phmap::node_hash_map<std::tuple<int32_t, int32_t>, CBEnumInfo> EnumTypesRegister;
  phmap::node_hash_map<std::string, CBVar> GlobalVariables;
  phmap::node_hash_map<std::string, CBCallback> RunLoopHooks;
  phmap::node_hash_map<std::string, CBCallback> ExitHooks;
  phmap::node_hash_map<std::string, CBChain*> GlobalChains;
  thread_local CBChain* CurrentChain;
};

// Utility
void dealloc(CBImage& self)
{
  if(self.data)
  {
    delete[] self.data;
    self.data = nullptr;
  }
}

// Utility
void alloc(CBImage& self)
{
  if(self.data)
    dealloc(self);
  
  auto binsize = self.width * self.height * self.channels;
  self.data = new uint8_t[binsize];
}

void releaseMemory(CBVar& self)
{
  if((self.valueType == String || self.valueType == ContextVar) && self.payload.stringValue != nullptr)
  {
    delete[] self.payload.stringValue;
    self.payload.stringValue = nullptr;
  }
  else if(self.valueType == Seq && self.payload.seqValue)
  {
    for(auto i = 0; i < stbds_arrlen(self.payload.seqValue); i++)
    {
      releaseMemory(self.payload.seqValue[i]);
    }
    stbds_arrfree(self.payload.seqValue);
    self.payload.seqValue = nullptr;
  }
  else if(self.valueType == Table && self.payload.tableValue)
  {
    for(auto i = 0; i < stbds_shlen(self.payload.tableValue); i++)
    {
      delete[] self.payload.tableValue[i].key;
      releaseMemory(self.payload.tableValue[i].value);
    }
    stbds_shfree(self.payload.tableValue);
    self.payload.tableValue = nullptr;
  }
  else if(self.valueType == Image)
  {
    dealloc(self.payload.imageValue);
  }
}

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

EXPORTED void __cdecl chainblocks_RegisterRunLoopCallback(const char* eventName, CBCallback callback)
{
  chainblocks::registerRunLoopCallback(eventName, callback);
}

EXPORTED void __cdecl chainblocks_UnregisterRunLoopCallback(const char* eventName)
{
  chainblocks::unregisterRunLoopCallback(eventName);
}

EXPORTED void __cdecl chainblocks_RegisterExitCallback(const char* eventName, CBCallback callback)
{
  chainblocks::registerExitCallback(eventName, callback);
}

EXPORTED void __cdecl chainblocks_UnregisterExitCallback(const char* eventName)
{
  chainblocks::unregisterExitCallback(eventName);
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

EXPORTED void __cdecl chainblocks_VarCopy(CBVar* dst, const CBVar* src)
{
  chainblocks::cloneVar(*dst, *src);
}

EXPORTED void __cdecl chainblocks_DestroyVar(CBVar* var)
{
  chainblocks::destroyVar(*var);
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
        { "value", int(var.payload.chainState) }
      };
      break;
    }
    case Object:
    {
      auto findIt = chainblocks::ObjectTypesRegister.find(std::make_tuple(var.payload.objectVendorId, var.payload.objectTypeId));
      if(findIt != chainblocks::ObjectTypesRegister.end() && findIt->second.serialize)
      {
        j = json{
          { "type", valType },
          { "vendorId", var.payload.objectVendorId },
          { "typeId", var.payload.objectTypeId },
          { "value", findIt->second.serialize(var.payload.objectValue) }
        };
      }
      else
      {
        j = json{
          { "type", valType },
          { "vendorId", var.payload.objectVendorId },
          { "typeId", var.payload.objectTypeId },
          { "value", nullptr }
        };
      }
      break;
    }
    case Bool:
    {
      j = json{
        { "type", valType },
        { "value", var.payload.boolValue }
      };
      break;
    }
    case Int:
    {
      j = json{
        { "type", valType },
        { "value", var.payload.intValue }
      };
      break;
    }
    case Int2:
    {
      auto vec = { var.payload.int2Value[0], var.payload.int2Value[1] };
      j = json{
        { "type", valType },
        { "value", vec }
      };
      break;
    }
    case Int3:
    {
      auto vec = { var.payload.int3Value[0], var.payload.int3Value[1], var.payload.int3Value[2] };
      j = json{
        { "type", valType },
        { "value", vec }
      };
      break;
    }
    case Int4:
    {
      auto vec = { var.payload.int4Value[0], var.payload.int4Value[1], var.payload.int4Value[2], var.payload.int4Value[3] };
      j = json{
        { "type", valType },
        { "value", vec }
      };
      break;
    }
    case Int8:
    {
      auto vec = { var.payload.int8Value[0], var.payload.int8Value[1], var.payload.int8Value[2], var.payload.int8Value[3],
        var.payload.int8Value[4], var.payload.int8Value[5], var.payload.int8Value[6], var.payload.int8Value[7]
      };
      j = json{
        { "type", valType },
        { "value", vec }
      };
      break;
    }
    case Int16:
    {
      auto vec = { var.payload.int4Value[0], var.payload.int4Value[1], var.payload.int4Value[2], var.payload.int4Value[3],
        var.payload.int4Value[4], var.payload.int4Value[5], var.payload.int4Value[6], var.payload.int4Value[7],
        var.payload.int4Value[8], var.payload.int4Value[9], var.payload.int4Value[10], var.payload.int4Value[11],
        var.payload.int4Value[12], var.payload.int4Value[13], var.payload.int4Value[14], var.payload.int4Value[15]
      };
      j = json{
        { "type", valType },
        { "value", vec }
      };
      break;
    }
    case Float:
    {
      j = json{
        { "type", valType },
        { "value", var.payload.intValue }
      };
      break;
    }
    case Float2:
    {
      auto vec = { var.payload.float2Value[0], var.payload.float2Value[1] };
      j = json{
        { "type", valType },
        { "value", vec }
      };
      break;
    }
    case Float3:
    {
      auto vec = { var.payload.float3Value[0], var.payload.float3Value[1], var.payload.float3Value[2] };
      j = json{
        { "type", valType },
        { "value", vec }
      };
      break;
    }
    case Float4:
    {
      auto vec = { var.payload.float4Value[0], var.payload.float4Value[1], var.payload.float4Value[2], var.payload.float4Value[3] };
      j = json{
        { "type", valType },
        { "value", vec }
      };
      break;
    }
    case ContextVar:
    case String:
    {
      j = json{
        { "type", valType },
        { "value", var.payload.stringValue }
      };
      break;
    }
    case Color:
    {
      j = json{
        { "type", valType },
        { "value", { var.payload.colorValue.r, var.payload.colorValue.g, var.payload.colorValue.b, var.payload.colorValue.a } }
      };
      break;
    }
    case Image:
    {
      if(var.payload.imageValue.data)
      {
        auto binsize = var.payload.imageValue.width * var.payload.imageValue.height * var.payload.imageValue.channels;
        std::vector<uint8_t> buffer(binsize);
        memcpy(&buffer[0], var.payload.imageValue.data, binsize);
        j = json{
          { "type", valType },
          { "width", var.payload.imageValue.width },
          { "height", var.payload.imageValue.height },
          { "channels", var.payload.imageValue.channels },
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
        { "value", int32_t(var.payload.enumValue) },
        { "vendorId", var.payload.enumVendorId },
        { "typeId", var.payload.enumTypeId }
      };
      break;
    }
    case Seq:
    {
      std::vector<json> items;
      for(int i = 0; i < stbds_arrlen(var.payload.seqValue); i++)
      {
        auto& v = var.payload.seqValue[i];
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
      for(int i = 0; i < stbds_arrlen(var.payload.tableValue); i++)
      {
        auto& v = var.payload.tableValue[i];
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
      if(var.payload.chainValue && *var.payload.chainValue)
      {
        j = json{
          { "type", valType },
          { "name", (*var.payload.chainValue)->name }
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
      var.payload.chainState = CBChainState(j.at("value").get<int>());
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
          var.payload.objectValue = findIt->second.deserialize(const_cast<CBString>(value.c_str()));
        }
        else
        {
          throw chainblocks::CBException("Failed to find a custom object deserializer.");
        }
      }
      else
      {
        var.payload.objectValue = nullptr;
      }
      var.payload.objectVendorId = vendorId;
      var.payload.objectTypeId = typeId;
      break;
    }
    case Bool:
    {
      var.valueType = Bool;
      var.payload.boolValue = j.at("value").get<bool>();
      break;
    }
    case Int:
    {
      var.valueType = Int;
      var.payload.intValue = j.at("value").get<int64_t>();
      break;
    }
    case Int2:
    {
      var.valueType = Int2;
      var.payload.int2Value[0] = j.at("value")[0].get<int64_t>();
      var.payload.int2Value[1] = j.at("value")[1].get<int64_t>();
      break;
    }
    case Int3:
    {
      var.valueType = Int3;
      var.payload.int3Value[0] = j.at("value")[0].get<int32_t>();
      var.payload.int3Value[1] = j.at("value")[1].get<int32_t>();
      var.payload.int3Value[2] = j.at("value")[2].get<int32_t>();
      break;
    }
    case Int4:
    {
      var.valueType = Int4;
      var.payload.int4Value[0] = j.at("value")[0].get<int32_t>();
      var.payload.int4Value[1] = j.at("value")[1].get<int32_t>();
      var.payload.int4Value[2] = j.at("value")[2].get<int32_t>();
      var.payload.int4Value[3] = j.at("value")[3].get<int32_t>();
      break;
    }
    case Int8:
    {
      var.valueType = Int8;
      for(auto i = 0; i < 8; i++)
      {
        var.payload.int8Value[i] = j.at("value")[i].get<int16_t>();  
      }
      break;
    }
    case Int16:
    {
      var.valueType = Int16;
      for(auto i = 0; i < 16; i++)
      {
        var.payload.int16Value[i] = j.at("value")[i].get<int8_t>();  
      }
      break;
    }
    case Float:
    {
      var.valueType = Float;
      var.payload.intValue = j.at("value").get<double>();
      break;
    }
    case Float2:
    {
      var.valueType = Float2;
      var.payload.float2Value[0] = j.at("value")[0].get<double>();
      var.payload.float2Value[1] = j.at("value")[1].get<double>();
      break;
    }
    case Float3:
    {
      var.valueType = Float3;
      var.payload.float3Value[0] = j.at("value")[0].get<float>();
      var.payload.float3Value[1] = j.at("value")[1].get<float>();
      var.payload.float3Value[2] = j.at("value")[2].get<float>();
      break;
    }
    case Float4:
    {
      var.valueType = Float4;
      var.payload.float4Value[0] = j.at("value")[0].get<float>();
      var.payload.float4Value[1] = j.at("value")[1].get<float>();
      var.payload.float4Value[2] = j.at("value")[2].get<float>();
      var.payload.float4Value[3] = j.at("value")[3].get<float>();
      break;
    }
    case ContextVar:
    {
      var.valueType = ContextVar;
      auto strVal = j.at("value").get<std::string>();
      var.payload.stringValue = new char[strVal.length() + 1];
      strcpy((char*)var.payload.stringValue, strVal.c_str());
      break;
    }
    case String:
    {
      var.valueType = String;
      auto strVal = j.at("value").get<std::string>();
      var.payload.stringValue = new char[strVal.length() + 1];
      strcpy((char*)var.payload.stringValue, strVal.c_str());
      break;
    }
    case Color:
    {
      var.valueType = Color;
      var.payload.colorValue.r = j.at("value")[0].get<uint8_t>();
      var.payload.colorValue.g = j.at("value")[1].get<uint8_t>();
      var.payload.colorValue.b = j.at("value")[2].get<uint8_t>();
      var.payload.colorValue.a = j.at("value")[3].get<uint8_t>();
      break;
    }
    case Image:
    {
      var.valueType = Image;
      var.payload.imageValue.width = j.at("width").get<int32_t>();
      var.payload.imageValue.height = j.at("height").get<int32_t>();
      var.payload.imageValue.channels = j.at("channels").get<int32_t>();
      var.payload.imageValue.data = nullptr;
      alloc(var.payload.imageValue);
      auto buffer = j.at("data").get<std::vector<uint8_t>>();
      auto binsize = var.payload.imageValue.width * var.payload.imageValue.height * var.payload.imageValue.channels;
      memcpy(var.payload.imageValue.data, &buffer[0], binsize);
      break;
    }
    case Enum:
    {
      var.valueType = Enum;
      var.payload.enumValue = CBEnum(j.at("value").get<int32_t>());
      var.payload.enumVendorId = CBEnum(j.at("vendorId").get<int32_t>());
      var.payload.enumTypeId = CBEnum(j.at("typeId").get<int32_t>());
      break;
    }
    case Seq:
    {
      var.valueType = Seq;
      auto items = j.at("values").get<std::vector<json>>();
      var.payload.seqValue = nullptr;
      for(auto item : items)
      {
        stbds_arrpush(var.payload.seqValue, item.get<CBVar>());
      }
      break;
    }
    case Table:
    {
      var.valueType = Seq;
      auto items = j.at("values").get<std::vector<json>>();
      var.payload.tableValue = nullptr;
      stbds_shdefault(var.payload.tableValue, CBVar());
      for(auto item : items)
      {
        auto key = item.at("key").get<std::string>();
        auto value = item.at("value").get<CBVar>();
        CBNamedVar named;
        named.key = new char[key.length() + 1];
        strcpy((char*)named.key, key.c_str());
        named.value = value;
        stbds_shputs(var.payload.tableValue, named);
      }
      break;
    }
    case Chain:
    {
      var.valueType = Chain;
      auto chainName = j.at("name").get<std::string>();
      var.payload.chainValue = &chainblocks::GlobalChains[chainName]; // might be null now, but might get filled after
      break;
    }
    default:
    {
      var = {};
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
      
      if(value.valueType != None)
      {
        for(auto i = 0; i < stbds_arrlen(blkParams); i++)
        {
          auto& paramInfo = blkParams[i];
          if(paramName == paramInfo.name)
          {
            blk->setParam(blk, i, value);
            break;
          }
        }
      }

      // Assume block copied memory internally so we can clean up here!!!
      releaseMemory(value);
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