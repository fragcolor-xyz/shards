#pragma once

#ifdef CHAINBLOCKS_RUNTIME

// ONLY CLANG AND GCC SUPPORTED FOR NOW

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wswitch"
#elif defined(__clang__)
#pragma clang diagnostic ignored "-Wtype-limits"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wswitch"
#endif

#include "chainblocks.hpp"
// C++ Mandatory from now!

// Stub inline blocks, actually implemented in respective nim code!
struct CBConstStub
{
  CBRuntimeBlock header;
  CBVar constValue;
};
struct CBSleepStub
{
  CBRuntimeBlock header;
  double sleepTime;
};
struct CBMathStub
{
  CBRuntimeBlock header;
  CBVar operand;
  CBVar* ctxOperand;
  CBSeq seqCache;
};
struct CBMathUnaryStub
{
  CBRuntimeBlock header;
  CBSeq seqCache;
};
struct CBCoreRepeat
{
  CBRuntimeBlock header;
  int32_t times;
  CBChainPtr* chain;
};
struct CBCoreIf
{
  CBRuntimeBlock header;
  uint8_t boolOp;
  CBVar match;
  CBVar* matchCtx;
  CBChainPtr* trueChain;
  CBChainPtr* falseChain;
  bool passthrough;
};
struct CBCoreSetVariable
{
  // Also Get and Add
  CBRuntimeBlock header;
  CBVar* target;
};
struct CBCoreSwapVariables
{
  CBRuntimeBlock header;
  CBVar* target1;
  CBVar* target2;
};

// Since we build the runtime we are free to use any std and lib
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <iostream>
#include <ctime>

// Required external dependencies
// For coroutines/context switches
#include <boost/context/continuation.hpp>
// For sleep
#if _WIN32
#include <Windows.h>
#else
#include <time.h>
#endif

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
  CBContext(CBCoro&& sink) : stack(nullptr), restarted(false), aborted(false), continuation(std::move(sink))
  {
  }

  std::unordered_map<std::string, CBVar> variables;
  CBSeq stack;
  std::string error;

  // Those 2 go together with CBVar chainstates restart and stop
  bool restarted;
  // Also used to cancel a chain
  bool aborted;

  // Used within the coro stack! (suspend, etc)
  CBCoro&& continuation;

  void setError(const char* errorMsg)
  {
    error = errorMsg;
  }
};

struct CBChain
{
  CBChain(const char* chain_name) : 
    name(chain_name),
    coro(nullptr),
    next(0),
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
      blk->cleanup(blk);
      blk->destroy(blk);
      //blk is responsible to free itself, as they might use any allocation strategy they wish!
    }
  }

  std::string name;

  CBCoro* coro;
  clock_t next;
  
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

  class CBException : public std::exception
  {
    public:
      CBException(const char* errmsg) : errorMessage(errmsg) {}

      const char * what () const noexcept
      {
        return errorMessage;
      }

    private:
      const char* errorMessage;
  };
};

