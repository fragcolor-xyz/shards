/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#ifndef CB_CHAINBLOCKS_H
#define CB_CHAINBLOCKS_H

// Use only basic types and libs, we want full ABI compatibility between blocks
// Cannot afford to use any C++ std as any block maker should be free to use
// their versions

#include <stdbool.h>
#include <stdint.h>

// Included 3rdparty
#ifdef USE_RPMALLOC
#include "rpmalloc/rpmalloc.h"
inline void *rp_init_realloc(void *ptr, size_t size) {
  rpmalloc_initialize();
  return rprealloc(ptr, size);
}
#define STBDS_REALLOC(context, ptr, size) rp_init_realloc(ptr, size)
#define STBDS_FREE(context, ptr) rpfree(ptr)
#endif
#include "stb_ds.h"

// All the available types
enum CBType : uint8_t {
  None,
  Any,
  Object,
  Enum,
  Bool,
  Int,    // A 64bits int
  Int2,   // A vector of 2 64bits ints
  Int3,   // A vector of 3 32bits ints
  Int4,   // A vector of 4 32bits ints
  Int8,   // A vector of 8 16bits ints
  Int16,  // A vector of 16 8bits ints
  Float,  // A 64bits float
  Float2, // A vector of 2 64bits floats
  Float3, // A vector of 3 32bits floats
  Float4, // A vector of 4 32bits floats
  Color,  // A vector of 4 uint8
  Chain,  // sub chains, e.g. IF/ELSE
  Block,  // a block, useful for future introspection blocks!

  EndOfBlittableTypes, // anything below this is not blittable (not exactly but
                       // for cloneVar mostly)

  Bytes, // pointer + size
  String,
  ContextVar, // A string label to find from CBContext variables
  Image,
  Seq,
  Table
};

enum CBChainState : uint8_t {
  Continue, // Nothing happened, continue
  Rebase,   // Continue this chain but put the local chain initial input as next
            // input
  Restart,  // Restart the local chain from the top (notice not the root!)
  Return,   // Used in conditional paths, end this chain and return previous
            // output
  Stop,     // Stop the chain execution (including root)
};

// These blocks run fully inline in the runchain threaded execution engine
enum CBInlineBlocks : uint8_t {
  NotInline,

  StackPush,
  StackPop,
  StackSwap,
  StackDrop,

  CoreConst,
  CoreSleep,
  CoreInput,
  CoreStop,
  CoreRestart,
  CoreRepeat,
  CoreGet,
  CoreSet,
  CoreUpdate,
  CoreSwap,
  CoreTakeSeq,
  CoreTakeInts,
  CoreTakeFloats,
  CoreTakeColor,
  CoreTakeBytes,
  CoreTakeString,
  CorePush,
  CoreIs,
  CoreIsNot,
  CoreAnd,
  CoreOr,
  CoreNot,
  CoreIsMore,
  CoreIsLess,
  CoreIsMoreEqual,
  CoreIsLessEqual,

  MathAdd,
  MathSubtract,
  MathMultiply,
  MathDivide,
  MathXor,
  MathAnd,
  MathOr,
  MathMod,
  MathLShift,
  MathRShift,

  MathAbs,
  MathExp,
  MathExp2,
  MathExpm1,
  MathLog,
  MathLog10,
  MathLog2,
  MathLog1p,
  MathSqrt,
  MathCbrt,
  MathSin,
  MathCos,
  MathTan,
  MathAsin,
  MathAcos,
  MathAtan,
  MathSinh,
  MathCosh,
  MathTanh,
  MathAsinh,
  MathAcosh,
  MathAtanh,
  MathErf,
  MathErfc,
  MathTGamma,
  MathLGamma,
  MathCeil,
  MathFloor,
  MathTrunc,
  MathRound,
};

typedef void *CBArray;

