#pragma once

#ifdef CHAINBLOCKS_RUNTIME

// ONLY CLANG AND GCC SUPPORTED FOR NOW

#include <string.h> // memset

#include <regex>

#include "blocks_macros.hpp"
#include "chainblocks.hpp"
// C++ Mandatory from now!

// Stub inline blocks, actually implemented in respective nim code!
struct CBConstStub {
  CBlock header;
  struct {
    CBVar constValue;
  };
};

struct CBSleepStub {
  CBlock header;
  struct {
    double sleepTime;
  };
};

struct CBMathStub {
  CBlock header;
  struct {
    CBVar operand;
    CBVar *ctxOperand;
    CBSeq seqCache;
  };
};

struct CBMathUnaryStub {
  CBlock header;
  struct {
    CBSeq seqCache;
  };
};

struct CBCoreRepeat {
  CBlock header;
  struct {
    bool doForever;
    int32_t times;
    CBSeq blocks;
  };
};

struct CBCoreIf {
  CBlock header;
  struct {
    uint8_t boolOp;
    CBVar match;
    CBVar *matchCtx;
    CBSeq trueBlocks;
    CBSeq falseBlocks;
    bool passthrough;
  };
};

struct CBCoreSetVariable {
  // Also Get and Add
  CBlock header;
  struct {
    CBVar *target;
  };
};

struct CBCoreSwapVariables {
  CBlock header;
  struct {
    CBVar *target1;
    CBVar *target2;
  };
};

// Since we build the runtime we are free to use any std and lib
#include <atomic>
#include <chrono>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>
using Clock = std::chrono::high_resolution_clock;
using Duration = std::chrono::duration<double>;
using Time = std::chrono::time_point<Clock, Duration>;

// Required external dependencies
// For coroutines/context switches
#include <boost/context/continuation.hpp>
// For sleep
#if _WIN32
#include <Windows.h>
#else
#include <time.h>
#endif

#include <tuple>
// Tuple hashing
namespace std {
namespace {

// Code from boost
// Reciprocal of the golden ratio helps spread entropy
//     and handles duplicates.
// See Mike Seymour in magic-numbers-in-boosthash-combine:
//     http://stackoverflow.com/questions/4948780

template <class T> inline void hash_combine(std::size_t &seed, T const &v) {
  seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

// Recursive template code derived from Matthieu M.
template <class Tuple, size_t Index = std::tuple_size<Tuple>::value - 1>
struct HashValueImpl {
  static void apply(size_t &seed, Tuple const &tuple) {
    HashValueImpl<Tuple, Index - 1>::apply(seed, tuple);
    hash_combine(seed, std::get<Index>(tuple));
  }
};

template <class Tuple> struct HashValueImpl<Tuple, 0> {
  static void apply(size_t &seed, Tuple const &tuple) {
    hash_combine(seed, std::get<0>(tuple));
  }
};
} // namespace

template <typename... TT> struct hash<std::tuple<TT...>> {
  size_t operator()(std::tuple<TT...> const &tt) const {
    size_t seed = 0;
    HashValueImpl<std::tuple<TT...>>::apply(seed, tt);
    return seed;
  }
};
} // namespace std

namespace chainblocks {
extern phmap::node_hash_map<std::string, CBBlockConstructor> BlocksRegister;
extern phmap::node_hash_map<std::tuple<int32_t, int32_t>, CBObjectInfo>
    ObjectTypesRegister;
extern phmap::node_hash_map<std::tuple<int32_t, int32_t>, CBEnumInfo>
    EnumTypesRegister;
extern phmap::node_hash_map<std::string, CBVar> GlobalVariables;
extern std::map<std::string, CBCallback> RunLoopHooks;
extern phmap::node_hash_map<std::string, CBCallback> ExitHooks;
extern phmap::node_hash_map<std::string, CBChain *> GlobalChains;

static CBlock *createBlock(const char *name);

static void registerChain(CBChain *chain);

static void unregisterChain(CBChain *chain);

static int destroyVar(CBVar &var);
}; // namespace chainblocks

typedef boost::context::continuation CBCoro;

struct CBChain {
  CBChain(const char *chain_name)
      : looped(false), unsafe(false), name(chain_name), coro(nullptr),
        started(false), finished(false), returned(false), failed(false),
        rootTickInput(CBVar()), finishedOutput(CBVar()), ownedOutput(false),
        context(nullptr), node(nullptr) {
    chainblocks::registerChain(this);
  }

  ~CBChain() {
    cleanup();
    chainblocks::unregisterChain(this);
    chainblocks::destroyVar(rootTickInput);
  }

  void cleanup();

  // Also the chain takes ownership of the block!
  void addBlock(CBlock *blk) { blocks.push_back(blk); }

  // Also removes ownership of the block
  void removeBlock(CBlock *blk) {
    auto findIt = std::find(blocks.begin(), blocks.end(), blk);
    if (findIt != blocks.end()) {
      blocks.erase(findIt);
    }
  }

  // Attributes
  bool looped;
  bool unsafe;

  std::string name;

  CBCoro *coro;

  // we could simply null check coro but actually some chains (sub chains), will
  // run without a coro within the root coro so we need this too
  bool started;

