/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#ifndef CB_CHAINBLOCKS_H
#define CB_CHAINBLOCKS_H

#include <stdbool.h> // bool
#include <stddef.h>  // size_t
#include <stdint.h>  // ints

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
  Path,       // An OS filesystem path
  ContextVar, // A string label to find from CBContext variables
  Image,
  Seq,
  Table,
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

// This (32bit sizes) is for obvious packing reason, sizeof(CBVar) == 32
// But also for compatibility, chainblocks supports 32bit systems
// So the max array size should be INT32_SIZE indeed
// Also most of array index operators in c++ use int
#define CB_ARRAY_DECL(_seq_, _val_)                                            \
  typedef struct _seq_ {                                                       \
    _val_ *elements;                                                           \
    uint32_t len;                                                              \
    uint32_t cap;                                                              \
  } _seq_

// Forward declarations
struct CBVar;
CB_ARRAY_DECL(CBSeq, struct CBVar);

typedef void *CBTableIterator;
struct CBTableInterface;
struct CBTable {
  void *opaque;
  struct CBTableInterface *api;
};

struct CBChain;
typedef struct CBChain *CBChainPtr;

struct CBChainProvider;

struct CBContext;

struct CBNode;

struct CBFlow;

struct CBlock;
typedef struct CBlock *CBlockPtr;
CB_ARRAY_DECL(CBlocks, CBlockPtr);

struct CBTypeInfo;
CB_ARRAY_DECL(CBTypesInfo, struct CBTypeInfo);

struct CBParameterInfo;
CB_ARRAY_DECL(CBParametersInfo, struct CBParameterInfo);

struct CBExposedTypeInfo;
CB_ARRAY_DECL(CBExposedTypesInfo, struct CBExposedTypeInfo);

typedef void *CBPointer;
typedef int64_t CBInt;
typedef double CBFloat;
typedef bool CBBool;
typedef int32_t CBEnum;

typedef const char *CBString;
CB_ARRAY_DECL(CBStrings, CBString);

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

#ifdef NDEBUG
#define ALWAYS_INLINE __attribute__((always_inline))
#define NO_INLINE __attribute__((noinline))
#else
#define ALWAYS_INLINE
#define NO_INLINE
#endif

#else // TODO
typedef int64_t CBInt2[2];
typedef int32_t CBInt3[3];
typedef int32_t CBInt4[4];
typedef int16_t CBInt8[8];
typedef int8_t CBInt16[16];

typedef double CBFloat2[2];
typedef float CBFloat3[3];
typedef float CBFloat4[4];

#define ALWAYS_INLINE
#define NO_INLINE

#endif

#ifndef _WIN32
#ifdef CPUBITS32
#define __cdecl __attribute__((__cdecl__))
#else
#define __cdecl
#endif
#endif

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

struct CBAudio {
  float sampleRate;
  uint16_t nsamples;
  uint16_t channels;
  float *samples;
};

// return false to abort iteration
typedef bool(__cdecl *CBTableForEachCallback)(const char *key,
                                              struct CBVar *value,
                                              void *userData);
// table interface
typedef void(__cdecl *CBTableForEach)(struct CBTable table,
                                      CBTableForEachCallback cb,
                                      void *userData);
typedef size_t(__cdecl *CBTableSize)(struct CBTable table);
typedef bool(__cdecl *CBTableContains)(struct CBTable table, const char *key);
typedef struct CBVar *(__cdecl *CBTableAt)(struct CBTable table,
                                           const char *key);
typedef void(__cdecl *CBTableRemove)(struct CBTable table, const char *key);
typedef void(__cdecl *CBTableClear)(struct CBTable table);
typedef void(__cdecl *CBTableFree)(struct CBTable table);

struct CBTableInterface {
  CBTableForEach tableForEach;
  CBTableSize tableSize;
  CBTableContains tableContains;
  CBTableAt tableAt;
  CBTableRemove tableRemove;
  CBTableClear tableClear;
  CBTableFree tableFree;
};

#ifdef CB_NO_ANON
#define CB_STRUCT(_name_) struct _name_
#define CB_UNION(_name_) union _name_
#define CB_UNION_NAME(_name_) _name_
#else
#define CB_STRUCT(_name_) struct
#define CB_UNION(_name_) union
#define CB_UNION_NAME(_name_)
#endif

struct CBTypeInfo {
  enum CBType basicType;

  CB_UNION(Details) {
    CB_STRUCT(Object) {
      int32_t vendorId;
      int32_t typeId;
    }
    object;

    CB_STRUCT(Enum) {
      int32_t vendorId;
      int32_t typeId;
    }
    enumeration;

    CBTypesInfo seqTypes;

    CB_STRUCT(Table) {
      // If tableKeys is populated, it is expected that
      // tableTypes will be populated as well and that at the same
      // key index there is the key's type
      CBStrings keys;
      // If tableKeys is not populated, len == 0 and tableKeys is populated len
      // > 0 it is assumed that tableTypes contains a sequence with the possible
      // types in the table
      CBTypesInfo types;
    }
    table;

    CBTypesInfo contextVarTypes;

    CB_STRUCT(Path) {
      // if is File, the extension allowed
      CBStrings extensions;
      // expects a path to a file
      bool isFile;
      // expects an existing path
      bool existing;
      // expects a relative path (relative to the CBCore.getRootPath)
      bool relative;
    }
    path;

    // Not meant for chain runtime checks
    // rather validation/compose checks of params and mutations
    CB_STRUCT(Integers) {
      int64_t min;
      int64_t max;
      bool valid;
    }
    integers;

    // Not meant for chain runtime checks
    // rather validation/compose checks of params and mutations
    CB_STRUCT(Real) {
      double min;
      double max;
      bool valid;
    }
    real;
  }
  CB_UNION_NAME(details);
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
  // if isTableEntry is true:
  // `name` will be the name of the table variable
  // `exposedType` will be of `Table` type
  // and `tableKeys` will contain the record's key name
  // while `tableTypes` the record's type
  bool isTableEntry;
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

// ### What about exposed/requiredVariables, parameters and input/outputTypes:
// * Same for them, they are just read only basically

struct CBVarPayload {
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
    // and only when used as variable (shared)
    CBSeq seqValue;

