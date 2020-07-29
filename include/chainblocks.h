/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#ifndef CB_CHAINBLOCKS_H
#define CB_CHAINBLOCKS_H

#include <stdbool.h> // bool
#include <stddef.h>  // size_t
#include <stdint.h>  // ints

// All the available types
#if defined(__cplusplus) || defined(CB_USE_ENUMS)
enum CBType : uint8_t {
  None,
  Any,
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
  Block,  // a block, useful for future introspection blocks!

  EndOfBlittableTypes = 50, // anything below this is not blittable (ish)

  Bytes, // pointer + size
  String,
  Path,       // An OS filesystem path
  ContextVar, // A string label to find from CBContext variables
  Image,
  Seq,
  Table,
  Chain,
  Object,
  Array // Notice: of just bilttable types!
};

enum CBChainState : uint8_t {
  Continue, // Nothing happened, continue
  Return,   // Control flow, end this chain/flow and return previous output
  Rebase,   // Continue but put the local chain initial input as next input
  Restart,  // Restart the current chain from the top (non inline chains)
  Stop      // Stop the flow execution
};

// These blocks run fully inline in the runchain threaded execution engine
enum CBInlineBlocks : uint32_t {
  NotInline,

  // special flag that will optimize and skip activate calls
  NoopBlock,

  CoreConst,
  CoreSleep,
  CoreInput,
  CoreRepeat,
  CoreOnce,
  CoreGet,
  CoreSet,
  CoreRef,
  CoreUpdate,
  CoreSwap,
  CoreTakeSeq,
  CoreTakeInts,
  CoreTakeFloats,
  CoreTakeColor,
  CoreTakeBytes,
  CoreTakeString,
  CoreTakeTable,
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
#else
typedef uint8_t CBType;
typedef uint8_t CBChainState;
typedef uint32_t CBInlineBlocks;
#endif

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

struct CBVarPayload;
CB_ARRAY_DECL(CBPayloadArray, struct CBVarPayload);

struct CBVar;
CB_ARRAY_DECL(CBSeq, struct CBVar);

typedef void *CBTableIterator;
struct CBTableInterface;
struct CBTable {
  void *opaque;
  struct CBTableInterface *api;
};

struct CBChain;
struct CBChainRefOpaque;
typedef struct CBChainRefOpaque *CBChainRef;

struct CBChainProvider;

struct CBContext;

struct CBNode;
struct CBNodeRefOpaque;
typedef struct CBNodeRefOpaque *CBNodeRef;

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

#ifndef __EMSCRIPTEN_DISABLED__
typedef int64_t CBInt2 __attribute__((vector_size(16)));
#else
// Emscripten and WASM
// Do not support vector Int64x2
// We need to emulate it
typedef struct CBInt2 {
  int64_t v[2];

#ifdef __cplusplus
  int64_t &operator[](int index) { return v[index]; }
  int64_t operator[](int index) const { return v[index]; }

#define CBINT2_BOOLOP(__op__, __op2__)                                         \
  CBInt2 __op__(const CBInt2 &other) const {                                   \
    CBInt2 res;                                                                \
    res[0] = v[0] __op2__ other.v[0] ? -1 : 0;                                 \
    res[0] = v[1] __op2__ other.v[1] ? -1 : 0;                                 \
    return res;                                                                \
  }

  CBINT2_BOOLOP(operator==, ==)
  CBINT2_BOOLOP(operator<=, <=)
  CBINT2_BOOLOP(operator<, <)

#define CBINT2_MATHOP(__op__, __op2__)                                         \
  CBInt2 __op__(const CBInt2 &other) const {                                   \
    CBInt2 res;                                                                \
    res[0] = v[0] __op2__ other.v[0];                                          \
    res[0] = v[1] __op2__ other.v[1];                                          \
    return res;                                                                \
  }

  CBINT2_MATHOP(operator+, +)
  CBINT2_MATHOP(operator-, -)
  CBINT2_MATHOP(operator*, *)
  CBINT2_MATHOP(operator/, /)
  CBINT2_MATHOP(operator%, %)
  CBINT2_MATHOP(operator^, ^)
  CBINT2_MATHOP(operator&, &)
  CBINT2_MATHOP(operator|, |)
  CBINT2_MATHOP(operator<<, <<)
  CBINT2_MATHOP(operator>>, >>)

#endif
} CBInt2;
#endif
typedef int32_t CBInt3 __attribute__((vector_size(16)));
typedef int32_t CBInt4 __attribute__((vector_size(16)));
typedef int16_t CBInt8 __attribute__((vector_size(16)));
typedef int8_t CBInt16 __attribute__((vector_size(16)));

typedef double CBFloat2 __attribute__((vector_size(16)));
typedef float CBFloat3 __attribute__((vector_size(16)));
typedef float CBFloat4 __attribute__((vector_size(16)));

#if defined(NDEBUG) && !defined(NO_FORCE_INLINE)
#define ALWAYS_INLINE __attribute__((always_inline))
#define NO_INLINE __attribute__((noinline))
#define FLATTEN __attribute__((flatten))
#else
#define ALWAYS_INLINE
#define NO_INLINE
#define FLATTEN
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
#define FLATTEN

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

#define CBIMAGE_FLAGS_NONE (0)
#define CBIMAGE_FLAGS_BGRA (1 << 0)
#define CBIMAGE_FLAGS_PREMULTIPLIED_ALPHA (1 << 1)
#define CBIMAGE_FLAGS_16BITS_INT (1 << 2)
#define CBIMAGE_FLAGS_32BITS_FLOAT (1 << 3)

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
typedef CBBool(__cdecl *CBTableForEachCallback)(const char *key,
                                                struct CBVar *value,
                                                void *userData);
// table interface
typedef void(__cdecl *CBTableForEach)(struct CBTable table,
                                      CBTableForEachCallback cb,
                                      void *userData);
typedef size_t(__cdecl *CBTableSize)(struct CBTable table);
typedef CBBool(__cdecl *CBTableContains)(struct CBTable table, const char *key);
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

struct CBAllocatorInterface {};

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
#if defined(__cplusplus) || defined(CB_USE_ENUMS)
  enum CBType basicType;
#else
  CBType basicType;
#endif

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
      CBBool isFile;
      // expects an existing path
      CBBool existing;
      // expects a relative path (relative to the CBCore.getRootPath)
      CBBool relative;
    }
    path;

    // Not meant for chain runtime checks
    // rather validation/compose checks of params and mutations
    CB_STRUCT(Integers) {
      int64_t min;
      int64_t max;
      CBBool valid;
    }
    integers;

    // Not meant for chain runtime checks
    // rather validation/compose checks of params and mutations
    CB_STRUCT(Real) {
      double min;
      double max;
      CBBool valid;
    }
    real;
  }
  CB_UNION_NAME(details);
};