  // this gets cleared before every runChain and set after every runChain
  std::atomic_bool finished;

  // when running as coro if actually the coro lambda exited
  bool returned;
  bool failed;

  CBVar rootTickInput;
  CBVar finishedOutput;
  bool ownedOutput;

  CBContext *context;
  CBNode *node;
  std::vector<CBlock *> blocks;
};

struct CBContext {
  CBContext(CBCoro &&sink, CBChain *running_chain)
      : chain(running_chain), restarted(false), aborted(false),
        shouldPause(false), paused(false), continuation(std::move(sink)) {
    static std::regex re(
        "[^abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\\-\\."
        "\\_]+");
    logger_name = std::regex_replace(chain->name, re, "_");
    logger_name = "chain." + logger_name;
    el::Loggers::getLogger(logger_name.c_str());
  }

  ~CBContext() { el::Loggers::unregisterLogger(logger_name.c_str()); }

  CBChain *chain;

  phmap::node_hash_map<std::string, CBVar> variables;
  std::string error;

  // Those 2 go together with CBVar chainstates restart and stop
  bool restarted;
  // Also used to cancel a chain
  bool aborted;
  // Used internally to pause a chain execution
  std::atomic_bool shouldPause;
  std::atomic_bool paused;

  // Used within the coro& stack! (suspend, etc)
  CBCoro &&continuation;
  Duration next;

  // Have a logger per context
  std::string logger_name;