// Forward declarations
struct CBVar;
typedef struct CBSeq {
  CBVar *elements;
  // This (32bit sizes) is for obvious packing reason, sizeof(CBVar) == 32
  // But also for compatibility, chainblocks supports 32bit systems
  // So the max array size should be INT32_SIZE indeed
  uint32_t len;
  uint32_t cap;
} CBSeq;

struct CBNamedVar;
typedef struct CBNamedVar *CBTable; // a stb string map

struct CBChain;
typedef struct CBChain *CBChainPtr;

struct CBChainProvider;

struct CBContext;

struct CBNode;
struct CBFlow;

struct CBlock;
typedef struct CBlock *CBlockRef;
typedef struct CBlock **CBlocks; // a stb array

struct CBTypeInfo;
typedef struct CBTypeInfo *CBTypesInfo; // a stb array

struct CBParameterInfo;
typedef struct CBParameterInfo *CBParametersInfo; // a stb array

struct CBExposedTypeInfo;
typedef struct CBExposedTypeInfo *CBExposedTypesInfo; // a stb array

typedef void *CBPointer;
typedef int64_t CBInt;
typedef double CBFloat;
typedef bool CBBool;
typedef int32_t CBEnum;
typedef const char *CBString;
typedef CBString *CBStrings; // a stb array

#if defined(__clang__) || defined(__GNUC__)
#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#ifdef __clang__
#define shufflevector __builtin_shufflevector
#else
#define shufflevector __builtin_shuffle
#endif

typedef int64_t CBInt2 __attribute__((vector_size(16)));
typedef int32_t CBInt3 __attribute__((vector_size(16)));
typedef int32_t CBInt4 __attribute__((vector_size(16)));
typedef int16_t CBInt8 __attribute__((vector_size(16)));
typedef int8_t CBInt16 __attribute__((vector_size(16)));

typedef double CBFloat2 __attribute__((vector_size(16)));
typedef float CBFloat3 __attribute__((vector_size(16)));
typedef float CBFloat4 __attribute__((vector_size(16)));

#define ALIGNED

#ifdef NDEBUG
#define ALWAYS_INLINE __attribute__((always_inline))
#else
#define ALWAYS_INLINE
#endif

#define PACK(__Declaration__) __Declaration__ __attribute__((__packed__))
#else // TODO
typedef int64_t CBInt2[2];
typedef int32_t CBInt3[3];
typedef int32_t CBInt4[4];
typedef int16_t CBInt8[8];
typedef int8_t CBInt16[16];

typedef double CBFloat2[2];
typedef float CBFloat3[3];
typedef float CBFloat4[4];

#define ALIGNED __declspec(align(16))

#define ALWAYS_INLINE

#define PACK(__Declaration__)                                                  \
  __pragma(pack(push, 1)) __Declaration__ __pragma(pack(pop))
#endif

#ifndef _WIN32
#ifdef I386_BUILD
#define __cdecl __attribute__((__cdecl__))
#else
#define __cdecl
#endif
#endif

PACK(struct _uint48_t {
  PACK(union {
    PACK(struct {
      uint32_t _x_u32;
      uint16_t _y_u16;
    });

#ifndef NO_BITFIELDS
    uint64_t value : 48;
#endif
  });
});

typedef struct _uint48_t uint48_t;

struct CBColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
};

// None = RGBA
#define CBIMAGE_FLAGS_NONE (0)
#define CBIMAGE_FLAGS_BGRA (1 << 0)

struct CBImage {
  uint16_t width;
  uint16_t height;
  uint8_t channels;
  uint8_t flags;
  uint8_t *data;
};

struct CBTypeInfo {
  enum CBType basicType;

  union {
    struct {
      int32_t objectVendorId;
      int32_t objectTypeId;
    };

    struct {
      int32_t enumVendorId;
      int32_t enumTypeId;
    };

    // If we are a simpe seq a pointer to the possible single type present in
    // this seq NULL if this is a any type seq, users must guard the outputs
    // with ExpectX type
    struct CBTypeInfo *seqType;

    // If we are a table, the possible types present in this table
    struct {
      CBStrings tableKeys; // todo, clarify/fix
      CBTypesInfo tableTypes;
    };

