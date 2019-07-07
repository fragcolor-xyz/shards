#pragma once

/*
  TODO:
  Make this header C compatible when CHAINBLOCKS_RUNTIME is not defined.
*/

// Use only basic types and libs, we want full ABI compatibility between blocks
// Cannot afford to use any C++ std as any block maker should be free to use their versions

#include <stdint.h>
#include "DG_dynarr.h"
#include "gb_string.h"

// All the available types
enum CBType : uint8_t
{
  None,
  Any,
  Object,
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
  BoolOp,
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

enum CBBoolOp
{
    Equal,
    More,
    Less,
    MoreEqual,
    LessEqual
};

// Forward declarations
struct CBVar;
DA_TYPEDEF(CBVar, CBSeq);
struct CBChain;
struct CBRuntimeBlock;
struct CBTypeInfo;
DA_TYPEDEF(CBTypeInfo, CBTypesInfo)
struct CBParameterInfo;
DA_TYPEDEF(CBParameterInfo, CBParametersInfo)
struct CBContext;
struct CBRegistry;

typedef void* CBPointer;
typedef int64_t CBInt;
typedef double CBFloat;
typedef bool CBBool;
typedef gbString CBString;

#if defined(__clang__) || defined(__GNUC__)
  typedef int64_t CBInt2 __attribute__((vector_size(16)));
  typedef int64_t CBInt3 __attribute__((vector_size(32)));
  typedef int64_t CBInt4 __attribute__((vector_size(32)));

  typedef double CBFloat2 __attribute__((vector_size(16)));
  typedef double CBFloat3 __attribute__((vector_size(32)));
  typedef double CBFloat4 __attribute__((vector_size(32)));
#else
  // TODO, this is not aligned correctly!
  struct CBInt2
  {
    CBInt elements[2];
  };

  struct CBInt3
  {
    CBInt elements[3];
  };

  struct CBInt4
  {
    CBInt elements[4];
  };

  struct CBFloat2
  {
    CBFloat elements[2];
  };

  struct CBFloat3
  {
    CBFloat elements[3];
  };

  struct CBFloat4
  {
    CBFloat elements[4];
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
  int8_t* data;
};

struct CBTypeInfo
{
  bool sequenced;
  CBType basicType;
  union
  {
    struct {
      int32_t objectVendorId;
      int32_t objectTypeId;
    };
  };
  CBTypesInfo seqTypes;
};

struct CBObjectInfo
{
  CBString name;
  // Add more info about custom objects here
};

struct CBParameterInfo
{
  CBString name;
  CBString help;
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

struct CBVar 
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

    CBChain* chainValue;

    CBBoolOp boolOpValue;
  };

  CBType valueType;

  // uint8_t reserved[31]; // compiler will align this anyway to 64 bytes due to vectors!

  void free()
  {
    if(valueType == String && stringValue != nullptr)
    {
      gb_free_string(stringValue);
      stringValue = nullptr;
    }
    else if(valueType == Seq)
    {
      da_free(seqValue);
    }
  }
};

typedef CBRuntimeBlock* (__cdecl *CBBlockConstructor)();

typedef const CBString (__cdecl *CBNameProc)(CBRuntimeBlock*);
typedef const CBString (__cdecl *CBHelpProc)(CBRuntimeBlock*);

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
  CBDestroyProc destroy; // A one time finalizer for the block
  
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

namespace chainblocks
{
  template<typename T>
  static T* allocateBlock() { return new T(); }
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
#include <boost/context/continuation.hpp>

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

struct CBRegistry
{
  std::unordered_map<std::string, CBBlockConstructor> blocksRegister;
  std::unordered_map<std::tuple<int32_t, int32_t>, CBObjectInfo> objectTypesRegister;
};

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
};

struct CBChain
{
  CBChain() : coro(nullptr), next(0), sleepSeconds(0.0), started(false), finished(false), returned(false), rootTickInput(CBVar()), finishedOutput(CBVar())
  {}

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

  void addBlock(CBRuntimeBlock* blk)
  {
    blocks.push_back(blk);
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

EXPORTED void __cdecl chainblocks_RegisterBlock(CBRegistry* registry, const CBString fullName, CBBlockConstructor constructor);
EXPORTED void __cdecl chainblocks_RegisterObjectType(CBRegistry* registry, int32_t vendorId, int32_t typeId, CBObjectInfo info);

EXPORTED CBVar* __cdecl chainblocks_ContextVariable(CBContext* context, const CBString name);
EXPORTED void __cdecl chainblocks_SetError(CBContext* context, const CBString errorText);

EXPORTED CBVar __cdecl chainblocks_Suspend(double seconds);

#ifdef __cplusplus
};
#endif

namespace chainblocks
{
  static CBRegistry GlobalRegistry;
  static std::unordered_map<std::string, CBVar> GlobalVariables;
  thread_local static CBChain* CurrentChain;

  static void setCurrentChain(CBChain* chain)
  {
    CurrentChain = chain;
  }

  static void setCurrentChain(CBChain& chain)
  {
    CurrentChain = &chain;
  }

  static CBChain& getCurrentChain()
  {
    return *CurrentChain;
  }

  static CBRegistry* GetGlobalRegistry()
  {
    return &GlobalRegistry;
  }

  static void registerBlock(CBRegistry& registry, const CBString fullName, CBBlockConstructor constructor)
  {
    auto cname = std::string(fullName);
    auto findIt = registry.blocksRegister.find(cname);
    if(findIt == registry.blocksRegister.end())
    {
      registry.blocksRegister.insert(std::make_pair(cname, constructor));
      std::cout << "added block: " << cname << "\n";
    }
    else
    {
      registry.blocksRegister[cname] = constructor;
      std::cout << "overridden block: " << cname << "\n";
    }
  }

  static void registerBlock(const CBString fullName, CBBlockConstructor constructor)
  {
    registerBlock(GlobalRegistry, fullName, constructor);
  }

  static void registerObjectType(CBRegistry& registry, int32_t vendorId, int32_t typeId, CBObjectInfo info)
  {
    auto tup = std::make_tuple(vendorId, typeId);
    auto typeName = std::string(info.name);
    auto findIt = registry.objectTypesRegister.find(tup);
    if(findIt == registry.objectTypesRegister.end())
    {
      registry.objectTypesRegister.insert(std::make_pair(tup, info));
      std::cout << "added object type: " << typeName << "\n";
    }
    else
    {
      registry.objectTypesRegister[tup] = info;
      std::cout << "overridden object type: " << typeName << "\n";
    }
  }

  static void registerObjectType(int32_t vendorId, int32_t typeId, CBObjectInfo info)
  {
    registerObjectType(GlobalRegistry, vendorId, typeId, info);
  }

  static CBVar* globalVariable(const CBString name)
  {
    CBVar& v = GlobalVariables[name];
    return &v;
  }

  static bool hasGlobalVariable(const CBString name)
  {
    auto findIt = GlobalVariables.find(name);
    if(findIt == GlobalVariables.end())
      return false;
    return true;
  }

  static CBVar* contextVariable(CBContext* ctx, const CBString name)
  {
    CBVar& v = ctx->variables[name];
    return &v;
  }

  static CBRuntimeBlock* createBlock(const CBString name)
  {
    auto it = GlobalRegistry.blocksRegister.find(name);
    if(it == GlobalRegistry.blocksRegister.end())
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
};
#endif