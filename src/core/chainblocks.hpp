#pragma once

/*
  TODO:
  Make this header C compatible when CHAINBLOCKS_RUNTIME is not defined.
*/

// Use only basic types and libs, we want full ABI compatibility between blocks
// Cannot afford to use any C++ std as any block maker should be free to use their versions

#include <stdint.h>
#include "3rdparty/DG_dynarr.h"
#include "3rdparty/gb_string.h"

// All the available types
enum CBType : uint8_t
{
  None,
  Any,
  Object,
  Enum,
  Bool,
  Int,
  Int2, // A vector of 2 ints
  Int3, // A vector of 3 ints
  Int4, // A vector of 4 ints
  Float,
  Float2, // A vector of 2 floats
  Float3, // A vector of 3 floats
  Float4, // A vector of 4 floats
  String,
  Color, // A vector of 4 uint8
  Image,
  Seq, 
  Chain, // sub chains, e.g. IF/ELSE
  ContextVar, // A string label to find from CBContext variables
};

enum CBChainState
{
  Continue, // Even if None returned, continue to next block
  Restart, // Restart the chain from the top
  Stop // Stop the chain execution
};

// Forward declarations
struct CBVar;
DA_TYPEDEF(CBVar, CBSeq);
struct CBChain;
typedef CBChain* CBChainPtr;
struct CBRuntimeBlock;
struct CBTypeInfo;
DA_TYPEDEF(CBTypeInfo, CBTypesInfo)
struct CBParameterInfo;
DA_TYPEDEF(CBParameterInfo, CBParametersInfo)
struct CBContext;

typedef void* CBPointer;
typedef int64_t CBInt;
typedef double CBFloat;
typedef bool CBBool;
typedef int32_t CBEnum;
typedef gbString CBString;
DA_TYPEDEF(const char*, CBStrings);

#if defined(__clang__) || defined(__GNUC__)
  typedef int64_t CBInt2 __attribute__((vector_size(16)));
  typedef int32_t CBInt3 __attribute__((vector_size(16)));
  typedef int32_t CBInt4 __attribute__((vector_size(16)));

  typedef double CBFloat2 __attribute__((vector_size(16)));
  typedef float CBFloat3 __attribute__((vector_size(16)));
  typedef float CBFloat4 __attribute__((vector_size(16)));
#else
  // TODO, this is not aligned correctly!
  struct CBInt2
  {
    int64_t elements[2];
  };

  struct CBInt3
  {
    int32_t elements[3];
  };

  struct CBInt4
  {
    int32_t elements[4];
  };

  struct CBFloat2
  {
    double elements[2];
  };

  struct CBFloat3
  {
    float elements[3];
  };

  struct CBFloat4
  {
    float elements[4];
  };
#endif

struct CBColor
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
};

struct CBImage
{
  int32_t width;
  int32_t height;
  int32_t channels;
  uint8_t* data;

  // Utility
  void alloc()
  {
    if(data)
      dealloc();

    auto binsize = width * height * channels;
    data = new uint8_t[binsize];
  }

  // Utility
  void dealloc()
  {
    if(data)
      delete[] data;
  }
};

struct CBTypeInfo
{
  CBType basicType;
  union
  {
    struct {
      int32_t objectVendorId;
      int32_t objectTypeId;
    };

    struct {
      int32_t enumVendorId;
      int32_t enumTypeId;
    };
  };
  bool sequenced;
};

typedef CBString (__cdecl *CBObjectSerializer)(CBPointer);
typedef CBPointer (__cdecl *CBObjectDeserializer)(CBString);
typedef CBPointer (__cdecl *CBObjectFree)(CBPointer);

struct CBObjectInfo
{
  const char* name;
  CBObjectSerializer serialize;
  CBObjectDeserializer deserialize;
  CBObjectFree free;
};

struct CBEnumInfo
{
  const char* name;
  CBStrings labels;
};

struct CBParameterInfo
{
  const char* name;
  const char* help;
  CBTypesInfo valueTypes;
  bool allowContext;
};

