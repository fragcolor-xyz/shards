#define STB_DS_IMPLEMENTATION 1
#define CHAINBLOCKS_RUNTIME 1
#define DLL_EXPORT 1
#include "runtime.hpp"
#include "blocks/core.hpp"
#include <cstdarg>

INITIALIZE_EASYLOGGINGPP

namespace chainblocks
{
  phmap::node_hash_map<std::string, CBBlockConstructor> BlocksRegister;
  phmap::node_hash_map<std::tuple<int32_t, int32_t>, CBObjectInfo> ObjectTypesRegister;
  phmap::node_hash_map<std::tuple<int32_t, int32_t>, CBEnumInfo> EnumTypesRegister;
  phmap::node_hash_map<std::string, CBVar> GlobalVariables;
  phmap::node_hash_map<std::string, CBCallback> ExitHooks;
  phmap::node_hash_map<std::string, CBChain*> GlobalChains;
  thread_local std::map<std::string, CBCallback> RunLoopHooks;
  thread_local CBChain* CurrentChain;

  static bool coreRegistered = false;
  static void registerCoreBlocks()
  {
    if(!coreRegistered)
    {
      // Do this here to prevent insanity loop
      coreRegistered = true;
      REGISTER_CORE_BLOCK(SetTableValue);
    }
  }
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

EXPORTED void __cdecl chainblocks_ThrowException(const char* errorText)
{
  throw chainblocks::CBException(errorText);
}

EXPORTED int __cdecl chainblocks_ContextState(CBContext* context)
{
  if(context->aborted)
    return 1;
  return 0;
}

EXPORTED CBVar __cdecl chainblocks_Suspend(double seconds)
{
  return chainblocks::suspend(seconds);
}

EXPORTED void __cdecl chainblocks_CloneVar(CBVar* dst, const CBVar* src)
{
  chainblocks::cloneVar(*dst, *src);
}

EXPORTED void __cdecl chainblocks_DestroyVar(CBVar* var)
{
  chainblocks::destroyVar(*var);
}

EXPORTED void __cdecl chainblocks_ActivateBlock(CBRuntimeBlock* block, CBContext* context, CBVar* input, CBVar* output)
{
  chainblocks::activateBlock(block, context, *input, *output);
}

EXPORTED void __cdecl chainblocks_Log(int level, const char* format, ...)
{
  auto temp = std::vector<char> {};
  auto length = std::size_t {63};
  std::va_list args;
  while (temp.size() <= length)
  {
    temp.resize(length + 1);
    va_start(args, format);
    const auto status = std::vsnprintf(temp.data(), temp.size(), format, args);
    va_end(args);
    if (status < 0)
      throw std::runtime_error {"string formatting error"};
    length = static_cast<std::size_t>(status);
  }
  std::string str(temp.data(), length);

  switch(level)
  {
    case CB_INFO:
    LOG(INFO) << "Info: " << str;
    break;
    case CB_DEBUG:
    LOG(INFO) << "Debug: " << str;
    break;
    case CB_TRACE:
    LOG(INFO) << "Trace: " << str;
    break;
  }
}

#ifdef __cplusplus
};
#endif

void to_json(json& j, const CBVar& var)
{
  auto valType = int(var.valueType);
  switch(var.valueType)
  {
    case Any:
    case EndOfBlittableTypes:
    {
      j = json{
        { "type", 0 },
        { "value", int(Continue) }
      };
      break;
    }
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
      auto vec = { var.payload.int16Value[0], var.payload.int16Value[1], var.payload.int16Value[2], var.payload.int16Value[3],
        var.payload.int16Value[4], var.payload.int16Value[5], var.payload.int16Value[6], var.payload.int16Value[7],
        var.payload.int16Value[8], var.payload.int16Value[9], var.payload.int16Value[10], var.payload.int16Value[11],
        var.payload.int16Value[12], var.payload.int16Value[13], var.payload.int16Value[14], var.payload.int16Value[15]
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
      j = json{
        { "type", valType },
        { "name", (var.payload.chainValue)->name }
      };
      break;
    }
    case Block:
    {
      auto blk = var.payload.blockValue;
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
      
      j = {
        { "type", valType },
        { "name", blk->name(blk) },
        { "params", params }
      };
      break;
    }
  };
}