    CBTypesInfo contextVarTypes;
  };
};

typedef const char *(__cdecl *CBObjectSerializer)(CBPointer);
typedef CBPointer(__cdecl *CBObjectDeserializer)(const char *);
typedef CBPointer(__cdecl *CBObjectDestroy)(CBPointer);

struct CBObjectInfo {
  const char *name;
  CBObjectSerializer serialize;
  CBObjectDeserializer deserialize;
  CBObjectDestroy destroy;
};

struct CBEnumInfo {
  const char *name;
  CBStrings labels;
};

struct CBParameterInfo {
  const char *name;
  const char *help;
  CBTypesInfo valueTypes;
};

struct CBExposedTypeInfo {
  const char *name;
  const char *help;
  struct CBTypeInfo exposedType;
  bool isMutable;
};

struct CBValidationResult {
  struct CBTypeInfo outputType;
  CBExposedTypesInfo exposedInfo;
};

struct CBFlow {
  struct CBChain *chain;
};

// # Of CBVars and memory

// ## Specifically String and Seq types

// ### Blocks need to maintain their own garbage, in a way so that any reciver
// of CBVar/s will not have to worry about it.

// #### In the case of getParam:
//   * the callee should allocate and preferably cache any String or Seq that
// needs to return.
//   * the callers will just read and should not modify the contents.
// #### In the case of activate:
//   * The input var memory is owned by the previous block.
//   * The output var memory is owned by the activating block.
//   * The activating block will have to manage any CBVar that puts in the stack
// or context variables as well!
// #### In the case of setParam:
//   * If the block needs to store the String or Seq data it will then need to
// deep copy it.
//   * Callers should free up any allocated memory.

// ### What about exposed/consumedVariables, parameters and input/outputTypes:
// * Same for them, they are just read only basically

ALIGNED struct CBVarPayload {
  union {
    enum CBChainState chainState;

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
    CBInt8 int8Value;
    CBInt16 int16Value;

    CBFloat floatValue;
    CBFloat2 float2Value;
    CBFloat3 float3Value;
    CBFloat4 float4Value;

    // notice, this is assumed not mutable!
    // unless specified in the CBTypeInfo of this value
    // and only when used as variable (consumable)
    CBSeq seqValue;

    CBTable tableValue;

    struct {
      CBString stringValue;
      // If ContextVar and stringValue == nullptr
      // assume we use the context stack if pos < 0
      // where -1 == stack top
      int64_t stackPosition;
    };

    struct CBColor colorValue;

    struct CBImage imageValue;

    CBChainPtr chainValue;

    struct CBlock *blockValue;

    struct {
      CBEnum enumValue;
      int32_t enumVendorId;
      int32_t enumTypeId;
    };

    struct {
      uint8_t *bytesValue;
      uint64_t bytesSize;
    };
  };
};

struct CBVar {
  struct CBVarPayload payload;

  union {
    int64_t _reserved;
  };

  enum CBType valueType;

  // Used by serialization/clone routines to keep track of actual storage
  // capacity 48 bits should be plenty for such sizes
  uint48_t capacity;
};

struct CBNamedVar {
  const char *key;
  struct CBVar value;
};

enum CBRunChainOutputState { Running, Restarted, Stopped, Failed };

struct CBRunChainOutput {
  struct CBVar output;
  enum CBRunChainOutputState state;
};

struct CBInstanceData {
  // Used to optimize activations, replacing function pointers during
  // bake/validation
  struct CBlock *block;

  // Info related to our activation
  struct CBTypeInfo inputType;
  CBTypesInfo stack;
  CBExposedTypesInfo consumables;
};

typedef struct CBlock *(__cdecl *CBBlockConstructor)();
typedef void(__cdecl *CBCallback)();

typedef const char *(__cdecl *CBNameProc)(struct CBlock *);
typedef const char *(__cdecl *CBHelpProc)(struct CBlock *);