/*
  # Of CBVars and memory

  ## Specifically String and Seq types

  ### Blocks need to maintain their own garbage, in a way so that any reciver of CBVar/s will not have to worry about it.
  
  #### In the case of getParam:
    * the callee should allocate and preferably cache any String or Seq that needs to return.
    * the callers will just read and should not modify the contents.
  #### In the case of activate:
    * The input var memory is owned by the previous block.
    * The output var memory is owned by the activating block.
    * The activating block will have to manage any CBVar that puts in the stack or context variables as well!
  #### In the case of setParam:
    * If the block needs to store the String or Seq data it will then need to deep copy it.
    * Callers should free up any allocated memory.
*/ 

struct CBVar // will be 48 bytes, 16 aligned due to vectors
{
  CBVar(CBChainState state = Continue) : valueType(None), chainState(state) {}

  union 
  {
    CBChainState chainState;
    
    struct {
      CBPointer objectValue;
      int32_t objectVendorId;
      int32_t objectTypeId;
    };
    
    CBBool boolValue;
    
    CBInt intValue;
    CBInt2 int2Value;
    CBInt3 int3Value;
    CBInt4 int4Value;

    CBFloat floatValue;
    CBFloat2 float2Value;
    CBFloat3 float3Value;
    CBFloat4 float4Value;

    CBSeq seqValue;

    CBString stringValue;

    CBColor colorValue;
    
    CBImage imageValue;

    CBChainPtr* chainValue;

    struct {
      CBEnum enumValue;
      int32_t enumVendorId;
      int32_t enumTypeId;
    };
  };

  CBType valueType;

  uint8_t reserved[15];

  // Use with care, only if you know you own the memory, this is just utility
  void free()
  {
    if((valueType == String || valueType == ContextVar) && stringValue != nullptr)
    {
      gb_free_string(stringValue);
      stringValue = nullptr;
    }
    else if(valueType == Seq)
    {
      da_free(seqValue);
      da_init(seqValue);
    }
    else if(valueType == Image)
    {
      imageValue.dealloc();
    }
  }
};

typedef CBRuntimeBlock* (__cdecl *CBBlockConstructor)();
typedef void (__cdecl *CBOnRunLoopTick)();

typedef const char* (__cdecl *CBNameProc)(CBRuntimeBlock*);
typedef const char* (__cdecl *CBHelpProc)(CBRuntimeBlock*);

// Construction/Destruction
typedef void (__cdecl *CBSetupProc)(CBRuntimeBlock*);
typedef void (__cdecl *CBDestroyProc)(CBRuntimeBlock*);

typedef CBTypesInfo (__cdecl *CBInputTypesProc)(CBRuntimeBlock*);
typedef CBTypesInfo (__cdecl *CBOutputTypesProc)(CBRuntimeBlock*);

typedef CBParametersInfo (__cdecl *CBExposedVariablesProc)(CBRuntimeBlock*);
typedef CBParametersInfo (__cdecl *CBConsumedVariablesProc)(CBRuntimeBlock*);

typedef CBParametersInfo (__cdecl *CBParametersProc)(CBRuntimeBlock*);
typedef void (__cdecl *CBSetParamProc)(CBRuntimeBlock*, int, CBVar);
typedef CBVar (__cdecl *CBGetParamProc)(CBRuntimeBlock*, int);

// All those happen inside a coroutine
typedef void (__cdecl *CBPreChainProc)(CBRuntimeBlock*, CBContext*);
typedef CBVar (__cdecl *CBActivateProc)(CBRuntimeBlock*, CBContext*, CBVar);
typedef void (__cdecl *CBPostChainProc)(CBRuntimeBlock*, CBContext*);

// Generally when stop() is called
typedef void (__cdecl *CBCleanupProc)(CBRuntimeBlock*);

struct CBRuntimeBlock
{
  CBRuntimeBlock() : 
    name(nullptr), help(nullptr), setup(nullptr), destroy(nullptr),
    preChain(nullptr), postChain(nullptr), inputTypes(nullptr), outputTypes(nullptr),
    exposedVariables(nullptr), consumedVariables(nullptr), parameters(nullptr), setParam(nullptr),
    getParam(nullptr), activate(nullptr), cleanup(nullptr)
  {}
  