  void setError(const char *errorMsg) { error = errorMsg; }
};

CBTypeInfo validateConnections(const std::vector<CBlock *> &chain,
                               CBValidationCallback callback, void *userData,
                               CBTypeInfo inputType = CBTypeInfo());
CBTypeInfo validateConnections(const CBlocks chain,
                               CBValidationCallback callback, void *userData,
                               CBTypeInfo inputType = CBTypeInfo());
CBTypeInfo validateConnections(const CBChain *chain,
                               CBValidationCallback callback, void *userData,
                               CBTypeInfo inputType = CBTypeInfo());
bool validateSetParam(CBlock *block, int index, CBVar &value,
                      CBValidationCallback callback, void *userData);

using json = nlohmann::json;
// The following procedures implement json.hpp protocol in order to allow easy
// integration! they must stay outside the namespace!
void to_json(json &j, const CBVar &var);
void from_json(const json &j, CBVar &var);
void to_json(json &j, const CBChainPtr &chain);
void from_json(const json &j, CBChainPtr &chain);

namespace chainblocks {
void installSignalHandlers();

static int destroyVar(CBVar &var) {
  int freeCount = 0;
  switch (var.valueType) {
  case Seq: {
    int len = stbds_arrlen(var.payload.seqValue);
    for (int i = 0; i < len; i++) {
      freeCount += destroyVar(var.payload.seqValue[i]);
    }
    stbds_arrfree(var.payload.seqValue);
    freeCount++;
  } break;
  case String:
  case ContextVar: {
    delete[] var.payload.stringValue;
    freeCount++;
  } break;
  case Image: {
    delete[] var.payload.imageValue.data;
    freeCount++;
  } break;
  case Table: {
    auto len = stbds_shlen(var.payload.tableValue);
    for (auto i = 0; i < len; i++) {
      freeCount += destroyVar(var.payload.tableValue[i].value);
    }
    stbds_shfree(var.payload.tableValue);
    freeCount++;
  } break;
  default:
    break;
  };

  memset((void *)&var, 0x0, sizeof(CBVar));

  return freeCount;
}

static int cloneVar(CBVar &dst, const CBVar &src) {
  int freeCount = 0;
  if (src.valueType < EndOfBlittableTypes &&
      dst.valueType < EndOfBlittableTypes) {
    memcpy((void *)&dst, (const void *)&src, sizeof(CBVar));
  } else if (src.valueType < EndOfBlittableTypes) {
    freeCount += destroyVar(dst);
    memcpy((void *)&dst, (const void *)&src, sizeof(CBVar));
  } else {
    switch (src.valueType) {
    case Seq: {
      int srcLen = stbds_arrlen(src.payload.seqValue);
      // reuse if seq and we got enough capacity
      if (dst.valueType != Seq ||
          (int)stbds_arrcap(dst.payload.seqValue) < srcLen) {
        freeCount += destroyVar(dst);
        dst.valueType = Seq;
        dst.payload.seqLen = -1;
        dst.payload.seqValue = nullptr;
      } else {
        int dstLen = stbds_arrlen(dst.payload.seqValue);
        if (srcLen < dstLen) {
          // need to destroy leftovers
          for (auto i = srcLen; i < dstLen; i++) {
            freeCount += destroyVar(dst.payload.seqValue[i]);
          }
        }
      }

      stbds_arrsetlen(dst.payload.seqValue, (unsigned)srcLen);
      for (auto i = 0; i < srcLen; i++) {
        auto &subsrc = src.payload.seqValue[i];
        memset(&dst.payload.seqValue[i], 0x0, sizeof(CBVar));
        freeCount += cloneVar(dst.payload.seqValue[i], subsrc);
      }
    } break;
    case String:
    case ContextVar: {
      auto srcLen = strlen(src.payload.stringValue);
      if ((dst.valueType != String && dst.valueType != ContextVar) ||
          strlen(dst.payload.stringValue) < srcLen) {
        freeCount += destroyVar(dst);
        dst.payload.stringValue = new char[srcLen + 1];
      }

      dst.valueType = src.valueType;
      memcpy((void *)dst.payload.stringValue, (void *)src.payload.stringValue,
             srcLen);
      ((char *)dst.payload.stringValue)[srcLen] = '\0';
    } break;
    case Image: {
      auto srcImgSize = src.payload.imageValue.height *
                        src.payload.imageValue.width *
                        src.payload.imageValue.channels;
      auto dstImgSize = dst.payload.imageValue.height *
                        dst.payload.imageValue.width *
                        dst.payload.imageValue.channels;
      if (dst.valueType != Image || srcImgSize > dstImgSize) {
        freeCount += destroyVar(dst);
        dst.valueType = Image;
        dst.payload.imageValue.height = src.payload.imageValue.height;
        dst.payload.imageValue.width = src.payload.imageValue.width;
        dst.payload.imageValue.channels = src.payload.imageValue.channels;
        dst.payload.imageValue.data = new uint8_t[dstImgSize];
      }

      memcpy(dst.payload.imageValue.data, src.payload.imageValue.data,
             srcImgSize);
    } break;
    case Table: {
      // Slowest case, it's a full copy using arena tho
      freeCount += destroyVar(dst);
      dst.valueType = Table;
      dst.payload.tableLen = -1;
      dst.payload.tableValue = nullptr;
      stbds_sh_new_arena(dst.payload.tableValue);
      auto srcLen = stbds_shlen(src.payload.tableValue);
      for (auto i = 0; i < srcLen; i++) {
        CBVar clone{};
        freeCount += cloneVar(clone, src.payload.tableValue[i].value);
        stbds_shput(dst.payload.tableValue, src.payload.tableValue[i].key,
                    clone);
      }
    } break;
    default:
      break;
    };
  }
  return freeCount;
}

static void registerBlock(const char *fullName,
                          CBBlockConstructor constructor) {
  auto cname = std::string(fullName);
  auto findIt = BlocksRegister.find(cname);
  if (findIt == BlocksRegister.end()) {
    BlocksRegister.insert(std::make_pair(cname, constructor));
    // DLOG(INFO) << "added block: " << cname;
  } else {
    BlocksRegister[cname] = constructor;
    LOG(INFO) << "overridden block: " << cname;
  }
}

static void registerObjectType(int32_t vendorId, int32_t typeId,
                               CBObjectInfo info) {
  auto tup = std::make_tuple(vendorId, typeId);
  auto typeName = std::string(info.name);
  auto findIt = ObjectTypesRegister.find(tup);
  if (findIt == ObjectTypesRegister.end()) {
    ObjectTypesRegister.insert(std::make_pair(tup, info));
    // DLOG(INFO) << "added object type: " << typeName;
  } else {
    ObjectTypesRegister[tup] = info;
    LOG(INFO) << "overridden object type: " << typeName;
  }
}

static void registerEnumType(int32_t vendorId, int32_t typeId,
                             CBEnumInfo info) {
  auto tup = std::make_tuple(vendorId, typeId);
  auto typeName = std::string(info.name);
  auto findIt = ObjectTypesRegister.find(tup);
  if (findIt == ObjectTypesRegister.end()) {
    EnumTypesRegister.insert(std::make_pair(tup, info));
    // DLOG(INFO) << "added enum type: " << typeName;
  } else {
    EnumTypesRegister[tup] = info;
    LOG(INFO) << "overridden enum type: " << typeName;
  }
}

static void registerRunLoopCallback(const char *eventName,
                                    CBCallback callback) {
  chainblocks::RunLoopHooks[eventName] = callback;
}

static void unregisterRunLoopCallback(const char *eventName) {
  auto findIt = chainblocks::RunLoopHooks.find(eventName);
  if (findIt != chainblocks::RunLoopHooks.end()) {
    chainblocks::RunLoopHooks.erase(findIt);
  }
}

static void registerExitCallback(const char *eventName, CBCallback callback) {
  chainblocks::ExitHooks[eventName] = callback;
}

static void unregisterExitCallback(const char *eventName) {
  auto findIt = chainblocks::ExitHooks.find(eventName);
  if (findIt != chainblocks::ExitHooks.end()) {
    chainblocks::ExitHooks.erase(findIt);
  }
}

static void registerChain(CBChain *chain) {
  chainblocks::GlobalChains[chain->name] = chain;
}

static void unregisterChain(CBChain *chain) {
  auto findIt = chainblocks::GlobalChains.find(chain->name);
  if (findIt != chainblocks::GlobalChains.end()) {
    chainblocks::GlobalChains.erase(findIt);
  }
}

static void callExitCallbacks() {
  // Iterate backwards
  for (auto it = chainblocks::ExitHooks.begin();
       it != chainblocks::ExitHooks.end(); ++it) {
    it->second();
  }
}

static CBVar *globalVariable(const char *name) {
  CBVar &v = GlobalVariables[name];
  return &v;
}

static bool hasGlobalVariable(const char *name) {
  auto findIt = GlobalVariables.find(name);
  if (findIt == GlobalVariables.end())
    return false;
  return true;
}

static CBVar *contextVariable(CBContext *ctx, const char *name) {
  CBVar &v = ctx->variables[name];
  return &v;
}

void registerCoreBlocks();

static CBlock *createBlock(const char *name) {
  auto it = BlocksRegister.find(name);
  if (it == BlocksRegister.end()) {
    return nullptr;
  }

  auto blkp = it->second();

  // Hook inline blocks to override activation in runChain
  if (strcmp(name, "Const") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreConst;
  } else if (strcmp(name, "Sleep") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreSleep;
  } else if (strcmp(name, "Repeat") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreRepeat;
  } else if (strcmp(name, "If") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreIf;
  } else if (strcmp(name, "GetVariable") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreGetVariable;
  } else if (strcmp(name, "SwapVariables") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreSwapVariables;
  } else if (strcmp(name, "Math.Add") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathAdd;
  } else if (strcmp(name, "Math.Subtract") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathSubtract;
  } else if (strcmp(name, "Math.Multiply") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathMultiply;
  } else if (strcmp(name, "Math.Divide") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathDivide;
  } else if (strcmp(name, "Math.Xor") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathXor;
  } else if (strcmp(name, "Math.And") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathAnd;
  } else if (strcmp(name, "Math.Or") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathOr;
  } else if (strcmp(name, "Math.Mod") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathMod;
  } else if (strcmp(name, "Math.LShift") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathLShift;
  } else if (strcmp(name, "Math.RShift") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathRShift;
  } else if (strcmp(name, "Math.Abs") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathAbs;
  } else if (strcmp(name, "Math.Exp") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathExp;
  } else if (strcmp(name, "Math.Exp2") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathExp2;
  } else if (strcmp(name, "Math.Expm1") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathExpm1;
  } else if (strcmp(name, "Math.Log") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathLog;
  } else if (strcmp(name, "Math.Log10") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathLog10;
  } else if (strcmp(name, "Math.Log2") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathLog2;
  } else if (strcmp(name, "Math.Log1p") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathLog1p;
  } else if (strcmp(name, "Math.Sqrt") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathSqrt;
  } else if (strcmp(name, "Math.Cbrt") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathCbrt;
  } else if (strcmp(name, "Math.Sin") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathSin;
  } else if (strcmp(name, "Math.Cos") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathCos;
  } else if (strcmp(name, "Math.Tan") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathTan;
  } else if (strcmp(name, "Math.Asin") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathAsin;
  } else if (strcmp(name, "Math.Acos") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathAcos;
  } else if (strcmp(name, "Math.Atan") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathAtan;
  } else if (strcmp(name, "Math.Sinh") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathSinh;
  } else if (strcmp(name, "Math.Cosh") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathCosh;
  } else if (strcmp(name, "Math.Tanh") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathTanh;
  } else if (strcmp(name, "Math.Asinh") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathAsinh;
  } else if (strcmp(name, "Math.Acosh") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathAcosh;
  } else if (strcmp(name, "Math.Atanh") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathAtanh;
  } else if (strcmp(name, "Math.Erf") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathErf;
  } else if (strcmp(name, "Math.Erfc") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathErfc;
  } else if (strcmp(name, "Math.TGamma") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathTGamma;
  } else if (strcmp(name, "Math.LGamma") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathLGamma;
  } else if (strcmp(name, "Math.Ceil") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathCeil;
  } else if (strcmp(name, "Math.Floor") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathFloor;
  } else if (strcmp(name, "Math.Trunc") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathTrunc;
  } else if (strcmp(name, "Math.Round") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathRound;
  }

  return blkp;
}

static CBVar suspend(CBContext *context, double seconds) {
  if (seconds <= 0) {
    context->next = Duration(0);
  } else {
    context->next = Clock::now().time_since_epoch() + Duration(seconds);
  }
  context->continuation = context->continuation.resume();
  if (context->restarted) {
    CBVar restart = {};
    restart.valueType = None;
    restart.payload.chainState = Restart;
    return restart;
  } else if (context->aborted) {
    CBVar stop = {};
    stop.valueType = None;
    stop.payload.chainState = Stop;
    return stop;
  }
  CBVar cont = {};
  cont.valueType = None;
  cont.payload.chainState = Continue;
  return cont;
}

#define cbpause(_time_)                                                        \
  {                                                                            \
    auto chainState = chainblocks::suspend(context, _time_);                   \
    if (chainState.payload.chainState != Continue) {                           \
      return chainState;                                                       \
    }                                                                          \
  }

#include "runtime_macros.hpp"

inline static bool activateBlocks(CBSeq blocks, CBContext *context,
                                  const CBVar &input, CBVar &output);

inline static void activateBlock(CBlock *blk, CBContext *context,
                                 const CBVar &input, CBVar &previousOutput) {
  switch (blk->inlineBlockId) {
  case CoreConst: {
    auto cblock = reinterpret_cast<CBConstStub *>(blk);
    previousOutput = cblock->constValue;
    return;
  }
  case CoreSleep: {
    auto cblock = reinterpret_cast<CBSleepStub *>(blk);
    auto suspendRes = suspend(context, cblock->sleepTime);
    if (suspendRes.payload.chainState != Continue)
      previousOutput = suspendRes;
    else
      previousOutput = input;
    return;
  }
  case CoreRepeat: {
    auto cblock = reinterpret_cast<CBCoreRepeat *>(blk);
    auto repeats = cblock->doForever ? 1 : cblock->times;
    while (repeats) {
      CBVar repeatOutput{};
      repeatOutput.valueType = None;
      repeatOutput.payload.chainState = Continue;
      if (!activateBlocks(cblock->blocks, context, input, repeatOutput)) {
        previousOutput = StopChain;
        return;
      }

      if (!cblock->doForever)
        repeats--;
    }
    previousOutput = input;
    return;
  }
  case CoreIf: {
    // We only do it quick in certain cases!
    auto cblock = reinterpret_cast<CBCoreIf *>(blk);
    auto match = cblock->match.valueType == ContextVar
                     ? cblock->matchCtx
                           ? *cblock->matchCtx
                           : *(cblock->matchCtx = contextVariable(
                                   context, cblock->match.payload.stringValue))
                     : cblock->match;
    auto result = false;
    CBVar ifOutput{};
    if (unlikely(input.valueType != match.valueType)) {
      goto ifFalsePath;
    } else {
      switch (input.valueType) {
      case Int:
        switch (cblock->boolOp) {
        case 0:
          result = input.payload.intValue == match.payload.intValue;
          break;
        case 1:
          result = input.payload.intValue > match.payload.intValue;
          break;
        case 2:
          result = input.payload.intValue < match.payload.intValue;
          break;
        case 3:
          result = input.payload.intValue >= match.payload.intValue;
          break;
        case 4:
          result = input.payload.intValue <= match.payload.intValue;
          break;
        }
        break;
      case Float:
        switch (cblock->boolOp) {
        case 0:
          result = input.payload.floatValue == match.payload.floatValue;
          break;
        case 1:
          result = input.payload.floatValue > match.payload.floatValue;
          break;
        case 2:
          result = input.payload.floatValue < match.payload.floatValue;
          break;
        case 3:
          result = input.payload.floatValue >= match.payload.floatValue;
          break;
        case 4:
          result = input.payload.floatValue <= match.payload.floatValue;
          break;
        }
        break;
      case String:
        // http://www.cplusplus.com/reference/string/string/operators/
        switch (cblock->boolOp) {
        case 0:
          result = input.payload.stringValue == match.payload.stringValue;
          break;
        case 1:
          result = input.payload.stringValue > match.payload.stringValue;
          break;
        case 2:
          result = input.payload.stringValue < match.payload.stringValue;
          break;
        case 3:
          result = input.payload.stringValue >= match.payload.stringValue;
          break;
        case 4:
          result = input.payload.stringValue <= match.payload.stringValue;
          break;
        }
        break;
      default:
        // too complex let's just make the activation call into nim
        previousOutput = blk->activate(blk, context, input);
        return;
      }

      if (result) {
        if (!activateBlocks(cblock->trueBlocks, context, input, ifOutput)) {
          previousOutput = StopChain;
        } else if (cblock->passthrough) {
          previousOutput = input;
        } else {
          previousOutput = ifOutput;
        }
        return;
      } else {
      ifFalsePath:
        if (!activateBlocks(cblock->falseBlocks, context, input, ifOutput)) {
          previousOutput = StopChain;
        } else if (cblock->passthrough) {
          previousOutput = input;
        } else {
          previousOutput = ifOutput;
        }
        return;
      }
    }
    break;
  }
  case CoreGetVariable: {
    auto cblock = reinterpret_cast<CBCoreSetVariable *>(blk);
    if (unlikely(!cblock->target)) // call first if we have no target
    {
      previousOutput = blk->activate(blk, context, input);
    } else {
      previousOutput = *cblock->target;
    }
    return;
  }
  case CoreSwapVariables: {
    auto cblock = reinterpret_cast<CBCoreSwapVariables *>(blk);
    if (unlikely(!cblock->target1 ||
                 !cblock->target2)) // call first if we have no targets
    {
      previousOutput = blk->activate(
          blk, context, input); // ignore previousOutput since we pass input
    } else {
      auto tmp = *cblock->target1;
      *cblock->target1 = *cblock->target2;
      *cblock->target2 = tmp;
      previousOutput = input;
    }
    return;
  }
  case MathAdd: {
    auto cblock = reinterpret_cast<CBMathStub *>(blk);
    runChainINLINEMATH(+, "+") return;
  }
  case MathSubtract: {
    auto cblock = reinterpret_cast<CBMathStub *>(blk);
    runChainINLINEMATH(-, "-") return;
  }
  case MathMultiply: {
    auto cblock = reinterpret_cast<CBMathStub *>(blk);
    runChainINLINEMATH(*, "*") return;
  }
  case MathDivide: {
    auto cblock = reinterpret_cast<CBMathStub *>(blk);
    runChainINLINEMATH(/, "/") return;
  }
  case MathXor: {
    auto cblock = reinterpret_cast<CBMathStub *>(blk);
    runChainINLINE_INT_MATH(^, "^") return;
  }
  case MathAnd: {
    auto cblock = reinterpret_cast<CBMathStub *>(blk);
    runChainINLINE_INT_MATH(&, "&") return;
  }
  case MathOr: {
    auto cblock = reinterpret_cast<CBMathStub *>(blk);
    runChainINLINE_INT_MATH(|, "|") return;
  }
  case MathMod: {
    auto cblock = reinterpret_cast<CBMathStub *>(blk);
    runChainINLINE_INT_MATH(%, "%") return;
  }
  case MathLShift: {
    auto cblock = reinterpret_cast<CBMathStub *>(blk);
    runChainINLINE_INT_MATH(<<, "<<") return;
  }
  case MathRShift: {
    auto cblock = reinterpret_cast<CBMathStub *>(blk);
    runChainINLINE_INT_MATH(>>, ">>") return;
  }
  case MathAbs: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(fabs, fabsf, "Abs") return;
  }
  case MathExp: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(exp, expf, "Exp") return;
  }
  case MathExp2: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(exp2, exp2f, "Exp2") return;
  }
  case MathExpm1: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(expm1, expm1f, "Expm1") return;
  }
  case MathLog: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(log, logf, "Log") return;
  }
  case MathLog10: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(log10, log10f, "Log10") return;
  }
  case MathLog2: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(log2, log2f, "Log2") return;
  }
  case MathLog1p: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(log1p, log1pf, "Log1p") return;
  }
  case MathSqrt: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(sqrt, sqrtf, "Sqrt") return;
  }
  case MathCbrt: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(cbrt, cbrtf, "Cbrt") return;
  }
  case MathSin: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(sin, sinf, "Sin") return;
  }
  case MathCos: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(cos, cosf, "Cos") return;
  }
  case MathTan: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(tan, tanf, "Tan") return;
  }
  case MathAsin: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(asin, asinf, "Asin") return;
  }
  case MathAcos: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(acos, acosf, "Acos") return;
  }
  case MathAtan: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(atan, atanf, "Atan") return;
  }
  case MathSinh: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(sinh, sinhf, "Sinh") return;
  }
  case MathCosh: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(cosh, coshf, "Cosh") return;
  }
  case MathTanh: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(tanh, tanhf, "Tanh") return;
  }
  case MathAsinh: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(asinh, asinhf, "Asinh") return;
  }
  case MathAcosh: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(acosh, acoshf, "Acosh") return;
  }
  case MathAtanh: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(atanh, atanhf, "Atanh") return;
  }
  case MathErf: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(erf, erff, "Erf") return;
  }
  case MathErfc: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(erfc, erfcf, "Erfc") return;
  }
  case MathTGamma: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(tgamma, tgammaf, "TGamma") return;
  }
  case MathLGamma: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(lgamma, lgammaf, "LGamma") return;
  }
  case MathCeil: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(ceil, ceilf, "Ceil") return;
  }
  case MathFloor: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(floor, floorf, "Floor") return;
  }
  case MathTrunc: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(trunc, truncf, "Trunc") return;
  }
  case MathRound: {
    auto cblock = reinterpret_cast<CBMathUnaryStub *>(blk);
    runChainINLINECMATH(round, roundf, "Round") return;
  }
  default: // NotInline
  {
    previousOutput = blk->activate(blk, context, input);
    return;
  }
  }
}