// Construction/Destruction
typedef void(__cdecl *CBSetupProc)(struct CBlock *);
typedef void(__cdecl *CBDestroyProc)(struct CBlock *);

typedef CBTypesInfo(__cdecl *CBInputTypesProc)(struct CBlock *);
typedef CBTypesInfo(__cdecl *CBOutputTypesProc)(struct CBlock *);

typedef CBExposedTypesInfo(__cdecl *CBExposedVariablesProc)(struct CBlock *);
typedef CBExposedTypesInfo(__cdecl *CBConsumedVariablesProc)(struct CBlock *);

typedef CBParametersInfo(__cdecl *CBParametersProc)(struct CBlock *);
typedef void(__cdecl *CBSetParamProc)(struct CBlock *, int, struct CBVar);
typedef struct CBVar(__cdecl *CBGetParamProc)(struct CBlock *, int);

typedef struct CBTypeInfo(__cdecl *CBComposeProc)(struct CBlock *,
                                                  struct CBInstanceData data);

// The core of the block processing, avoid syscalls here
typedef struct CBVar(__cdecl *CBActivateProc)(struct CBlock *,
                                              struct CBContext *,
                                              const struct CBVar *);

// Generally when stop() is called
typedef void(__cdecl *CBCleanupProc)(struct CBlock *);

struct CBlock {
  enum CBInlineBlocks inlineBlockId;

  CBNameProc name; // Returns the name of the block, do not free the string,
                   // generally const
  CBHelpProc help; // Returns the help text of the block, do not free the
                   // string, generally const

  CBSetupProc setup;     // A one time construtor setup for the block
  CBDestroyProc destroy; // A one time finalizer for the block, blocks should
                         // also free all the memory in here!

  CBInputTypesProc inputTypes;
  CBOutputTypesProc outputTypes;

  CBExposedVariablesProc exposedVariables;
  CBConsumedVariablesProc consumedVariables;

  // Optional call used during validation to fixup "Any" input
  // type and provide valid output and exposed variable types
  CBComposeProc compose;

  CBParametersProc parameters;
  CBSetParamProc setParam; // Set a parameter, the block will copy the value, so
                           // if you allocated any memory you should free it
  CBGetParamProc getParam; // Gets a parameter, the block is the owner of any
                           // allocated stuff, DO NOT free them

  CBActivateProc activate;
  CBCleanupProc cleanup; // Called every time you stop a coroutine or sometimes
                         // internally to clean up the block state
};

struct CBChainProviderUpdate {
  const char *error;     // if any or nullptr
  struct CBChain *chain; // or nullptr if error
};

typedef void(__cdecl *CBProviderReset)(struct CBChainProvider *provider);

typedef bool(__cdecl *CBProviderReady)(struct CBChainProvider *provider);
typedef void(__cdecl *CBProviderSetup)(struct CBChainProvider *provider,
                                       const char *path,
                                       struct CBInstanceData data);

typedef bool(__cdecl *CBProviderUpdated)(struct CBChainProvider *provider);
typedef struct CBChainProviderUpdate(__cdecl *CBProviderAcquire)(
    struct CBChainProvider *provider);

typedef void(__cdecl *CBProviderReleaseChain)(struct CBChainProvider *provider,
                                              struct CBChain *chain);

struct CBChainProvider {
  CBProviderReset reset;

  CBProviderReady ready;
  CBProviderSetup setup;

  CBProviderUpdated updated;
  CBProviderAcquire acquire;

  CBProviderReleaseChain release;

  void *userData;
};

typedef void(__cdecl *CBValidationCallback)(const struct CBlock *errorBlock,
                                            const char *errorTxt,
                                            bool nonfatalWarning,
                                            void *userData);

typedef void(__cdecl *CBRegisterBlock)(const char *fullName,
                                       CBBlockConstructor constructor);

typedef void(__cdecl *CBRegisterObjectType)(int32_t vendorId, int32_t typeId,
                                            struct CBObjectInfo info);