  CBRuntimeBlock(CBRuntimeBlock*& blk) 
  {
    name = blk->name;
    help = blk->help;
    setup = blk->setup;
    destroy = blk->destroy;
    preChain = blk->preChain;
    postChain = blk->postChain;
    inputTypes = blk->inputTypes;
    outputTypes = blk->outputTypes;
    exposedVariables = blk->exposedVariables;
    consumedVariables = blk->consumedVariables;
    parameters = blk->parameters;
    setParam = blk->setParam;
    getParam = blk->getParam;
    activate = blk->activate;
    cleanup = blk->cleanup;
  }

  CBNameProc name; // Returns the name of the block, do not free the string, generally const
  CBHelpProc help; // Returns the help text of the block, do not free the string, generally const

  CBSetupProc setup; // A one time construtor setup for the block
  CBDestroyProc destroy; // A one time finalizer for the block, blocks should also free all the memory in here!
  
  CBPreChainProc preChain; // Called inside the coro before a chain starts
  CBPostChainProc postChain; // Called inside the coro afer a chain ends
  
  CBInputTypesProc inputTypes;
  CBOutputTypesProc outputTypes;
  
  CBExposedVariablesProc exposedVariables;
  CBConsumedVariablesProc consumedVariables;

  CBParametersProc parameters;
  CBSetParamProc setParam; // Set a parameter, the block will copy the value, so if you allocated any memory you should free it
  CBGetParamProc getParam; // Gets a parameter, the block is the owner of any allocated stuff, DO NOT free them
  
  CBActivateProc activate;
  CBCleanupProc cleanup; // Called every time you stop a coroutine or sometimes internally to clean up the block state
};

#ifdef CHAINBLOCKS_RUNTIME
// C++ Mandatory from now!

// Since we build the runtime we are free to use any std and lib
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <iostream>
#include <ctime>
#include <thread>
#include <chrono>

// Required external dependency!
#include <boost/context/continuation.hpp>

// Included 3rdparty
#include "3rdparty/json.hpp"

#include <tuple>
namespace std{
  namespace
  {

    // Code from boost
    // Reciprocal of the golden ratio helps spread entropy
    //     and handles duplicates.
    // See Mike Seymour in magic-numbers-in-boosthash-combine:
    //     http://stackoverflow.com/questions/4948780

    template <class T>
    inline void hash_combine(std::size_t& seed, T const& v)
    {
        seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
    }

    // Recursive template code derived from Matthieu M.
    template <class Tuple, size_t Index = std::tuple_size<Tuple>::value - 1>
    struct HashValueImpl
    {
      static void apply(size_t& seed, Tuple const& tuple)
      {
        HashValueImpl<Tuple, Index-1>::apply(seed, tuple);
        hash_combine(seed, std::get<Index>(tuple));
      }
    };

    template <class Tuple>
    struct HashValueImpl<Tuple,0>
    {
      static void apply(size_t& seed, Tuple const& tuple)
      {
        hash_combine(seed, std::get<0>(tuple));
      }
    };
  }

  template <typename ... TT>
  struct hash<std::tuple<TT...>> 
  {
    size_t
    operator()(std::tuple<TT...> const& tt) const
    {                                              
      size_t seed = 0;                             
      HashValueImpl<std::tuple<TT...> >::apply(seed, tt);    
      return seed;                                 
    }                                              
  };
}

typedef boost::context::continuation CBCoro;

struct CBContext
{
  CBContext(CBCoro&& sink) : error(nullptr), restarted(false), aborted(false), continuation(std::move(sink))
  {
    da_init(stack);
  }

  std::unordered_map<std::string, CBVar> variables;
  CBSeq stack;
  CBString error;

  // Those 2 go together with CBVar chainstates restart and stop
  bool restarted;
  // Also used to cancel a chain
  bool aborted;

  // Used within the coro stack! (suspend, etc)
  CBCoro&& continuation;

  void setError(const char* errorMsg)
  {
    if(!error)
      error = gb_make_string(errorMsg);
    else
      error = gb_set_string(error, errorMsg);
  }
};

struct CBChain
{
  CBChain(const char* chain_name) : 
    name(chain_name),
    coro(nullptr),
    next(0),
    sleepSeconds(0.0),
    started(false),
    finished(false),
    returned(false),
    rootTickInput(CBVar()),
    finishedOutput(CBVar()),
    context(nullptr)
  {}