inline static bool activateBlocks(CBlocks blocks, int nblocks,
                                  CBContext *context, const CBVar &chainInput,
                                  CBVar &output) {
  auto input = chainInput;
  for (auto i = 0; i < nblocks; i++) {
    activateBlock(blocks[i], context, input, output);
    input = output;
    if (output.valueType == None) {
      switch (output.payload.chainState) {
      case Restart: {
        return true;
      }
      case Stop: {
        return false;
      }
      case Continue:
        continue;
      }
    }
  }
  return true;
}

inline static bool activateBlocks(CBSeq blocks, CBContext *context,
                                  const CBVar &chainInput, CBVar &output) {
  auto input = chainInput;
  for (auto i = 0; i < stbds_arrlen(blocks); i++) {
    activateBlock(blocks[i].payload.blockValue, context, input, output);
    input = output;
    if (output.valueType == None) {
      switch (output.payload.chainState) {
      case Restart: {
        return true;
      }
      case Stop: {
        return false;
      }
      case Continue:
        continue;
      }
    }
  }
  return true;
}

inline static CBRunChainOutput runChain(CBChain *chain, CBContext *context,
                                        CBVar chainInput) {
  auto previousOutput = CBVar();

  // Detect and pause if we need to here
  // avoid pausing in the middle or so, that is for a proper debug mode runner,
  // here we care about performance
  while (context->shouldPause) {
    context->paused = true;

    auto suspendRes = suspend(context, 0.0);
    // Since we suspended we need to make sure we should continue when resuming
    switch (suspendRes.payload.chainState) {
    case Restart: {
      return {previousOutput, Restarted};
    }
    case Stop: {
      return {previousOutput, Stopped};
    }
    case Continue:
      continue;
    }
  }

  chain->started = true;
  context->paused = false;
  chain->context = context;

  for (auto blk : chain->blocks) {
    if (blk->preChain) {
      try {
        blk->preChain(blk, context);
      } catch (const std::exception &e) {
        LOG(ERROR) << "Pre chain failure, failed block: "
                   << std::string(blk->name(blk));
        if (context->error.length() > 0)
          LOG(ERROR) << "Last error: " << std::string(context->error);
        LOG(ERROR) << e.what();
        return {CBVar(), Failed};
      } catch (...) {
        LOG(ERROR) << "Pre chain failure, failed block: "
                   << std::string(blk->name(blk));
        if (context->error.length() > 0)
          LOG(ERROR) << "Last error: " << std::string(context->error);
        return {CBVar(), Failed};
      }
    }
  }

  auto input = chainInput;
  for (auto blk : chain->blocks) {
    try {
      activateBlock(blk, context, input, previousOutput);
      input = previousOutput;

      if (previousOutput.valueType == None) {
        switch (previousOutput.payload.chainState) {
        case Restart: {
          runChainPOSTCHAIN;
          return {previousOutput, Restarted};
        }
        case Stop: {
          runChainPOSTCHAIN;

          // Print errors if any, we might have stopped because of some error!
          if (unlikely(context->error.length() > 0)) {
            LOG(ERROR) << "Block activation error, failed block: "
                       << std::string(blk->name(blk));
            LOG(ERROR) << "Last error: " << std::string(context->error);
            return {previousOutput, Failed};
          } else {
            return {previousOutput, Stopped};
          }
        }
        case Continue:
          continue;
        }
      }
    } catch (const std::exception &e) {
      LOG(ERROR) << "Block activation error, failed block: "
                 << std::string(blk->name(blk));
      if (context->error.length() > 0)
        LOG(ERROR) << "Last error: " << std::string(context->error);
      LOG(ERROR) << e.what();
      ;
      runChainPOSTCHAIN;
      return {previousOutput, Failed};
    } catch (...) {
      LOG(ERROR) << "Block activation error, failed block: "
                 << std::string(blk->name(blk));
      if (context->error.length() > 0)
        LOG(ERROR) << "Last error: " << std::string(context->error);
      runChainPOSTCHAIN;
      return {previousOutput, Failed};
    }
  }

  runChainPOSTCHAIN;
  return {previousOutput, Running};
}