// if outData is NULL will just give you a valid outLen
// still must check result is true!
typedef CBBool(__cdecl *CBObjectSerializer)(CBPointer, uint8_t **outData,
                                            size_t *outLen,
                                            CBPointer *customHandle);
typedef void(__cdecl *CBObjectSerializerFree)(CBPointer customHandle);
typedef CBPointer(__cdecl *CBObjectDeserializer)(uint8_t *data, size_t len);
typedef void(__cdecl *CBObjectReference)(CBPointer);
typedef void(__cdecl *CBObjectRelease)(CBPointer);

struct CBObjectInfo {
  const char *name;

  CBObjectSerializer serialize;
  CBObjectSerializerFree free;
  CBObjectDeserializer deserialize;

  CBObjectReference reference;
  CBObjectRelease release;
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

  // the following are actually used only when exposing.

  // generally those are from internal blocks like Set
  // means they can change in-place
  CBBool isMutable;

  // a protected variable is ignored by Get/Set etc.
  // can only be used directly as reference from params
  CBBool isProtected;

  // if isTableEntry is true:
  // `name` will be the name of the table variable
  // `exposedType` will be of `Table` type
  // and `tableKeys` will contain the record's key name
  // while `tableTypes` the record's type
  CBBool isTableEntry;

  // If the exposed variable should be available to all chains in the node
  CBBool global;

  // private/internal, leave nullptr
  struct CBChain *scope;
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
//   * The activating block will have to manage any CBVar that puts in context
// #### In the case of setParam:
//   * If the block needs to store the String or Seq data it will then need to
// deep copy it.
//   * Callers should free up any allocated memory.

// ### What about exposed/requiredVariables, parameters and input/outputTypes:
// * Same for them, they are just read only basically

struct CBVarPayload {
  union {
    CBBool boolValue;