  ~CBChain()
  {
    for(auto blk : blocks)
    {
      blk->destroy(blk);
      //blk is responsible to free itself, as they might use any allocation strategy they wish!
    }
  }

  std::string name;

  CBCoro* coro;
  clock_t next;
  double sleepSeconds;
  
  // we could simply null check coro but actually some chains (sub chains), will run without a coro within the root coro so we need this too
  bool started;
  
  // this gets cleared before every runChain and set after every runChain
  bool finished;
  
  // when running as coro if actually the coro lambda exited
  bool returned;

  CBVar rootTickInput;
  CBVar finishedOutput;
  
  CBContext* context;
  std::vector<CBRuntimeBlock*> blocks;

  // Also the chain takes ownership of the block!
  void addBlock(CBRuntimeBlock* blk)
  {
    blocks.push_back(blk);
  }

  // Also removes ownership of the block
  void removeBlock(CBRuntimeBlock* blk)
  {
    auto findIt = std::find(blocks.begin(), blocks.end(), blk);
    if(findIt != blocks.end())
    {
      blocks.erase(findIt);
    }
  }
};

#ifdef _WIN32
# ifdef DLL_EXPORT
  #define EXPORTED  __declspec(dllexport)
# else
  #define EXPORTED  __declspec(dllimport)
# endif
#else
  #define EXPORTED
#endif

#ifdef __cplusplus
extern "C" {
#endif

// The runtime (even if it is an exe), will export the following, they need to be available in order to load and work with blocks collections within dlls

EXPORTED void __cdecl chainblocks_RegisterBlock(const char* fullName, CBBlockConstructor constructor);
EXPORTED void __cdecl chainblocks_RegisterObjectType(int32_t vendorId, int32_t typeId, CBObjectInfo info);
EXPORTED void __cdecl chainblocks_RegisterEnumType(int32_t vendorId, int32_t typeId, CBEnumInfo info);
EXPORTED void __cdecl chainblocks_RegisterRunLoopCallback(const char* eventName, CBOnRunLoopTick callback);
EXPORTED void __cdecl chainblocks_UnregisterRunLoopCallback(const char* eventName);

EXPORTED CBVar* __cdecl chainblocks_ContextVariable(CBContext* context, const char* name);
EXPORTED void __cdecl chainblocks_SetError(CBContext* context, const char* errorText);

EXPORTED CBVar __cdecl chainblocks_Suspend(double seconds);

#ifdef __cplusplus
};
#endif

namespace chainblocks
{
  extern std::unordered_map<std::string, CBBlockConstructor> BlocksRegister;
  extern std::unordered_map<std::tuple<int32_t, int32_t>, CBObjectInfo> ObjectTypesRegister;
  extern std::unordered_map<std::tuple<int32_t, int32_t>, CBEnumInfo> EnumTypesRegister;
  extern std::unordered_map<std::string, CBVar> GlobalVariables;
  extern std::unordered_map<std::string, CBOnRunLoopTick> RunLoopHooks;
  extern std::unordered_map<std::string, CBChain*> GlobalChains;
  extern thread_local CBChain* CurrentChain;

  static CBRuntimeBlock* createBlock(const char* name);

  struct CBException : public std::exception
  {
    CBException(const char* errmsg) : errorMessage(errmsg) {}

    const char * what () const throw ()
    {
      return errorMessage;
    }

    const char* errorMessage;
  };
};