static void cleanup(CBChain *chain) {
  // Run cleanup on all blocks, prepare them for a new start if necessary
  // Do this in reverse to allow a safer cleanup
  for (auto it = chain->blocks.rbegin(); it != chain->blocks.rend(); ++it) {
    auto blk = *it;
    try {
      blk->cleanup(blk);
    } catch (const std::exception &e) {
      LOG(ERROR) << "Block cleanup error, failed block: "
                 << std::string(blk->name(blk));
      LOG(ERROR) << e.what() << '\n';
    } catch (...) {
      LOG(ERROR) << "Block cleanup error, failed block: "
                 << std::string(blk->name(blk));
    }
  }
}

static boost::context::continuation run(CBChain *chain,
                                        boost::context::continuation &&sink) {
  auto running = true;
  // Reset return state
  chain->returned = false;
  // Clean previous output if we had one
  if (chain->ownedOutput) {
    destroyVar(chain->finishedOutput);
    chain->ownedOutput = false;
  }
  // Reset error
  chain->failed = false;
  // Create a new context and copy the sink in
  CBContext context(std::move(sink), chain);

  // We prerolled our coro, suspend here before actually starting.
  // This allows us to allocate the stack ahead of time.
  context.continuation = context.continuation.resume();
  if (context.aborted) // We might have stopped before even starting!
    goto endOfChain;

  while (running) {
    running = chain->looped;
    context.restarted = false; // Remove restarted flag

    chain->finished = false; // Reset finished flag (atomic)
    auto runRes = runChain(chain, &context, chain->rootTickInput);
    chain->finishedOutput = runRes.output; // Write result before setting flag
    chain->finished = true;                // Set finished flag (atomic)
    if (runRes.state == Failed) {
      chain->failed = true;
      context.aborted = true;
      break;
    }

    if (!chain->unsafe && chain->looped) {
      // Ensure no while(true), yield anyway every run
      context.next = Duration(0);
      context.continuation = context.continuation.resume();
      // This is delayed upon continuation!!
      if (context.aborted)
        break;
    }
  }

endOfChain:
  // Copy the output variable since the next call might wipe it
  auto tmp = chain->finishedOutput;
  chain->finishedOutput =
      CBVar(); // Reset it we are not sure on the internal state
  chain->ownedOutput = true;
  cloneVar(chain->finishedOutput, tmp);

  // run cleanup on all the blocks
  cleanup(chain);

  // Need to take care that we might have stopped the chain very early due to
  // errors and the next eventual stop() should avoid resuming
  chain->returned = true;
  return std::move(context.continuation);
}