    struct {
      CBPointer objectValue;
      int32_t objectVendorId;
      int32_t objectTypeId;
    };

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
      // this field is an optional optimization
      // if 0 strlen should be called to find the string length
      // if not 0 should be assumed valid
      uint32_t stringLen;
      // this is mostly used internal
      // useful when serializing, recycling memory
      uint32_t stringCapacity;
    };

    struct CBColor colorValue;

    struct CBImage imageValue;

    // TODO
    struct CBAudio audioValue;

    CBChainRef chainValue;

    CBlockPtr blockValue;

    struct {
      CBEnum enumValue;
      int32_t enumVendorId;
      int32_t enumTypeId;
    };

    struct {
      uint8_t *bytesValue;
      uint32_t bytesSize;
      // this is mostly used internal
      // useful when serializing, recycling memory
      uint32_t bytesCapacity;
    };

    CBPayloadArray arrayValue;
  };
} __attribute__((aligned(16)));

// CBVar flags
#define CBVAR_FLAGS_NONE (0)
#define CBVAR_FLAGS_USES_ALLOCATOR (1 << 0)
#define CBVAR_FLAGS_USES_OBJINFO (1 << 1)
#define CBVAR_FLAGS_REF_COUNTED (1 << 2)

struct CBVar {
  struct CBVarPayload payload;
  union {
    // currently used when dealing with Object type variables
    // if you need (serialization & destroy) you should populate this field
    // when CBVAR_FLAGS_USES_OBJINFO
    struct CBObjectInfo *objectInfo;
    // when CBVAR_FLAGS_USES_ALLOCATOR, used for IPC and such
    struct CBAllocatorInterface *allocator;
  };
  uint32_t refcount;
#if defined(__cplusplus) || defined(CB_USE_ENUMS)
  enum CBType valueType;
  enum CBType innerType;
#else
  CBType valueType;
  CBType innerType;
#endif
  uint8_t flags;
} __attribute__((aligned(16)));

enum CBRunChainOutputState { Running, Restarted, Stopped, Failed };

struct CBRunChainOutput {
  struct CBVar output;
  enum CBRunChainOutputState state;
} __attribute__((aligned(16)));

struct CBComposeResult {
  struct CBTypeInfo outputType;

  // Allow external blocks to fail
  // Internally we use exceptions
  CBBool failed;
  struct CBVar failureMessage; // destroyVar after use if failed

  CBExposedTypesInfo exposedInfo;
  CBExposedTypesInfo requiredInfo;

  // used when the last block of the flow is
  // Restart/Stop/Return etc
  bool flowStopper;
};

typedef void(__cdecl *CBComposeError)(void *privateContext,
                                      const char *errorText,
                                      CBBool warningOnly);

struct CBInstanceData {
  // Used to optimize activations,
  // replacing function pointers during compose
  struct CBlock *block;

  // The current chain we are composing
  struct CBChain *chain;

  // Info related to our activation
  struct CBTypeInfo inputType;
  CBExposedTypesInfo shared;

  // basically what the next block can get as input
  struct CBTypesInfo outputTypes;

  // Use this to report compose errors (or warnings)
  CBComposeError reportError;
  // Reserved to use and pass with reportError
  void *privateContext;
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

typedef void(__cdecl *CBComposedProc)(struct CBlock *,
                                      const struct CBChain *chain,
                                      const struct CBComposeResult *data);

// The core of the block processing, avoid syscalls here
typedef struct CBVar(__cdecl *CBActivateProc)(struct CBlock *,
                                              struct CBContext *,
                                              const struct CBVar *);

// Generally when stop() is called
typedef void(__cdecl *CBCleanupProc)(struct CBlock *);

typedef void(__cdecl *CBWarmupProc)(struct CBlock *, struct CBContext *);

// Genetic programming optional mutation procedure
typedef void(__cdecl *CBMutateProc)(struct CBlock *, struct CBTable options);
// Genetic programming optional crossover (inplace/3way) procedure
typedef void(__cdecl *CBCrossoverProc)(struct CBlock *, struct CBVar state0,
                                       struct CBVar state1);

// Used for serialization, to deep serialize internal block state
typedef struct CBVar(__cdecl *CBGetStateProc)(struct CBlock *);
typedef void(__cdecl *CBSetStateProc)(struct CBlock *, struct CBVar state);
typedef void(__cdecl *CBResetStateProc)(struct CBlock *);

struct CBlock {
  // \-- Internal stuff, do not directly use! --/

#if defined(__cplusplus) || defined(CB_USE_ENUMS)
  enum CBInlineBlocks inlineBlockId;
#else
  CBInlineBlocks inlineBlockId;
#endif