using json = nlohmann::json;
// The following procedures implement json.hpp protocol in order to allow easy integration! they must stay outside the namespace!
static void to_json(json& j, const CBVar& var)
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
      for(int i = 0; i < da_count(var.seqValue); i++)
      {
        auto v = da_get(var.seqValue, i);
        items.push_back(v);
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

static void from_json(const json& j, CBVar& var)
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
      var.stringValue = gb_make_string(strVal.c_str());
      break;
    }
    case String:
    {
      var.valueType = String;
      auto strVal = j.at("value").get<std::string>();
      var.stringValue = gb_make_string(strVal.c_str());
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
      da_init(var.seqValue);
      for(auto item : items)
      {
        da_push(var.seqValue, item.get<CBVar>());
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

static void to_json(json& j, const CBChainPtr& chain)
{
  std::vector<json> blocks;
  for(auto blk : chain->blocks)
  {
    std::vector<json> params;
    auto paramsDesc = blk->parameters(blk);
    for(int i = 0; i < da_count(paramsDesc); i++)
    {
      auto desc = da_getptr(paramsDesc, i);
      auto value = blk->getParam(blk, i);

      json param_obj = {
        { "name", desc->name },
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

static void from_json(const json& j, CBChainPtr& chain)
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
      for(auto i = 0; i < da_count(blkParams); i++)
      {
        auto paramInfo = da_getptr(blkParams, i);
        if(paramName == paramInfo->name)
        {
          blk->setParam(blk, i, value);
          break;
        }
      }
      value.free();
    }

    // From now on this chain owns the block
    chain->addBlock(blk);
  }
}

namespace chainblocks
{
  static void setCurrentChain(CBChain* chain)
  {
    CurrentChain = chain;
  }

  static CBChain* getCurrentChain()
  {
    return CurrentChain;
  }

  static void registerChain(CBChain* chain)
  {
    chainblocks::GlobalChains[chain->name] = chain;
  }

  static void unregisterChain(CBChain* chain)
  {
    auto findIt = chainblocks::GlobalChains.find(chain->name);
    if(findIt != chainblocks::GlobalChains.end())
    {
      chainblocks::GlobalChains.erase(findIt);
    }
  }

  static void registerBlock(const char* fullName, CBBlockConstructor constructor)
  {
    auto cname = std::string(fullName);
    auto findIt = BlocksRegister.find(cname);
    if(findIt == BlocksRegister.end())
    {
      BlocksRegister.insert(std::make_pair(cname, constructor));
      std::cout << "added block: " << cname << "\n";
    }
    else
    {
      BlocksRegister[cname] = constructor;
      std::cout << "overridden block: " << cname << "\n";
    }
  }

  static void registerObjectType(int32_t vendorId, int32_t typeId, CBObjectInfo info)
  {
    auto tup = std::make_tuple(vendorId, typeId);
    auto typeName = std::string(info.name);
    auto findIt = ObjectTypesRegister.find(tup);
    if(findIt == ObjectTypesRegister.end())
    {
      ObjectTypesRegister.insert(std::make_pair(tup, info));
      std::cout << "added object type: " << typeName << "\n";
    }
    else
    {
      ObjectTypesRegister[tup] = info;
      std::cout << "overridden object type: " << typeName << "\n";
    }
  }

  static void registerEnumType(int32_t vendorId, int32_t typeId, CBEnumInfo info)
  {
    auto tup = std::make_tuple(vendorId, typeId);
    auto typeName = std::string(info.name);
    auto findIt = ObjectTypesRegister.find(tup);
    if(findIt == ObjectTypesRegister.end())
    {
      EnumTypesRegister.insert(std::make_pair(tup, info));
      std::cout << "added object type: " << typeName << "\n";
    }
    else
    {
      EnumTypesRegister[tup] = info;
      std::cout << "overridden object type: " << typeName << "\n";
    }
  }

  static void registerRunLoopCallback(const char* eventName, CBOnRunLoopTick callback)
  {
    chainblocks::RunLoopHooks[eventName] = callback;
  }

  static void unregisterRunLoopCallback(const char* eventName)
  {
    auto findIt = chainblocks::RunLoopHooks.find(eventName);
    if(findIt != chainblocks::RunLoopHooks.end())
    {
      chainblocks::RunLoopHooks.erase(findIt);
    }
  }

  static CBVar* globalVariable(const char* name)
  {
    CBVar& v = GlobalVariables[name];
    return &v;
  }

  static bool hasGlobalVariable(const char* name)
  {
    auto findIt = GlobalVariables.find(name);
    if(findIt == GlobalVariables.end())
      return false;
    return true;
  }

  static CBVar* contextVariable(CBContext* ctx, const char* name)
  {
    CBVar& v = ctx->variables[name];
    return &v;
  }

  static CBRuntimeBlock* createBlock(const char* name)
  {
    auto it = BlocksRegister.find(name);
    if(it == BlocksRegister.end())
    {
      return nullptr;
    }

    auto blkp = it->second();
    return blkp;
  }

  static std::tuple<bool, CBVar> runChain(CBChain* chain, CBContext* context, CBVar chainInput)
  {
    chain->started = true;
    chain->context = context;
    CBVar previousOutput;
    auto previousChain = CurrentChain;
    CurrentChain = chain;

    for(auto blk : chain->blocks)
    {
      try
      {
        blk->preChain(blk, context);
      }
      catch(const std::exception& e)
      {
        std::cerr << "Pre chain failure, failed block: " << std::string(blk->name(blk)) << "\n";
        if(context->error)
          std::cerr << "Last error: " << std::string(context->error) << "\n";
        std::cerr << e.what() << "\n";
        return { false, {} };
      }
      catch(...)
      {
        std::cerr << "Pre chain failure, failed block: " << std::string(blk->name(blk)) << "\n";
        if(context->error)
          std::cerr << "Last error: " << std::string(context->error) << "\n";
        return { false, {} };
      }
    }

    #define runChainPOSTCHAIN for(auto blk : chain->blocks)\
    {\
      try\
      {\
        blk->postChain(blk, context);\
      }\
      catch(const std::exception& e)\
      {\
        std::cerr << "Post chain failure, failed block: " << std::string(blk->name(blk)) << "\n";\
        if(context->error)\
          std::cerr << "Last error: " << std::string(context->error) << "\n";\
        std::cerr << e.what() << "\n";\
        return { false, previousOutput };\
      }\
      catch(...)\
      {\
        std::cerr << "Post chain failure, failed block: " << std::string(blk->name(blk)) << "\n";\
        if(context->error)\
          std::cerr << "Last error: " << std::string(context->error) << "\n";\
        return { false, previousOutput };\
      }\
    }

    for(auto blk : chain->blocks)
    {
      try
      {
        #if 0
          std::cout << "Activating block: " << std::string(blk->name(blk)) << "\n";
        #endif

        // Pass chain root input every time we find None, this allows a long chain to re-process the root input if wanted!
        auto input = previousOutput.valueType == None ? chainInput : previousOutput;
        previousOutput = blk->activate(blk, context, input);

        if(previousOutput.valueType == None)
        {
          switch(previousOutput.chainState)
          {
            case Restart:
            {
              runChainPOSTCHAIN
              return { true, previousOutput };
            }
            case Stop:
            {
              // Print errors if any, we might have stopped because of some error!
              if(context->error)
                std::cerr << "Last error: " << std::string(context->error) << "\n";
              runChainPOSTCHAIN
              return { false, previousOutput };
            }
          }
        }
      }
      catch(const std::exception& e)
      {
        std::cerr << "Block activation error, failed block: " << std::string(blk->name(blk)) << "\n";
        if(context->error)
          std::cerr << "Last error: " << std::string(context->error) << "\n";
        std::cerr << e.what() << "\n";;
        runChainPOSTCHAIN
        return { false, previousOutput };
      }
      catch(...)
      {
        std::cerr << "Block activation error, failed block: " << std::string(blk->name(blk)) << "\n";
        if(context->error)
          std::cerr << "Last error: " << std::string(context->error) << "\n";
        runChainPOSTCHAIN
        return { false, previousOutput };
      }
    }

    runChainPOSTCHAIN
    return { true, previousOutput };
  }

  static void start(CBChain* chain, bool loop = false)
  {
    chain->coro = new CBCoro(boost::context::callcc([&chain, &loop](boost::context::continuation&& sink)
    {
      // Need to copy those in this stack!
      CBChain* thisChain = chain;
      bool looped = loop;
      // Reset return state
      thisChain->returned = false;
      // Create a new context and copy the sink in
      CBContext context(std::move(sink));

      auto running = true;
      while(running)
      {
        running = looped;
        context.restarted = false; // Remove restarted flag

        // Reset len to 0 of the stack
        da_clear(context.stack);
        
        thisChain->finished = false; // Reset finished flag

        auto runRes = runChain(thisChain, &context, thisChain->rootTickInput);
        thisChain->finished = true;
        thisChain->finishedOutput = std::get<1>(runRes);
        if(!std::get<0>(runRes))
        {
          context.aborted = true;
          break;
        }
        
        if(looped) 
        {
          // Ensure no while(true), yield anyway every run
          thisChain->sleepSeconds = 0.0;
          context.continuation = context.continuation.resume();
          // This is delayed upon continuation!!
          if(context.aborted)
            break;
        }
      }

      // Completely free the stack
      da_free(context.stack);

      // Free any error we might have, internally there is a null check!
      gb_free_string(context.error);
      
      // Need to take care that we might have stopped the chain very early due to errors and the next eventual stop() should avoid resuming
      thisChain->returned = true;
      return std::move(context.continuation);
    }));
  }

  static CBVar stop(CBChain* chain)
  {
    // notice if we called start, started is going to happen before any suspensions
    if(chain->started) 
    {
      if(chain->coro)
      {
        // Run until exit if alive, need to propagate to all suspended blocks!
        if((*chain->coro) && !chain->returned)
        {
          // Push current chain
          auto previous = chainblocks::CurrentChain;
          chainblocks::CurrentChain = chain;

          // set abortion flag, we always have a context in this case
          chain->context->aborted = true;
          
          // BIG Warning: chain->context existed in the coro stack!!!
          // after this resume chain->context is trash!
          chain->coro->resume();
          
          // Pop current chain
          chainblocks::CurrentChain = previous;
        }

        // delete also the coro ptr
        delete chain->coro;
        chain->coro = nullptr;
      }

      // Run cleanup on all blocks, prepare them for a new start if necessary
      for(auto blk : chain->blocks)
      {
        try
        {
          blk->cleanup(blk);
        }
        catch(const std::exception& e)
        {
          std::cerr << "Block cleanup error, failed block: " << std::string(blk->name(blk)) << "\n";
          std::cerr << e.what() << '\n';
        }
        catch(...)
        {
          std::cerr << "Block cleanup error, failed block: " << std::string(blk->name(blk)) << "\n";
        }
      }

      chain->started = false;

      return chain->finishedOutput;
    }

    return CBVar();
  }

  static CBVar suspend(double seconds)
  {
    auto current = chainblocks::CurrentChain;
    current->sleepSeconds = seconds;
    current->context->continuation = current->context->continuation.resume();
    if(current->context->restarted)
      return CBVar(Restart);
    else if(current->context->aborted)
      return CBVar(Stop);
    return CBVar(Continue);
  }

  static void tick(CBChain* chain, CBVar rootInput = CBVar())
  {
    if(!chain->coro || !(*chain->coro) || chain->returned)
      return; // check if not null and bool operator also to see if alive!
    
    auto now = std::clock();
    if(now >= chain->next)
    {
      auto previousChain = chainblocks::CurrentChain;
      chainblocks::CurrentChain = chain;
      
      chain->rootTickInput = rootInput;
      *chain->coro = chain->coro->resume();
      
      chainblocks::CurrentChain = previousChain;
      
      if(chain->sleepSeconds <= 0)
      {
        chain->next = now; //Ensure it will execute exactly the next tick
      }
      else
      {
        chain->next = now + clock_t(chain->sleepSeconds * double(CLOCKS_PER_SEC));
      }
    }
  } 

  static std::string store(CBVar var) 
  { 
    json jsonVar = var;
    return jsonVar.dump();
  }

  static void load(CBVar& var, const char* jsonStr)
  {
    auto j = json::parse(jsonStr);
    var = j.get<CBVar>();
  }

  static std::string store(CBChainPtr chain) 
  { 
    json jsonChain = chain;
    return jsonChain.dump(); 
  }

  static void load(CBChainPtr& chain, const char* jsonStr)
  {
    auto j = json::parse(jsonStr);
    chain = j.get<CBChainPtr>();
  }

  static void sleep(double seconds = -1.0)
  {
    //negative = no sleep, just run callbacks
    if(seconds >= 0)
    {
      std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
    }

    // Run loop callbacks after sleeping
    for(auto& cbinfo : RunLoopHooks)
    {
      if(cbinfo.second)
      {
        cbinfo.second();
      }
    }
  }
};
#endif