static void prepare(CBChain *chain) {
  if (chain->coro)
    return;

  chain->coro = new CBCoro(
      boost::context::callcc([&chain](boost::context::continuation &&sink) {
        return run(chain, std::move(sink));
      }));
}

static void start(CBChain *chain, CBVar input = {}) {
  if (!chain->coro || !(*chain->coro) || chain->started)
    return; // check if not null and bool operator also to see if alive!

  chainblocks::cloneVar(chain->rootTickInput, input);
  *chain->coro = chain->coro->resume();
}

static bool stop(CBChain *chain, CBVar *result = nullptr) {
  // Clone the results if we need them
  if (result)
    cloneVar(*result, chain->finishedOutput);

  if (chain->coro) {
    // Run until exit if alive, need to propagate to all suspended blocks!
    if ((*chain->coro) && !chain->returned) {
      // set abortion flag, we always have a context in this case
      chain->context->aborted = true;

      // BIG Warning: chain->context existed in the coro stack!!!
      // after this resume chain->context is trash!
      chain->coro->resume();
    }

    // delete also the coro ptr
    delete chain->coro;
    chain->coro = nullptr;
  } else {
    // if we had a coro this will run inside it!
    cleanup(chain);
  }

  chain->started = false;

  if (chain->failed) {
    return false;
  }

  return true;
}