typedef void(__cdecl *CBRegisterEnumType)(int32_t vendorId, int32_t typeId,
                                          struct CBEnumInfo info);

typedef void(__cdecl *CBRegisterRunLoopCallback)(const char *eventName,
                                                 CBCallback callback);

typedef void(__cdecl *CBRegisterExitCallback)(const char *eventName,
                                              CBCallback callback);

typedef void(__cdecl *CBUnregisterRunLoopCallback)(const char *eventName);

typedef void(__cdecl *CBUnregisterExitCallback)(const char *eventName);

typedef struct CBVar *(__cdecl *CBContextVariable)(struct CBContext *context,
                                                   const char *name);

typedef void(__cdecl *CBThrowException)(const char *errorText);

typedef struct CBVar(__cdecl *CBSuspend)(struct CBContext *context,
                                         double seconds);

typedef void(__cdecl *CBCloneVar)(struct CBVar *dst, const struct CBVar *src);

typedef void(__cdecl *CBDestroyVar)(struct CBVar *var);

typedef struct CBValidationResult(__cdecl *CBValidateChain)(
    struct CBChain *chain, CBValidationCallback callback, void *userData,
    struct CBInstanceData data);

typedef struct CBRunChainOutput(__cdecl *CBRunChain)(struct CBChain *chain,
                                                     struct CBContext *context,
                                                     struct CBVar input);

typedef struct CBValidationResult(__cdecl *CBValidateBlocks)(
    CBlocks blocks, CBValidationCallback callback, void *userData,
    struct CBInstanceData data);

typedef struct CBVar(__cdecl *CBRunBlocks)(CBlocks blocks,
                                           struct CBContext *context,
                                           struct CBVar input);

typedef void(__cdecl *CBLog)(const char *msg);

typedef struct CBlock *(__cdecl *CBCreateBlock)(const char *name);

typedef struct CBChain *(__cdecl *CBCreateChain)(const char *name,
                                                 CBlocks blocks, bool looped,
                                                 bool unsafe);
typedef void(__cdecl *CBDestroyChain)(struct CBChain *chain);

typedef struct CBNode *(__cdecl *CBCreateNode)();
typedef void(__cdecl *CBDestroyNode)(struct CBNode *chain);
typedef void(__cdecl *CBSchedule)(struct CBNode *node, struct CBChain *chain);
typedef void(__cdecl *CBTick)(struct CBNode *node);
typedef void(__cdecl *CBSleep)(double seconds, bool runCallbacks);