    struct CBTable tableValue;

    struct {
      CBString stringValue;
      // If ContextVar and stringValue == nullptr
      // assume we use the context stack if pos < 0
      // where -1 == stack top
      uint32_t stackPosition;
      // If ContextVar, signals a chain variable rather then node
      bool chainLocal;
    };

    struct CBColor colorValue;

    struct CBImage imageValue;

    // TODO
    struct CBAudio audioValue;

    CBChainPtr chainValue;

    CBlockPtr blockValue;

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
} __attribute__((aligned(16)));

struct CBVar {
  struct CBVarPayload payload;
  union {
    // currently used when dealing with Object type variables
    // if you need (serialization & destroy) you should populate this field
    struct CBObjectInfo *objectInfo;
    // currently used internally when serializing
    uint64_t capacity;
  };
  uint32_t refcount;
  enum CBType valueType;
} __attribute__((aligned(16)));

enum CBRunChainOutputState { Running, Restarted, Stopped, Failed };

struct CBRunChainOutput {
  struct CBVar output;
  enum CBRunChainOutputState state;
} __attribute__((aligned(16)));

struct CBInstanceData {
  // Used to optimize activations, replacing function pointers during
  // bake/validation
  struct CBlock *block;

  // Info related to our activation
  struct CBTypeInfo inputType;
  CBTypesInfo stack;
  CBExposedTypesInfo shared;
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
typedef CBExposedTypesInfo(__cdecl *CBRequiredVariablesProc)(struct CBlock *);

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

  // The interface to fill
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
  CBRequiredVariablesProc requiredVariables;

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

typedef struct CBVar *(__cdecl *CBReferenceVariable)(struct CBContext *context,
                                                     const char *name,
                                                     bool chainLocal);

typedef void(__cdecl *CBReleaseVariable)(struct CBVar *variable);

typedef CBSeq *(__cdecl *CBGetStack)(struct CBContext *context);

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
  typedef void(__cdecl * _array_##Free)(_array_ *);                            \
  typedef void(__cdecl * _array_##Push)(_array_ *, const _value_ *);           \
  typedef void(__cdecl * _array_##Insert)(_array_ *, uint64_t,                 \
                                          const _value_ *);                    \
  typedef _value_(__cdecl *_array_##Pop)(_array_ *);                           \
  typedef void(__cdecl * _array_##Resize)(_array_ *, uint64_t);                \
  typedef void(__cdecl * _array_##FastDelete)(_array_ *, uint64_t);            \
  typedef void(__cdecl * _array_##SlowDelete)(_array_ *, uint64_t)

CB_ARRAY_TYPE(CBSeq, struct CBVar);
CB_ARRAY_TYPE(CBTypesInfo, struct CBTypeInfo);
CB_ARRAY_TYPE(CBParametersInfo, struct CBParameterInfo);
CB_ARRAY_TYPE(CBlocks, CBlockPtr);
CB_ARRAY_TYPE(CBExposedTypesInfo, struct CBExposedTypeInfo);
CB_ARRAY_TYPE(CBStrings, CBString);

#define CB_ARRAY_PROCS(_array_, _short_)                                       \
  _array_##Free _short_##Free;                                                 \
  _array_##Push _short_##Push;                                                 \
  _array_##Insert _short_##Insert;                                             \
  _array_##Pop _short_##Pop;                                                   \
  _array_##Resize _short_##Resize;                                             \
  _array_##FastDelete _short_##FastDelete;                                     \
  _array_##SlowDelete _short_##SlowDelete

typedef struct CBTable(__cdecl *CBTableNew)();

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

  // To be used within blocks, to manipulate variables
  CBReferenceVariable referenceVariable;
  CBReleaseVariable releaseVariable;

  // Context stack access
  CBGetStack getStack;

  // Can be used to propagate block errors
  CBThrowException throwException;
  // To be used within blocks, to suspend the coroutine
  CBSuspend suspend;

  // Utility to deal with CBVars
  CBCloneVar cloneVar;
  CBDestroyVar destroyVar;

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

  // Utility to deal with CBStrings
  CB_ARRAY_PROCS(CBStrings, strings);

  // CBTable interface
  CBTableNew tableNew;

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

#define CB_CAT_IMPL(s1, s2) s1##s2
#define CB_CAT(s1, s2) CAT_IMPL(s1, s2)

#ifdef __COUNTER__
#define CB_GENSYM(str) CB_CAT(str, __COUNTER__)
#else
#define CB_GENSYM(str) CB_CAT(str, __LINE__)
#endif

#endif