static bool tick(CBChain *chain, CBVar rootInput = CBVar()) {
  if (!chain->context || !chain->coro || !(*chain->coro) || chain->returned ||
      !chain->started)
    return false; // check if not null and bool operator also to see if alive!

  Duration now = Clock::now().time_since_epoch();
  if (now >= chain->context->next) {
    chain->rootTickInput = rootInput;
    *chain->coro = chain->coro->resume();
  }
  return true;
}

static bool isRunning(CBChain *chain) {
  return chain->started && !chain->returned;
}

static bool hasEnded(CBChain *chain) {
  return chain->started && chain->returned;
}

static bool isCanceled(CBContext *context) { return context->aborted; }

static std::string store(CBVar var) {
  json jsonVar = var;
  return jsonVar.dump();
}

static void load(CBVar &var, const char *jsonStr) {
  auto j = json::parse(jsonStr);
  var = j.get<CBVar>();
}

static std::string store(CBChainPtr chain) {
  json jsonChain = chain;
  return jsonChain.dump();
}

static void load(CBChainPtr &chain, const char *jsonStr) {
  auto j = json::parse(jsonStr);
  chain = j.get<CBChainPtr>();
}

static void sleep(double seconds = -1.0) {
  // negative = no sleep, just run callbacks
  if (seconds >= 0) {
#ifdef _WIN32
    HANDLE timer;
    LARGE_INTEGER ft;
    ft.QuadPart = -(int64_t(seconds * 10000000));
    timer = CreateWaitableTimer(NULL, TRUE, NULL);
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
#else
    struct timespec delay = {0, int64_t(seconds * 1000000000)};
    while (nanosleep(&delay, &delay))
      ;
#endif
  }

  // Run loop callbacks after sleeping
  for (auto &cbinfo : RunLoopHooks) {
    if (cbinfo.second) {
      cbinfo.second();
    }
  }
}
}; // namespace chainblocks