#define CB_ARRAY_TYPE(_array_, _value_)                                        \
  typedef _array_(__cdecl *_array_##Push)(_array_, const _value_ *);           \
  typedef _array_(__cdecl *_array_##Insert)(_array_, uint64_t,                 \
                                            const _value_ *);                  \
  typedef _value_(__cdecl *_array_##Pop)(_array_);                             \
  typedef _array_(__cdecl *_array_##Resize)(_array_, uint64_t);                \
  typedef void(__cdecl * _array_##FastDelete)(_array_, uint64_t);              \
  typedef void(__cdecl * _array_##SlowDelete)(_array_, uint64_t)

CB_ARRAY_TYPE(CBSeq, struct CBVar);
CB_ARRAY_TYPE(CBTypesInfo, struct CBTypeInfo);
CB_ARRAY_TYPE(CBParametersInfo, struct CBParameterInfo);
CB_ARRAY_TYPE(CBlocks, CBlockRef);
CB_ARRAY_TYPE(CBExposedTypesInfo, struct CBExposedTypeInfo);

#define CB_ARRAY_PROCS(_array_, _short_)                                       \
  _array_##Push _short_##Push;                                                 \
  _array_##Insert _short_##Insert;                                             \
  _array_##Pop _short_##Pop;                                                   \
  _array_##Resize _short_##Resize;                                             \
  _array_##FastDelete _short_##FastDelete;                                     \
  _array_##SlowDelete _short_##SlowDelete

// CB dynamic arrays interface
typedef void(__cdecl *CBArrayFree)(CBArray);
typedef uint64_t(__cdecl *CBArrayLength)(CBArray);

typedef const char *(__cdecl *CBGetRootPath)();
typedef void(__cdecl *CBSetRootPath)(const char *);

struct CBCore {
  // Adds a block to the runtime database
  CBRegisterBlock registerBlock;
  // Adds a custom object type to the runtime database
  CBRegisterObjectType registerObjectType;
  // Adds a custom enumeration type to the runtime database
  CBRegisterEnumType registerEnumType;

  // Adds a custom call to call every chainblocks sleep/yield internally
  // These call will run on a single thread, usually the main, but they are safe
  // due to the fact it runs after all Nodes ticked once
  CBRegisterRunLoopCallback registerRunLoopCallback;
  // Removes a previously added run loop callback
  CBUnregisterRunLoopCallback unregisterRunLoopCallback;

  // Adds a custom call to be called on final application exit
  CBRegisterExitCallback registerExitCallback;
  // Removes a previously added exit callback
  CBUnregisterExitCallback unregisterExitCallback;

  // To be used within blocks, to fetch context variables
  CBContextVariable findVariable;
  // Can be used to propagate block errors
  CBThrowException throwException;
  // To be used within blocks, to suspend the coroutine
  CBSuspend suspend;

  // Utility to deal with CBVars
  CBCloneVar cloneVar;
  CBDestroyVar destroyVar;

  // Utility to deal with CB dynamic arrays
  CBArrayLength arrayLength;
  CBArrayFree arrayFree;

  // Utility to deal with CBSeqs
  CB_ARRAY_PROCS(CBSeq, seq);

  // Utility to deal with CBTypesInfo
  CB_ARRAY_PROCS(CBTypesInfo, types);

  // Utility to deal with CBParamsInfo
  CB_ARRAY_PROCS(CBParametersInfo, params);

  // Utility to deal with CBlocks
  CB_ARRAY_PROCS(CBlocks, blocks);

  // Utility to deal with CBExposedTypeInfo
  CB_ARRAY_PROCS(CBExposedTypesInfo, expTypes);

  // Utility to use blocks within blocks
  CBValidateChain validateChain;
  CBRunChain runChain;
  CBValidateBlocks validateBlocks;
  CBRunBlocks runBlocks;

  // Logging
  CBLog log;

  // Chain creation
  CBCreateBlock createBlock;

  CBCreateChain createChain;
  CBDestroyChain destroyChain;

  // Chain scheduling
  CBCreateNode createNode;
  CBDestroyNode destroyNode;
  CBSchedule schedule;
  CBTick tick;
  CBSleep sleep;

  // Environment utilities
  CBGetRootPath getRootPath;
  CBSetRootPath setRootPath;
};

typedef struct CBCore(__cdecl *CBChainblocksInterface)(uint32_t abi_version);

#ifdef _WIN32
#ifdef CB_DLL_EXPORT
#define EXPORTED __declspec(dllexport)
#elif defined(CB_DLL_IMPORT)
#define EXPORTED __declspec(dllimport)
#else
#define EXPORTED
#endif
#else
#ifdef CB_DLL_EXPORT
#define EXPORTED __attribute__((visibility("default")))
#else
#define EXPORTED
#endif
#endif

#define CHAINBLOCKS_CURRENT_ABI 0x20200101

#ifdef __cplusplus
extern "C" {
#endif
EXPORTED struct CBCore __cdecl chainblocksInterface(uint32_t abi_version);
#ifdef __cplusplus
};
#endif

// For future proper use
// Basically we want during development of chains to have those on
// When baking exe/lib/release we turn them off
#define cbassert assert
#ifndef NDEBUG
#define CB_DEBUG_MODE 1
#else
#define CB_DEBUG_MODE 0
#endif
#define cb_debug_only(__CODE__)                                                \
  if (CB_DEBUG_MODE) {                                                         \
    __CODE__;                                                                  \
  }

#endif