  // flag to ensure blocks are unique when flows/chains
  CBBool owned;

  // \-- The interface to fill --/

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
  // Optional call called when the whole chain hosting this block has been
  // composed, this is called only on the top level, blocks that host other
  // blocks should propagate it properly
  CBComposedProc composed;

  CBParametersProc parameters;
  CBSetParamProc setParam; // Set a parameter, the block will copy the value, so
                           // if you allocated any memory you should free it
  CBGetParamProc getParam; // Gets a parameter, the block is the owner of any
                           // allocated stuff, DO NOT free them

  CBWarmupProc warmup; // Called before running the chain, once
  CBActivateProc activate;
  // Called every time you stop a coroutine or sometimes
  // internally to clean up the block
  CBCleanupProc cleanup;

  // Optional genetic programming helpers
  // getState/setState are also used during serialization
  // assume all the following to be called out of chain
  // likely the chain will be stopped after cleanup() called
  // so any state (in fact) should be kept
  CBMutateProc mutate;
  CBCrossoverProc crossover;
  // intended as persistent state during block lifetime
  // persisting cleanups
  CBGetStateProc getState;
  CBSetStateProc setState;
  CBResetStateProc resetState;
};

struct CBChainProviderUpdate {
  const char *error;     // if any or nullptr
  struct CBChain *chain; // or nullptr if error
};

typedef void(__cdecl *CBProviderReset)(struct CBChainProvider *provider);

typedef CBBool(__cdecl *CBProviderReady)(struct CBChainProvider *provider);
typedef void(__cdecl *CBProviderSetup)(struct CBChainProvider *provider,
                                       const char *path,
                                       struct CBInstanceData data);

typedef CBBool(__cdecl *CBProviderUpdated)(struct CBChainProvider *provider);
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
                                            CBBool nonfatalWarning,
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
                                                     const char *name);
typedef struct CBVar *(__cdecl *CBReferenceChainVariable)(CBChainRef chain,
                                                          const char *name);

typedef void(__cdecl *CBReleaseVariable)(struct CBVar *variable);

typedef void(__cdecl *CBAbortChain)(struct CBContext *context,
                                    const char *errorText);

#if defined(__cplusplus) || defined(CB_USE_ENUMS)
typedef enum CBChainState(__cdecl *CBSuspend)(struct CBContext *context,
                                              double seconds);
#else
typedef CBChainState(__cdecl *CBSuspend)(struct CBContext *context,
                                         double seconds);
#endif

typedef void(__cdecl *CBCloneVar)(struct CBVar *dst, const struct CBVar *src);

typedef void(__cdecl *CBDestroyVar)(struct CBVar *var);

typedef CBBool(__cdecl *CBValidateSetParam)(struct CBlock *block, int index,
                                            struct CBVar param,
                                            CBValidationCallback callback,
                                            void *userData);

typedef struct CBComposeResult(__cdecl *CBComposeBlocks)(
    CBlocks blocks, CBValidationCallback callback, void *userData,
    struct CBInstanceData data);

#if defined(__cplusplus) || defined(CB_USE_ENUMS)
typedef enum CBChainState(__cdecl *CBRunBlocks)(CBlocks blocks,
                                                struct CBContext *context,
                                                struct CBVar input,
                                                struct CBVar *output,
                                                const CBBool handleReturn);
#else
typedef CBChainState(__cdecl *CBRunBlocks)(CBlocks blocks,
                                           struct CBContext *context,
                                           struct CBVar input,
                                           struct CBVar *output,
                                           const CBBool handleReturn);
#endif

typedef void(__cdecl *CBLog)(const char *msg);

typedef struct CBlock *(__cdecl *CBCreateBlock)(const char *name);