struct CBNode {
  ~CBNode() { terminate(); }

  void schedule(CBChain *chain, CBVar input = {}, bool validate = true) {
    if (chain->node)
      throw chainblocks::CBException(
          "schedule failed, chain was already scheduled!");

    // Validate the chain
    if (validate) {
      validateConnections(
          chain->blocks,
          [](const CBlock *errorBlock, const char *errorTxt,
             bool nonfatalWarning, void *userData) {
            auto node = reinterpret_cast<CBNode *>(userData);
            auto blk = const_cast<CBlock *>(errorBlock);
            if (!nonfatalWarning) {
              node->errorMsg.assign(errorTxt);
              node->errorMsg += ", input block: " + std::string(blk->name(blk));
              throw chainblocks::CBException(node->errorMsg.c_str());
            } else {
              LOG(INFO) << "Validation warning: " << errorTxt
                        << " input block: " << blk->name(blk);
            }
          },
          this);
    }

    chains.push_back(chain);
    chain->node = this;
    chainblocks::prepare(chain);
    chainblocks::start(chain, input);
  }

  bool tick(CBVar input = {}) {
    auto noErrors = true;
    chainsTicking = chains;
    for (auto chain : chainsTicking) {
      chainblocks::tick(chain, input);
      if (!chainblocks::isRunning(chain)) {
        if (!chainblocks::stop(chain)) {
          noErrors = false;
        }
        chains.remove(chain);
        chain->node = nullptr;
      }
    }
    return noErrors;
  }

  void terminate() {
    for (auto chain : chains) {
      chainblocks::stop(chain);
      chain->node = nullptr;
    }
    chains.clear();
  }

  void remove(CBChain *chain) {
    chainblocks::stop(chain);
    chains.remove(chain);
    chain->node = nullptr;
  }

  bool empty() { return chains.empty(); }

private:
  std::list<CBChain *> chains;
  std::list<CBChain *> chainsTicking;
  std::string errorMsg;
};

#endif // CHAINBLOCKS_RUNTIME