void from_json(const json& j, CBVar& var)
{
  auto valType = CBType(j.at("type").get<int>());
  switch (valType)
  {
    case Any:
    case EndOfBlittableTypes:
    {
      var = {};
      break;
    }
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
      auto findIt = chainblocks::GlobalChains.find(chainName);
      if(findIt != chainblocks::GlobalChains.end())
      {
        var.payload.chainValue = findIt->second;
      }
      else
      {
        // create it anyway, deserialize when we can
        var.payload.chainValue = new CBChain(chainName.c_str());
        chainblocks::GlobalChains[chainName] = var.payload.chainValue;
      }
      break;
    }
    case Block:
    {
      var.valueType = Block;
      auto blkname = j.at("name").get<std::string>();
      auto blk = chainblocks::createBlock(blkname.c_str());
      if(!blk)
      {
        auto errmsg = "Failed to create block of type: " + std::string("blkname");
        throw chainblocks::CBException(errmsg.c_str());
      }
      var.payload.blockValue = blk;
    
      // Setup
      blk->setup(blk);
      
      // Set params
      auto jparams = j.at("params");
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
      break;
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
    { "looped", chain->looped },
    { "unsafe", chain->unsafe },
    { "version", 0.1 },
  };
}

void from_json(const json& j, CBChainPtr& chain)
{
  auto chainName = j.at("name").get<std::string>();
  auto findIt = chainblocks::GlobalChains.find(chainName);
  if(findIt != chainblocks::GlobalChains.end())
  {
    chain = findIt->second;
    // Need to clean it up for rewrite!
    chain->cleanup();
  }
  else
  {
    chain = new CBChain(chainName.c_str());
    chainblocks::GlobalChains["chainName"] = chain;
  }

  chain->looped = j.at("looped").get<bool>();
  chain->unsafe = j.at("unsafe").get<bool>();

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

bool matchTypes(CBTypeInfo& exposedType, CBTypeInfo& consumedType)
{
  if(consumedType.basicType == Any || consumedType.basicType == None)
    return true;
  
  if(exposedType.basicType != consumedType.basicType)
  {
    // Fail if basic type differs
    return false;
  }

  // if(exposedType.sequenced && !consumedType.sequenced)
  // {
  //   // Fail if we might output a sequenced value but input cannot deal with it
  //   return false;
  // }
  
  switch(exposedType.basicType)
  {
    case Object:
    {
      if(exposedType.objectVendorId != consumedType.objectVendorId || exposedType.objectTypeId != consumedType.objectTypeId)
      {
        return false;
      }
      break;
    }
    case Enum:
    {
      if(exposedType.enumVendorId != consumedType.enumVendorId || exposedType.enumTypeId != consumedType.enumTypeId)
      {
        return false;
      }
      break;
    }
    case Seq:
    {
      // TODO
      break;
    }
    default:
      return true;
  }
  return true;
}

void validateConnection(phmap::flat_hash_map<std::string, CBParameterInfo>& exposed, CBValidationCallback cb, CBRuntimeBlock* top, CBRuntimeBlock* bottom, void* userData)
{
  auto outputInfo = top->outputTypes(top);
  auto inputInfo = bottom->inputTypes(bottom);
  auto exposedVars = bottom->exposedVariables(bottom);
  auto consumedVar = bottom->consumedVariables(bottom);
  
  // make sure we have the vars we need
  for(auto i = 0; i < stbds_arrlen(consumedVar); i++)
  {
    auto& consumed_param = consumedVar[i];
    std::string name(consumed_param.name);
    auto findIt = exposed.find(name);
    if(findIt == exposed.end())
    {
      std::string err("Required consumed variable not found: " + name);
      cb(bottom, err.c_str(), false, userData);
    }
    
    // Validate types!
    // Ensure they match
    auto exposedTypes = findIt->second.valueTypes;
    auto requiredTyes = consumed_param.valueTypes;
    if(stbds_arrlen(exposedTypes) != stbds_arrlen(requiredTyes))
    {
      std::string err("Required consumed types do not match currently exposed ones: " + name);
      cb(bottom, err.c_str(), false, userData);
    }
    auto len = stbds_arrlen(exposedTypes);
    for(auto i = 0; i < len; i++)
    {
      auto& exposedType = exposedTypes[i];
      auto& consumedType = requiredTyes[i];

      if(!matchTypes(exposedType, consumedType))
      {
        std::string err("Required consumed types do not match currently exposed ones: " + name);
        cb(bottom, err.c_str(), false, userData);
      }
    }
  }

  // Add the vars we expose
  for(auto i = 0; i < stbds_arrlen(exposedVars); i++)
  {
    auto& exposed_param = exposedVars[i];
    std::string name(exposed_param.name);
    auto findIt = exposed.find(name);
    if(findIt != exposed.end())
    {
      // do we want to override it?, warn at least
      std::string err("Overriding already exposed variable: " + name);
      cb(bottom, err.c_str(), true, userData);
    }

    exposed[name] = exposed_param;
  }

  // Finally match input/output
  for(auto i = 0; i < stbds_arrlen(inputInfo); i++)
  {
    auto& inputType = inputInfo[i];
    // Go thru all of them and try match, exit if we do!
    for(auto i = 0; i < stbds_arrlen(outputInfo); i++)
    {
      auto& outputType = outputInfo[i];
      if(matchTypes(outputType, inputType))
      {
        return;
      }
    }
  }

  std::string err("Could not find a matching output type");
  cb(bottom, err.c_str(), false, userData);
}

void validateConnections(const CBChain* chain, CBValidationCallback callback, void* userData)
{
  phmap::flat_hash_map<std::string, CBParameterInfo> exposedVars;
  CBRuntimeBlock* previousBlock = nullptr;
  for(auto blk : chain->blocks)
  {
    if(previousBlock)
    {
      validateConnection(exposedVars, callback, previousBlock, blk, userData);
    }
    previousBlock = blk;
  }
}

void validateSetParam(CBRuntimeBlock* block, int index, CBVar& value, CBValidationCallback callback, void* userData)
{
  auto params = block->parameters(block);
  if(stbds_arrlen(params) <= index)
  {
    std::string err("Parameter index out of range");
    callback(block, err.c_str(), false, userData);
    return;
  }
  
  auto param = params[index];
  
  // Build a CBTypeInfo for the var
  CBTypeInfo varType;
  varType.basicType = value.valueType;
  switch(value.valueType)
  {
    case Object:
    {
      varType.objectVendorId = value.payload.objectVendorId;
      varType.objectTypeId = value.payload.objectTypeId;
      break;
    }
    case Enum:
    {
      varType.enumVendorId = value.payload.enumVendorId;
      varType.enumTypeId = value.payload.enumTypeId;
      break;
    }
    case Seq:
    {
      // TODO
      break;
    }
    default:
      break;
  };
  
  for(auto i = 0; i < stbds_arrlen(param.valueTypes); i++)
  {
    if(matchTypes(varType, param.valueTypes[i]))
    {
      return; // we are good just exit
    }
  }
  
  std::string err("Parameter not accepting this kind of variable");
  callback(block, err.c_str(), false, userData);
}

#ifdef TESTING
  static CBChain mainChain("MainChain");

  int main()
  {
    auto blk = chainblocks::createBlock("SetTableValue");
    LOG(INFO) << blk->name(blk);
    LOG(INFO) << blk->exposedVariables(blk);
  }
#endif