typedef CBChainRef(__cdecl *CBCreateChain)();
typedef void(__cdecl *CBSetChainName)(CBChainRef chain, const char *name);
typedef void(__cdecl *CBSetChainLooped)(CBChainRef chain, CBBool looped);
typedef void(__cdecl *CBSetChainUnsafe)(CBChainRef chain, CBBool unsafe);
typedef void(__cdecl *CBAddBlock)(CBChainRef chain, CBlockPtr block);
typedef void(__cdecl *CBRemBlock)(CBChainRef chain, CBlockPtr block);
typedef void(__cdecl *CBDestroyChain)(CBChainRef chain);
typedef struct CBVar(__cdecl *CBStopChain)(CBChainRef chain);
typedef struct CBComposeResult(__cdecl *CBComposeChain)(
    CBChainRef chain, CBValidationCallback callback, void *userData,
    struct CBInstanceData data);
typedef struct CBRunChainOutput(__cdecl *CBRunChain)(CBChainRef chain,
                                                     struct CBContext *context,
                                                     struct CBVar input);

typedef CBNodeRef(__cdecl *CBCreateNode)();
typedef void(__cdecl *CBDestroyNode)(CBNodeRef node);
typedef void(__cdecl *CBSchedule)(CBNodeRef node, CBChainRef chain);
typedef CBBool(__cdecl *CBTick)(CBNodeRef node);
typedef void(__cdecl *CBSleep)(double seconds, CBBool runCallbacks);

#define CB_ARRAY_TYPE(_array_, _value_)                                        \
  typedef void(__cdecl * _array_##Free)(_array_ *);                            \
  typedef void(__cdecl * _array_##Push)(_array_ *, const _value_ *);           \
  typedef void(__cdecl * _array_##Insert)(_array_ *, uint32_t,                 \
                                          const _value_ *);                    \
  typedef _value_(__cdecl *_array_##Pop)(_array_ *);                           \
  typedef void(__cdecl * _array_##Resize)(_array_ *, uint32_t);                \
  typedef void(__cdecl * _array_##FastDelete)(_array_ *, uint32_t);            \
  typedef void(__cdecl * _array_##SlowDelete)(_array_ *, uint32_t)

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

typedef struct CBVar(__cdecl *CBAsyncActivateProc)(struct CBContext *context,
                                                   void *userData);

typedef struct CBVar(__cdecl *CBRunAsyncActivate)(struct CBContext *context,
                                                  void *userData,
                                                  CBAsyncActivateProc call);

struct CBChainInfo {
  const char *name;
  CBBool looped;
  CBBool unsafe;
  const struct CBChain *chain;
  const CBlocks blocks;
  CBBool isRunning;
};

struct CBLoggingOptions {
  uint32_t maxSize;
};

typedef struct CBChainInfo(__cdecl *CBGetChainInfo)(CBChainRef chainref);

typedef void(__cdecl *CBSetLoggingOptions)(struct CBLoggingOptions options);

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
  CBReferenceChainVariable referenceChainVariable;
  CBReleaseVariable releaseVariable;

  // To be used within blocks, to suspend the coroutine
  CBSuspend suspend;
  // To be used within blocks, to abort the chain
  // after this call you should return immediately from activate
  CBAbortChain abortChain;

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
  CBComposeBlocks composeBlocks;
  CBRunBlocks runBlocks;

  // Logging
  CBLog log;
  CBSetLoggingOptions setLoggingOptions;

  // Chain creation
  CBCreateBlock createBlock;
  CBValidateSetParam validateSetParam;

  CBCreateChain createChain;
  CBSetChainName setChainName;
  CBSetChainLooped setChainLooped;
  CBSetChainUnsafe setChainUnsafe;
  CBAddBlock addBlock;
  CBRemBlock removeBlock;
  CBDestroyChain destroyChain;
  CBStopChain stopChain; // must destroyVar once done
  CBComposeChain composeChain;
  CBRunChain runChain;
  CBGetChainInfo getChainInfo;

  // Chain scheduling
  CBCreateNode createNode;
  CBDestroyNode destroyNode;
  CBSchedule schedule;
  CBSchedule unschedule;
  CBTick tick; // returns false if all chains are done!
  CBSleep sleep;

  // Environment utilities
  CBGetRootPath getRootPath;
  CBSetRootPath setRootPath;

  // async execution
  CBRunAsyncActivate asyncActivate;
};

typedef CBBool(__cdecl *CBChainblocksInterface)(uint32_t abi_version,
                                                struct CBCore *core);

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

#if defined(__cplusplus)
extern "C" {
#endif
EXPORTED CBBool __cdecl chainblocksInterface(uint32_t abi_version,
                                             struct CBCore *core);
#if defined(__cplusplus)
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