using json = nlohmann::json;
// The following procedures implement json.hpp protocol in order to allow easy integration! they must stay outside the namespace!
void to_json(json& j, const CBVar& var);
void from_json(const json& j, CBVar& var);
void to_json(json& j, const CBChainPtr& chain);
void from_json(const json& j, CBChainPtr& chain);

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

    // Hook inline blocks to override activation in runChain
    if(strcmp(name, "Const") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::CoreConst;
    }
    else if(strcmp(name, "Sleep") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::CoreSleep;
    }
    else if(strcmp(name, "Repeat") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::CoreRepeat;
    }
    else if(strcmp(name, "If") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::CoreIf;
    }
    else if(strcmp(name, "SetVariable") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::CoreSetVariable;
    }
    else if(strcmp(name, "GetVariable") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::CoreGetVariable;
    }
    else if(strcmp(name, "SwapVariables") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::CoreSwapVariables;
    }
    else if(strcmp(name, "Math.Add") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathAdd;
    }
    else if(strcmp(name, "Math.Subtract") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathSubtract;
    }
    else if(strcmp(name, "Math.Multiply") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathMultiply;
    }
    else if(strcmp(name, "Math.Divide") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathDivide;
    }
    else if(strcmp(name, "Math.Xor") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathXor;
    }
    else if(strcmp(name, "Math.And") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathAnd;
    }
    else if(strcmp(name, "Math.Or") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathOr;
    }
    else if(strcmp(name, "Math.Mod") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathMod;
    }
    else if(strcmp(name, "Math.LShift") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathLShift;
    }
    else if(strcmp(name, "Math.RShift") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathRShift;
    }
    else if(strcmp(name, "Math.Abs") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathAbs;
    }
    else if(strcmp(name, "Math.Exp") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathExp;
    }
    else if(strcmp(name, "Math.Exp2") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathExp2;
    }
    else if(strcmp(name, "Math.Expm1") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathExpm1;
    }
    else if(strcmp(name, "Math.Log") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathLog;
    }
    else if(strcmp(name, "Math.Log10") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathLog10;
    }
    else if(strcmp(name, "Math.Log2") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathLog2;
    }
    else if(strcmp(name, "Math.Log1p") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathLog1p;
    }
    else if(strcmp(name, "Math.Sqrt") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathSqrt;
    }
    else if(strcmp(name, "Math.Cbrt") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathCbrt;
    }
    else if(strcmp(name, "Math.Sin") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathSin;
    }
    else if(strcmp(name, "Math.Cos") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathCos;
    }
    else if(strcmp(name, "Math.Tan") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathTan;
    }
    else if(strcmp(name, "Math.Asin") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathAsin;
    }
    else if(strcmp(name, "Math.Acos") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathAcos;
    }
    else if(strcmp(name, "Math.Atan") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathAtan;
    }
    else if(strcmp(name, "Math.Sinh") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathSinh;
    }
    else if(strcmp(name, "Math.Cosh") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathCosh;
    }
    else if(strcmp(name, "Math.Tanh") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathTanh;
    }
    else if(strcmp(name, "Math.Asinh") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathAsinh;
    }
    else if(strcmp(name, "Math.Acosh") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathAcosh;
    }
    else if(strcmp(name, "Math.Atanh") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathAtanh;
    }
    else if(strcmp(name, "Math.Erf") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathErf;
    }
    else if(strcmp(name, "Math.Erfc") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathErfc;
    }
    else if(strcmp(name, "Math.TGamma") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathTGamma;
    }
    else if(strcmp(name, "Math.LGamma") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathLGamma;
    }
    else if(strcmp(name, "Math.Ceil") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathCeil;
    }
    else if(strcmp(name, "Math.Floor") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathFloor;
    }
    else if(strcmp(name, "Math.Trunc") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathTrunc;
    }
    else if(strcmp(name, "Math.Round") == 0)
    {
      blkp->inlineBlockId = CBInlineBlocks::MathRound;
    }

    return blkp;
  }

  static CBVar suspend(double seconds)
  {
    auto current = chainblocks::CurrentChain;
    if(seconds <= 0)
      current->next = 0;
    else  
      current->next = std::clock() + clock_t(seconds * double(CLOCKS_PER_SEC));
    current->context->continuation = current->context->continuation.resume();
    if(current->context->restarted)
    {
      CBVar restart = {};
      restart.valueType = None;
      restart.chainState = Restart;
      return restart;
    }
    else if(current->context->aborted)
    {
      CBVar stop = {};
      stop.valueType = None;
      stop.chainState = Stop;
      return stop;
    }
    CBVar cont = {};
    cont.valueType = None;
    cont.chainState = Continue;
    return cont;
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
        if(context->error.length() > 0)
          std::cerr << "Last error: " << std::string(context->error) << "\n";
        std::cerr << e.what() << "\n";
        CurrentChain = previousChain;
        return { false, {} };
      }
      catch(...)
      {
        std::cerr << "Pre chain failure, failed block: " << std::string(blk->name(blk)) << "\n";
        if(context->error.length() > 0)
          std::cerr << "Last error: " << std::string(context->error) << "\n";
        CurrentChain = previousChain;
        return { false, {} };
      }
    }

    #include "runtime_macros.hpp"

    for(auto blk : chain->blocks)
    {
      try
      {
        #if 0
          std::cout << "Activating block: " << std::string(blk->name(blk)) << "\n";
        #endif

        // Pass chain root input every time we find None, this allows a long chain to re-process the root input if wanted!
        auto input = previousOutput.valueType == None ? chainInput : previousOutput;
        switch(blk->inlineBlockId)
        {
          case CoreConst:
          {
            auto cblock = reinterpret_cast<CBConstStub*>(blk);
            previousOutput = cblock->constValue;
            break;
          }
          case CoreSleep:
          {
            auto cblock = reinterpret_cast<CBSleepStub*>(blk);
            auto suspendRes = suspend(cblock->sleepTime);
            if(suspendRes.chainState != Continue)
              previousOutput = suspendRes;
            break;
          }
          case CoreRepeat:
          {
            auto cblock = reinterpret_cast<CBCoreRepeat*>(blk);
            if(likely(cblock->chain and *cblock->chain))
            {
              for(auto i = 0; i < cblock->times; i++)
              {
                runChainQUICKRUN(*cblock->chain)
              }
            }
            else if(unlikely(cblock->chain and !(*cblock->chain)))
            {
              throw CBException("A required sub-chain was not found, stopping!");
            }
            break;
          }
          case CoreIf:
          {
            // We only do it quick in certain cases!
            auto cblock = reinterpret_cast<CBCoreIf*>(blk);            
            auto match = cblock->match.valueType == ContextVar ?
              cblock->matchCtx ? *cblock->matchCtx : *(cblock->matchCtx = contextVariable(context, cblock->match.stringValue)) :
                cblock->match;
            auto result = false;
            if(unlikely(input.valueType != match.valueType))
            {
              // Always fail the test when different types
              if(likely(cblock->falseChain and *cblock->falseChain))
              {
                runChainQUICKRUN(*cblock->falseChain)
                if(!cblock->passthrough)
                {
                  previousOutput = std::get<1>(chainRes);
                }
              }
              else
              {
                throw CBException("A required sub-chain was not found, stopping!");
              }
            }
            else
            {
              switch(input.valueType)
              {
                case Int:
                  switch(cblock->boolOp)
                  {
                    case 0:
                      result = input.intValue == match.intValue;
                      break;
                    case 1:
                      result = input.intValue > match.intValue;
                      break;
                    case 2:
                      result = input.intValue < match.intValue;
                      break;
                    case 3:
                      result = input.intValue >= match.intValue;
                      break;
                    case 4:
                      result = input.intValue <= match.intValue;
                      break;
                  }
                  break;
                case Float:
                  switch(cblock->boolOp)
                  {
                    case 0:
                      result = input.floatValue == match.floatValue;
                      break;
                    case 1:
                      result = input.floatValue > match.floatValue;
                      break;
                    case 2:
                      result = input.floatValue < match.floatValue;
                      break;
                    case 3:
                      result = input.floatValue >= match.floatValue;
                      break;
                    case 4:
                      result = input.floatValue <= match.floatValue;
                      break;
                  }
                  break;
                case String:
                  // http://www.cplusplus.com/reference/string/string/operators/
                  switch(cblock->boolOp)
                  {
                    case 0:
                      result = input.stringValue == match.stringValue;
                      break;
                    case 1:
                      result = input.stringValue > match.stringValue;
                      break;
                    case 2:
                      result = input.stringValue < match.stringValue;
                      break;
                    case 3:
                      result = input.stringValue >= match.stringValue;
                      break;
                    case 4:
                      result = input.stringValue <= match.stringValue;
                      break;
                  }
                  break;
                default:
                  // too complex let's just make the activation call!
                  previousOutput = blk->activate(blk, context, input);
                  goto ifdone;
              }
              if(result)
              {
                if(likely(cblock->trueChain and *cblock->trueChain))
                {
                  runChainQUICKRUN(*cblock->trueChain)
                  if(!cblock->passthrough)
                  {
                    previousOutput = std::get<1>(chainRes);
                  }
                }
                else
                {
                  throw CBException("A required sub-chain was not found, stopping!");
                }
              }
              else
              {
                if(likely(cblock->falseChain and *cblock->falseChain))
                {
                  runChainQUICKRUN(*cblock->falseChain)
                  if(!cblock->passthrough)
                  {
                    previousOutput = std::get<1>(chainRes);
                  }
                }
                else
                {
                  throw CBException("A required sub-chain was not found, stopping!");
                }
              }
              ifdone:
                if(0) {}
            }
            break;
          }
          case CoreSetVariable:
          {
            auto cblock = reinterpret_cast<CBCoreSetVariable*>(blk);
            if(unlikely(!cblock->target)) // call first if we have no target
            {
              blk->activate(blk, context, input); // ignore previousOutput since we pass input
            }
            else
            {
              // Handle the special case of a seq that might need reset every run
              if(unlikely((*cblock->target).valueType == Seq && input.valueType == None))
              {
                stbds_arrsetlen((*cblock->target).seqValue, size_t(0));
              }
              else
              {
                *cblock->target = input;
              }
            }
            break;
          }
          case CoreGetVariable:
          {
            auto cblock = reinterpret_cast<CBCoreSetVariable*>(blk);
            if(unlikely(!cblock->target)) // call first if we have no target
            {
              previousOutput = blk->activate(blk, context, input);
            }
            else
            {
              previousOutput = *cblock->target;
            }
            break;
          }
          case CoreSwapVariables:
          {
            auto cblock = reinterpret_cast<CBCoreSwapVariables*>(blk);
            if(unlikely(!cblock->target1 || !cblock->target2)) // call first if we have no targets
            {
              blk->activate(blk, context, input); // ignore previousOutput since we pass input
            }
            else
            {
              auto tmp = *cblock->target1;
              *cblock->target1 = *cblock->target2;
              *cblock->target2 = tmp;
            }
            break;
          }
          case MathAdd:
          {
            auto cblock = reinterpret_cast<CBMathStub*>(blk);
            runChainINLINEMATH(+, "+")
            break;
          }
          case MathSubtract:
          {
            auto cblock = reinterpret_cast<CBMathStub*>(blk);
            runChainINLINEMATH(-, "-")
            break;
          }
          case MathMultiply:
          {
            auto cblock = reinterpret_cast<CBMathStub*>(blk);
            runChainINLINEMATH(*, "*")
            break;
          }
          case MathDivide:
          {
            auto cblock = reinterpret_cast<CBMathStub*>(blk);
            runChainINLINEMATH(/, "/")
            break;
          }
          case MathXor:
          {
            auto cblock = reinterpret_cast<CBMathStub*>(blk);
            runChainINLINE_INT_MATH(^, "^")
            break;
          }
          case MathAnd:
          {
            auto cblock = reinterpret_cast<CBMathStub*>(blk);
            runChainINLINE_INT_MATH(&, "&")
            break;
          }
          case MathOr:
          {
            auto cblock = reinterpret_cast<CBMathStub*>(blk);
            runChainINLINE_INT_MATH(|, "|")
            break;
          }
          case MathMod:
          {
            auto cblock = reinterpret_cast<CBMathStub*>(blk);
            runChainINLINE_INT_MATH(%, "%")
            break;
          }
          case MathLShift:
          {
            auto cblock = reinterpret_cast<CBMathStub*>(blk);
            runChainINLINE_INT_MATH(<<, "<<")
            break;
          }
          case MathRShift:
          {
            auto cblock = reinterpret_cast<CBMathStub*>(blk);
            runChainINLINE_INT_MATH(>>, ">>")
            break;
          }
          case MathAbs:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(fabs, fabsf, "Abs")
            break;
          }
          case MathExp:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(exp, expf, "Exp")
            break;
          }
          case MathExp2:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(exp2, exp2f, "Exp2")
            break;
          }
          case MathExpm1:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(expm1, expm1f, "Expm1")
            break;
          }
          case MathLog:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(log, logf, "Log")
            break;
          }
          case MathLog10:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(log10, log10f, "Log10")
            break;
          }
          case MathLog2:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(log2, log2f, "Log2")
            break;
          }
          case MathLog1p:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(log1p, log1pf, "Log1p")
            break;
          }
          case MathSqrt:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(sqrt, sqrtf, "Sqrt")
            break;
          }
          case MathCbrt:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(cbrt, cbrtf, "Cbrt")
            break;
          }
          case MathSin:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(sin, sinf, "Sin")
            break;
          }
          case MathCos:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(cos, cosf, "Cos")
            break;
          }
          case MathTan:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(tan, tanf, "Tan")
            break;
          }
          case MathAsin:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(asin, asinf, "Asin")
            break;
          }
          case MathAcos:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(acos, acosf, "Acos")
            break;
          }
          case MathAtan:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(atan, atanf, "Atan")
            break;
          }
          case MathSinh:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(sinh, sinhf, "Sinh")
            break;
          }
          case MathCosh:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(cosh, coshf, "Cosh")
            break;
          }
          case MathTanh:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(tanh, tanhf, "Tanh")
            break;
          }
          case MathAsinh:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(asinh, asinhf, "Asinh")
            break;
          }
          case MathAcosh:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(acosh, acoshf, "Acosh")
            break;
          }
          case MathAtanh:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(atanh, atanhf, "Atanh")
            break;
          }
          case MathErf:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(erf, erff, "Erf")
            break;
          }
          case MathErfc:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(erfc, erfcf, "Erfc")
            break;
          }
          case MathTGamma:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(tgamma, tgammaf, "TGamma")
            break;
          }
          case MathLGamma:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(lgamma, lgammaf, "LGamma")
            break;
          }
          case MathCeil:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(ceil, ceilf, "Ceil")
            break;
          }
          case MathFloor:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(floor, floorf, "Floor")
            break;
          }
          case MathTrunc:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(trunc, truncf, "Trunc")
            break;
          }
          case MathRound:
          {
            auto cblock = reinterpret_cast<CBMathUnaryStub*>(blk);
            runChainINLINECMATH(round, roundf, "Round")
            break;
          }
          default: // NotInline
          {
            previousOutput = blk->activate(blk, context, input);
          }
        }

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
              if(context->error.length() > 0)
                std::cerr << "Last error: " << std::string(context->error) << "\n";
              runChainPOSTCHAIN
              CurrentChain = previousChain;
              return { false, previousOutput };
            }
          }
        }
      }
      catch(const std::exception& e)
      {
        std::cerr << "Block activation error, failed block: " << std::string(blk->name(blk)) << "\n";
        if(context->error.length() > 0)
          std::cerr << "Last error: " << std::string(context->error) << "\n";
        std::cerr << e.what() << "\n";;
        runChainPOSTCHAIN
        CurrentChain = previousChain;
        return { false, previousOutput };
      }
      catch(...)
      {
        std::cerr << "Block activation error, failed block: " << std::string(blk->name(blk)) << "\n";
        if(context->error.length() > 0)
          std::cerr << "Last error: " << std::string(context->error) << "\n";
        runChainPOSTCHAIN
        CurrentChain = previousChain;
        return { false, previousOutput };
      }
    }

    runChainPOSTCHAIN
    CurrentChain = previousChain;
    return { true, previousOutput };
  }

  static void start(CBChain* chain, bool loop = false, bool unsafe = false)
  {
    chain->coro = new CBCoro(boost::context::callcc([&chain, &loop, &unsafe](boost::context::continuation&& sink)
    {
      // Need to copy those in this stack!
      CBChain* thisChain = chain;
      bool looped = loop;
      bool unsafeLoop = unsafe;
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
        if(context.stack)
          stbds_arrfree(context.stack);
        
        thisChain->finished = false; // Reset finished flag

        auto runRes = runChain(thisChain, &context, thisChain->rootTickInput);
        thisChain->finished = true;
        thisChain->finishedOutput = std::get<1>(runRes);
        if(!std::get<0>(runRes))
        {
          context.aborted = true;
          break;
        }
        
        if(!unsafeLoop && looped) 
        {
          // Ensure no while(true), yield anyway every run
          thisChain->next = 0;
          context.continuation = context.continuation.resume();
          // This is delayed upon continuation!!
          if(context.aborted)
            break;
        }
      }

      // Completely free the stack
      if(context.stack)
        stbds_arrfree(context.stack);

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
      // Do this in reverse to allow a safer cleanup
      for (auto it = chain->blocks.rbegin(); it != chain->blocks.rend(); ++it)
      {
        auto blk = *it;
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
#ifdef _WIN32
      HANDLE timer;
      LARGE_INTEGER ft;
      ft.QuadPart = -(int64_t(seconds * 10000000));
      timer = CreateWaitableTimer(NULL, TRUE, NULL);
      SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
      WaitForSingleObject(timer, INFINITE);
      CloseHandle(timer);
#else
      struct timespec delay = {0, int64_t(seconds * 1000000000)}
      while(nanosleep(&delay, &delay));
#endif
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

#endif //CHAINBLOCKS_RUNTIME
