/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#ifndef SH_SHARDS_H
#define SH_SHARDS_H

#include <stdbool.h> // bool
#include <stddef.h>  // size_t
#include <stdint.h>  // ints

// All the available types
#if defined(__cplusplus) || defined(SH_USE_ENUMS)
enum SHType : uint8_t {
  // Blittables
  None,
  Any,
  Enum,
  Bool,
  Int,      // A 64bits int
  Int2,     // A vector of 2 64bits ints
  Int3,     // A vector of 3 32bits ints
  Int4,     // A vector of 4 32bits ints
  Int8,     // A vector of 8 16bits ints
  Int16,    // A vector of 16 8bits ints
  Float,    // A 64bits float
  Float2,   // A vector of 2 64bits floats
  Float3,   // A vector of 3 32bits floats
  Float4,   // A vector of 4 32bits floats
  Color,    // A vector of 4 uint8
  ShardRef, // a shard, useful for future introspection shards!

  // Internal use only
  EndOfBlittableTypes = 50, // anything below this is not blittable (ish)

  // Non Blittables
  Bytes, // pointer + size
  String,
  Path,       // An OS filesystem path
  ContextVar, // A string label to find from SHContext variables
  Image,
  Seq,
  Table,
  Wire,
  Object,
  Array, // Notice: of just blittable types/WIP!
  Set,
  Audio,
};

enum SHWireState : uint8_t {
  Continue, // Nothing happened, continue
  Return,   // Control flow, end this wire/flow and return previous output
  Rebase,   // Continue but put the local wire initial input as next input
  Restart,  // Restart the current wire from the top (non inline wires)
  Stop,     // Stop the flow execution
  Error,    // Stop the flow execution and raise an error
};

// These shards run fully inline in the wire threaded execution engine
enum SHInlineShards : uint32_t {
  // regular shards
  NotInline,
  // special flag that will optimize and skip activate calls
  NoopShard,
  // internal "quick" inlined shards
  CoreConst,
  CoreSleep,
  CoreInput,
  CoreForRange,
  CoreRepeat,
  CoreOnce,
  CoreGet,
  CoreSet,
  CoreRefRegular,
  CoreRefTable,
  CoreUpdate,
  CoreSwap,
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
  MathFastSqrt,
  MathFastInvSqrt,
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
typedef uint8_t SHType;
typedef uint8_t SHWireState;
typedef uint32_t SHInlineShards;
#endif

typedef void *SHArray;

// This (32bit sizes) is for obvious packing reason, sizeof(SHVar) == 32
// But also for compatibility, shards supports 32bit systems
// So the max array size should be INT32_SIZE indeed
// Also most of array index operators in c++ use int
#define SH_ARRAY_DECL(_seq_, _val_) \
  typedef struct _seq_ {            \
    _val_ *elements;                \
    uint32_t len;                   \
    uint32_t cap;                   \
  } _seq_

// Forward declarations

struct SHVarPayload;
SH_ARRAY_DECL(SHPayloadArray, struct SHVarPayload);

struct SHVar;
SH_ARRAY_DECL(SHSeq, struct SHVar);

// 64 bytes should be huge and well enough space for an iterator...
typedef char SHTableIterator[64];
struct SHTableInterface;
struct SHTable {
  void *opaque;
  struct SHTableInterface *api;
};

// 64 bytes should be huge and well enough space for an iterator...
typedef char SHSetIterator[64];
struct SHSetInterface;
struct SHSet {
  void *opaque;
  struct SHSetInterface *api;
};

struct SHWire;
struct SHWireRefOpaque;
typedef struct SHWireRefOpaque *SHWireRef;

struct SHWireProvider;

struct SHContext;

struct SHMesh;
struct SHMeshRefOpaque;
typedef struct SHMeshRefOpaque *SHMeshRef;

struct SHFlow;

struct Shard;
typedef struct Shard *ShardPtr;
SH_ARRAY_DECL(Shards, ShardPtr);

struct SHTypeInfo;
SH_ARRAY_DECL(SHTypesInfo, struct SHTypeInfo);

struct SHParameterInfo;
SH_ARRAY_DECL(SHParametersInfo, struct SHParameterInfo);

struct SHExposedTypeInfo;
SH_ARRAY_DECL(SHExposedTypesInfo, struct SHExposedTypeInfo);

typedef void *SHPointer;
typedef int64_t SHInt;
typedef double SHFloat;
typedef bool SHBool;
typedef int32_t SHEnum;
SH_ARRAY_DECL(SHEnums, SHEnum);

typedef const char *SHString;
SH_ARRAY_DECL(SHStrings, SHString);
// used in order to compress/omit/localize the strings at runtime
// specially for help functions
// if string is null, crc is checked and used to retrieve a string
typedef struct _SHOptionalString {
  SHString string;
  uint32_t crc;
} SHOptionalString;

#if defined(__clang__) || defined(__GNUC__)
#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#ifdef __clang__
#define shufflevector __builtin_shufflevector
#else
#define shufflevector __builtin_shuffle
#endif

typedef int64_t SHInt2 __attribute__((vector_size(16)));
typedef int32_t SHInt3 __attribute__((vector_size(16)));
typedef int32_t SHInt4 __attribute__((vector_size(16)));
typedef int16_t SHInt8 __attribute__((vector_size(16)));
typedef int8_t SHInt16 __attribute__((vector_size(16)));

typedef double SHFloat2 __attribute__((vector_size(16)));
typedef float SHFloat3 __attribute__((vector_size(16)));
typedef float SHFloat4 __attribute__((vector_size(16)));

#define NO_INLINE __attribute__((noinline))

#if defined(NDEBUG) && !defined(NO_FORCE_INLINE)
#define ALWAYS_INLINE __attribute__((always_inline))
#define FLATTEN __attribute__((flatten))
#else
#define ALWAYS_INLINE
#define FLATTEN
#endif

#else // TODO
#error "Unsupported compiler"

typedef int64_t SHInt2[2];
typedef int32_t SHInt3[3];
typedef int32_t SHInt4[4];
typedef int16_t SHInt8[8];
typedef int8_t SHInt16[16];

typedef double SHFloat2[2];
typedef float SHFloat3[3];
typedef float SHFloat4[4];

#define ALWAYS_INLINE
#define NO_INLINE
#define FLATTEN

#endif

#ifndef _WIN32
#if defined(__i386__)
#define __cdecl __attribute__((__cdecl__))
#else
#define __cdecl
#endif
#endif

struct SHColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
};

#define SHIMAGE_FLAGS_NONE (0)
#define SHIMAGE_FLAGS_BGRA (1 << 0)
#define SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA (1 << 1)
#define SHIMAGE_FLAGS_16BITS_INT (1 << 2)
#define SHIMAGE_FLAGS_32BITS_FLOAT (1 << 3)

struct SHImage {
  uint16_t width;
  uint16_t height;
  uint8_t channels;
  uint8_t flags;
  uint8_t *data;
};

struct SHAudio {
  uint32_t sampleRate; // set to 0 if unknown/not relevant
  uint16_t nsamples;
  uint16_t channels;
  float *samples;
};

#define SH_FLOW_CONTINUE (0)
#define SH_FLOW_ERROR (1 << 0)
#define SH_FLOW_CHANGE_STATE (1 << 1)

struct SHError {
  uint8_t code; // 0 if no error, 1 if error so far
  SHString message;

#ifdef __cplusplus
  static const SHError Success;
#endif
};

#define SH_ERROR_NONE (0)

#ifdef __cplusplus
constexpr const SHError SHError::Success = {SH_ERROR_NONE, nullptr};
#endif

// table interface
typedef void(__cdecl *SHTableGetIterator)(struct SHTable table, SHTableIterator *outIter);
typedef SHBool(__cdecl *SHTableNext)(struct SHTable table, SHTableIterator *inIter, SHString *outKey, struct SHVar *outValue);
typedef size_t(__cdecl *SHTableSize)(struct SHTable table);
typedef SHBool(__cdecl *SHTableContains)(struct SHTable table, SHString key);
typedef struct SHVar *(__cdecl *SHTableAt)(struct SHTable table, SHString key);
typedef void(__cdecl *SHTableRemove)(struct SHTable table, SHString key);
typedef void(__cdecl *SHTableClear)(struct SHTable table);
typedef void(__cdecl *SHTableFree)(struct SHTable table);

struct SHTableInterface {
  SHTableGetIterator tableGetIterator;
  SHTableNext tableNext;
  SHTableSize tableSize;
  SHTableContains tableContains;
  SHTableAt tableAt;
  SHTableRemove tableRemove;
  SHTableClear tableClear;
  SHTableFree tableFree;
};

// set interface
typedef void(__cdecl *SHSetGetIterator)(struct SHSet set, SHSetIterator *outIter);
typedef SHBool(__cdecl *SHSetNext)(struct SHSet set, SHSetIterator *inIter, struct SHVar *outValue);
typedef size_t(__cdecl *SHSetSize)(struct SHSet table);
typedef SHBool(__cdecl *SHSetContains)(struct SHSet table, struct SHVar value);
typedef SHBool(__cdecl *SHSetInclude)(struct SHSet table, struct SHVar value);
typedef SHBool(__cdecl *SHSetExclude)(struct SHSet table, struct SHVar value);
typedef void(__cdecl *SHSetClear)(struct SHSet table);
typedef void(__cdecl *SHSetFree)(struct SHSet table);

struct SHSetInterface {
  SHSetGetIterator setGetIterator;
  SHSetNext setNext;
  SHSetSize setSize;
  SHSetContains setContains;
  SHSetInclude setInclude;
  SHSetExclude setExclude;
  SHSetClear setClear;
  SHSetFree setFree;
};

#ifdef SH_NO_ANON
#define SH_STRUCT(_name_) struct _name_
#define SH_UNION(_name_) union _name_
#define SH_UNION_NAME(_name_) _name_
#else
#define SH_STRUCT(_name_) struct
#define SH_UNION(_name_) union
#define SH_UNION_NAME(_name_)
#endif

struct SHTypeInfo {
#if defined(__cplusplus) || defined(SH_USE_ENUMS)
  enum SHType basicType;
#else
  SHType basicType;
#endif

  SH_UNION(Details) {
    SH_STRUCT(Object) {
      int32_t vendorId;
      int32_t typeId;
    }
    object;

    SH_STRUCT(Enum) {
      int32_t vendorId;
      int32_t typeId;
    }
    enumeration;

    SHTypesInfo seqTypes;
    SHTypesInfo setTypes;

    SH_STRUCT(Table) {
      // If tableKeys is populated, it is expected that
      // tableTypes will be populated as well and that at the same
      // key index there is the key's type
      SHStrings keys;
      // If tableKeys is not populated, len == 0 and tableKeys is populated len
      // > 0 it is assumed that tableTypes contains a sequence with the possible
      // types in the table
      SHTypesInfo types;
    }
    table;

    SHTypesInfo contextVarTypes;

    SH_STRUCT(Path) {
      // if is File, the extension allowed
      SHStrings extensions;
      // expects a path to a file
      SHBool isFile;
      // expects an existing path
      SHBool existing;
      // expects a relative path (relative to the SHCore.getRootPath)
      SHBool relative;
    }
    path;

    // Not meant for wire runtime checks
    // rather validation/compose checks of params and mutations
    SH_STRUCT(Integers) {
      int64_t min;
      int64_t max;
      SHBool valid;
    }
    integers;

    // Not meant for wire runtime checks
    // rather validation/compose checks of params and mutations
    SH_STRUCT(Real) {
      double min;
      double max;
      SHBool valid;
    }
    real;
  }
  SH_UNION_NAME(details);

  // used for Seq and Array only for now, to allow optimizations if the size is
  // known at compose time.
  // Should not be considered when hashing this type
  uint32_t fixedSize;
  // Used by Array type, which is still not implemented properly and unstable.
  enum SHType innerType;
  // used internally to make our live easy when types are recursive (aka Self is
  // inside the seqTypes or so)
  // Should not be considered when hashing this type
  SHBool recursiveSelf;
};

struct SHShardComposeResult {
  struct SHError error;
  struct SHTypeInfo result;
};

// if outData is NULL will just give you a valid outLen
// still must check result is true!
typedef SHBool(__cdecl *SHObjectSerializer)(SHPointer, uint8_t **outData, size_t *outLen, SHPointer *customHandle);
typedef void(__cdecl *SHObjectSerializerFree)(SHPointer customHandle);
typedef SHPointer(__cdecl *SHObjectDeserializer)(uint8_t *data, size_t len);
typedef void(__cdecl *SHObjectReference)(SHPointer);
typedef void(__cdecl *SHObjectRelease)(SHPointer);
typedef uint64_t(__cdecl *SHObjectHash)(SHPointer);

struct SHObjectInfo {
  SHString name;

  SHObjectSerializer serialize;
  SHObjectSerializerFree free;
  SHObjectDeserializer deserialize;

  SHObjectReference reference;
  SHObjectRelease release;

  SHObjectHash hash;
};

struct SHEnumInfo {
  SHString name;
  SHStrings labels;
  SHEnums values;
};

struct SHParameterInfo {
  SHString name;
  SHOptionalString help;
  SHTypesInfo valueTypes;
};

struct SHExposedTypeInfo {
  SHString name;
  SHOptionalString help;
  struct SHTypeInfo exposedType;

  // the following are actually used only when exposing.

  // generally those are from internal shards like Set
  // means they can change in-place
  SHBool isMutable;

  // a protected variable is ignored by Get/Set etc.
  // can only be used directly as reference from params
  SHBool isProtected;

  // if isTableEntry is true:
  // `name` will be the name of the table variable
  // `exposedType` will be of `Table` type
  // and `tableKeys` will contain the record's key name
  // while `tableTypes` the record's type
  SHBool isTableEntry;

  // If the exposed variable should be available to all wires in the mesh
  SHBool global;
};

struct SHFlow {
  struct SHWire *wire;
};

// # Of SHVars and memory

// ## Specifically String and Seq types

// ### Shards need to maintain their own garbage, in a way so that any reciver
// of SHVar/s will not have to worry about it.

// #### In the case of getParam:
//   * the callee should allocate and preferably cache any String or Seq that
// needs to return.
//   * the callers will just read and should not modify the contents.
// #### In the case of activate:
//   * The input var memory is owned by the previous shard.
//   * The output var memory is owned by the activating shard.
//   * The activating shard will have to manage any SHVar that puts in context
// #### In the case of setParam:
//   * If the shard needs to store the String or Seq data it will then need to
// deep copy it.
//   * Callers should free up any allocated memory.

// ### What about exposed/requiredVariables, parameters and input/outputTypes:
// * Same for them, they are just read only basically

struct SHVarPayload {
  union {
    SHBool boolValue;

    struct {
      SHPointer objectValue;
      int32_t objectVendorId;
      int32_t objectTypeId;
    };

    SHInt intValue;
    SHInt2 int2Value;
    SHInt3 int3Value;
    SHInt4 int4Value;
    SHInt8 int8Value;
    SHInt16 int16Value;

    SHFloat floatValue;
    SHFloat2 float2Value;
    SHFloat3 float3Value;
    SHFloat4 float4Value;

    // notice, this is assumed not mutable!
    // unless specified in the SHTypeInfo of this value
    // and only when used as variable (shared)
    SHSeq seqValue;

    struct SHTable tableValue;

    struct SHSet setValue;

    struct {
      SHString stringValue;
      // this field is an optional optimization
      // if 0 strlen should be called to find the string length
      // if not 0 should be assumed valid
      uint32_t stringLen;
      // this is mostly used internal
      // useful when serializing, recycling memory
      uint32_t stringCapacity;
    };

    struct SHColor colorValue;

    struct SHImage imageValue;

    struct SHAudio audioValue;

    SHWireRef wireValue;

    ShardPtr shardValue;

    struct {
      SHEnum enumValue;
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

    SHPayloadArray arrayValue;
  };
} __attribute__((aligned(16)));

// SHVar flags
#define SHVAR_FLAGS_NONE (0)
#define SHVAR_FLAGS_USES_OBJINFO (1 << 0)
#define SHVAR_FLAGS_REF_COUNTED (1 << 1)
// this marks a variable external and even if references are counted
// it won't be destroyed automatically
#define SHVAR_FLAGS_EXTERNAL (1 << 2)

struct SHVar {
  struct SHVarPayload payload;
#if defined(__cplusplus) || defined(SH_USE_ENUMS)
  enum SHType valueType;
  enum SHType innerType;
#else
  SHType valueType;
  SHType innerType;
#endif
  uint16_t flags;
  uint32_t refcount;
  struct SHObjectInfo *objectInfo;
#if defined(__i386__) || defined(__EMSCRIPTEN__)
  uint32_t _cpu32bits_padding;
#endif
} __attribute__((aligned(16)));

enum SHRunWireOutputState { Running, Restarted, Stopped, Failed };

struct SHRunWireOutput {
  struct SHVar output;
  enum SHRunWireOutputState state;
} __attribute__((aligned(16)));

struct SHComposeResult {
  struct SHTypeInfo outputType;

  // Allow external shards to fail
  // Internally we use exceptions
  SHBool failed;
  struct SHVar failureMessage; // destroyVar after use if failed

  SHExposedTypesInfo exposedInfo;
  SHExposedTypesInfo requiredInfo;

  // used when the last shard of the flow is
  // Restart/Stop/Return etc
  bool flowStopper;
};

struct SHInstanceData {
  // Used to optimize activations,
  // replacing function pointers during compose
  struct Shard *shard;

  // The current wire we are composing
  struct SHWire *wire;

  // Info related to our activation
  struct SHTypeInfo inputType;
  SHExposedTypesInfo shared;
  // if this activation might happen in a worker thread
  // for example cos this shard is within an Await shard
  // useful to fail during compose if we don't wish this
  bool onWorkerThread;

  // basically what the next shard can get as input
  struct SHTypesInfo outputTypes;

  // Internally used
  void *requiredVariables;
};

typedef struct Shard *(__cdecl *SHShardConstructor)();
typedef void(__cdecl *SHCallback)();

typedef SHString(__cdecl *SHNameProc)(struct Shard *);
typedef uint32_t(__cdecl *SHHashProc)(struct Shard *);
typedef SHOptionalString(__cdecl *SHHelpProc)(struct Shard *);
typedef const struct SHTable *(__cdecl *SHPropertiesProc)(struct Shard *);

// Construction/Destruction
typedef void(__cdecl *SHSetupProc)(struct Shard *);
typedef void(__cdecl *SHDestroyProc)(struct Shard *);

typedef SHTypesInfo(__cdecl *SHInputTypesProc)(struct Shard *);
typedef SHTypesInfo(__cdecl *SHOutputTypesProc)(struct Shard *);

typedef SHExposedTypesInfo(__cdecl *SHExposedVariablesProc)(struct Shard *);
typedef SHExposedTypesInfo(__cdecl *SHRequiredVariablesProc)(struct Shard *);

typedef SHParametersInfo(__cdecl *SHParametersProc)(struct Shard *);
typedef struct SHError(__cdecl *SHSetParamProc)(struct Shard *, int, const struct SHVar *);
typedef struct SHVar(__cdecl *SHGetParamProc)(struct Shard *, int);

typedef struct SHShardComposeResult(__cdecl *SHComposeProc)(struct Shard *, struct SHInstanceData data);

typedef void(__cdecl *SHComposedProc)(struct Shard *, const struct SHWire *wire, const struct SHComposeResult *data);

// The core of the shard processing, avoid syscalls here
typedef struct SHVar(__cdecl *SHActivateProc)(struct Shard *, struct SHContext *, const struct SHVar *);

// Generally when stop() is called
typedef struct SHError(__cdecl *SHCleanupProc)(struct Shard *);

typedef struct SHError(__cdecl *SHWarmupProc)(struct Shard *, struct SHContext *);

typedef void(__cdecl *SHNextFrameProc)(struct Shard *, struct SHContext *);

// Genetic programming optional mutation procedure
typedef void(__cdecl *SHMutateProc)(struct Shard *, struct SHTable options);
// Genetic programming optional crossover (inplace/3way) procedure
typedef void(__cdecl *SHCrossoverProc)(struct Shard *, const struct SHVar *state0, const struct SHVar *state1);

// Used for serialization, to deep serialize internal shard state
typedef struct SHVar(__cdecl *SHGetStateProc)(struct Shard *);
typedef void(__cdecl *SHSetStateProc)(struct Shard *, const struct SHVar *state);
typedef void(__cdecl *SHResetStateProc)(struct Shard *);

struct Shard {
  // \-- Internal stuff, do not directly use! --/

#if defined(__cplusplus) || defined(SH_USE_ENUMS)
  enum SHInlineShards inlineShardId;
#else
  SHInlineShards inlineShardId;
#endif

  // flag to ensure shards are unique when flows/wires
  SHBool owned;

  // \-- The interface to fill --/

  SHNameProc name;             // Returns the name of the shard, do not free the string,
                               // generally const
  SHHashProc hash;             // Returns the hash of the shard, useful for serialization
  SHHelpProc help;             // Returns the help text of the shard, do not free the
                               // string, generally const
  SHHelpProc inputHelp;        // optional help text for the input
  SHHelpProc outputHelp;       // optional help text for the output
  SHPropertiesProc properties; // optional properties

  SHSetupProc setup;     // A one time construtor setup for the shard
  SHDestroyProc destroy; // A one time finalizer for the shard, shards should
                         // also free all the memory in here!

  SHInputTypesProc inputTypes;
  SHOutputTypesProc outputTypes;

  SHExposedVariablesProc exposedVariables;
  SHRequiredVariablesProc requiredVariables;

  // Optional call used during validation to fixup "Any" input
  // type and provide valid output and exposed variable types
  SHComposeProc compose;
  // Optional call called when the whole wire hosting this shard has been
  // composed, this is called only on the top level, shards that host other
  // shards should propagate it properly
  SHComposedProc composed;

  SHParametersProc parameters;
  SHSetParamProc setParam; // Set a parameter, the shard will copy the value, so
                           // if you allocated any memory you should free it
  SHGetParamProc getParam; // Gets a parameter, the shard is the owner of any
                           // allocated stuff, DO NOT free them

  SHWarmupProc warmup; // Called before running the wire, once
  SHActivateProc activate;
  // Called every time you stop a coroutine or sometimes
  // internally to clean up the shard
  SHCleanupProc cleanup;

  // this proc is optional and won't be called unless requested
  // using ... (todo)
  // it will be called always before activate, right at the beginning of the
  // next top wire coroutine iteration even if the shard is not activated
  SHNextFrameProc nextFrame;

  // Optional genetic programming helpers
  // getState/setState are also used during serialization
  // assume all the following to be called out of wire
  // likely the wire will be stopped after cleanup() called
  // so any state (in fact) should be kept
  SHMutateProc mutate;
  SHCrossoverProc crossover;
  // intended as persistent state during shard lifetime
  // persisting cleanups
  SHGetStateProc getState;
  SHSetStateProc setState;
  SHResetStateProc resetState;
};

struct SHWireProviderUpdate {
  SHString error;      // if any or nullptr
  struct SHWire *wire; // or nullptr if error
};

typedef void(__cdecl *SHProviderReset)(struct SHWireProvider *provider);

typedef SHBool(__cdecl *SHProviderReady)(struct SHWireProvider *provider);
typedef void(__cdecl *SHProviderSetup)(struct SHWireProvider *provider, SHString path, struct SHInstanceData data);

typedef SHBool(__cdecl *SHProviderUpdated)(struct SHWireProvider *provider);
typedef struct SHWireProviderUpdate(__cdecl *SHProviderAcquire)(struct SHWireProvider *provider);

typedef void(__cdecl *SHProviderReleaseWire)(struct SHWireProvider *provider, struct SHWire *wire);

struct SHWireProvider {
  SHProviderReset reset;

  SHProviderReady ready;
  SHProviderSetup setup;

  SHProviderUpdated updated;
  SHProviderAcquire acquire;

  SHProviderReleaseWire release;

  void *userData;
};

typedef void(__cdecl *SHValidationCallback)(const struct Shard *errorShard, SHString errorTxt, SHBool nonfatalWarning,
                                            void *userData);

typedef void(__cdecl *SHRegisterShard)(SHString fullName, SHShardConstructor constructor);

typedef void(__cdecl *SHRegisterObjectType)(int32_t vendorId, int32_t typeId, struct SHObjectInfo info);

typedef void(__cdecl *SHRegisterEnumType)(int32_t vendorId, int32_t typeId, struct SHEnumInfo info);

typedef void(__cdecl *SHRegisterRunLoopCallback)(SHString eventName, SHCallback callback);

typedef void(__cdecl *SHRegisterExitCallback)(SHString eventName, SHCallback callback);

typedef void(__cdecl *SHUnregisterRunLoopCallback)(SHString eventName);

typedef void(__cdecl *SHUnregisterExitCallback)(SHString eventName);

typedef struct SHVar *(__cdecl *SHReferenceVariable)(struct SHContext *context, SHString name);
typedef struct SHVar *(__cdecl *SHReferenceWireVariable)(SHWireRef wire, SHString name);

typedef void(__cdecl *SHSetExternalVariable)(SHWireRef wire, SHString name, struct SHVar *pVar);

typedef void(__cdecl *SHRemoveExternalVariable)(SHWireRef wire, SHString name);

typedef struct SHVar *(__cdecl *SHAllocExternalVariable)(SHWireRef wire, SHString name);

typedef void(__cdecl *SHFreeExternalVariable)(SHWireRef wire, SHString name);

typedef void(__cdecl *SHReleaseVariable)(struct SHVar *variable);

typedef void(__cdecl *SHAbortWire)(struct SHContext *context, SHString errorText);

#if defined(__cplusplus) || defined(SH_USE_ENUMS)
typedef enum SHWireState(__cdecl *SHSuspend)(struct SHContext *context, double seconds);
typedef enum SHWireState(__cdecl *SHGetState)(struct SHContext *context);
#else
typedef SHWireState(__cdecl *SHSuspend)(struct SHContext *context, double seconds);
typedef SHWireState(__cdecl *SHGetState)(struct SHContext *context);
#endif

typedef void(__cdecl *SHCloneVar)(struct SHVar *dst, const struct SHVar *src);

typedef void(__cdecl *SHDestroyVar)(struct SHVar *var);

typedef SHBool(__cdecl *SHValidateSetParam)(struct Shard *shard, int index, const struct SHVar *param,
                                            SHValidationCallback callback, void *userData);

typedef struct SHComposeResult(__cdecl *SHComposeShards)(Shards shards, SHValidationCallback callback, void *userData,
                                                         struct SHInstanceData data);

#if defined(__cplusplus) || defined(SH_USE_ENUMS)
typedef enum SHWireState(__cdecl *SHRunShards)(Shards shards, struct SHContext *context, const struct SHVar *input,
                                               struct SHVar *output);
#else
typedef SHWireState(__cdecl *SHRunShards)(Shards shards, struct SHContext *context, const struct SHVar *input,
                                          struct SHVar *output);
#endif

#if defined(__cplusplus) || defined(SH_USE_ENUMS)
typedef enum SHWireState(__cdecl *SHRunShardsHashed)(Shards shards, struct SHContext *context, const struct SHVar *input,
                                                     struct SHVar *output, struct SHVar *outHash);
#else
typedef SHWireState(__cdecl *SHRunShardsHashed)(Shards shards, struct SHContext *context, const struct SHVar *input,
                                                struct SHVar *output, struct SHVar *outHash);
#endif

typedef void(__cdecl *SHLog)(SHString msg);
typedef void(__cdecl *SHLogLevel)(int level, SHString msg);

typedef struct Shard *(__cdecl *SHCreateShard)(SHString name);

typedef SHWireRef(__cdecl *SHCreateWire)();
typedef void(__cdecl *SHSetWireName)(SHWireRef wire, SHString name);
typedef void(__cdecl *SHSetWireLooped)(SHWireRef wire, SHBool looped);
typedef void(__cdecl *SHSetWireUnsafe)(SHWireRef wire, SHBool unsafe);
typedef void(__cdecl *SHAddShard)(SHWireRef wire, ShardPtr shard);
typedef void(__cdecl *SHRemShard)(SHWireRef wire, ShardPtr shard);
typedef void(__cdecl *SHDestroyWire)(SHWireRef wire);
typedef struct SHVar(__cdecl *SHStopWire)(SHWireRef wire);
typedef struct SHComposeResult(__cdecl *SHComposeWire)(SHWireRef wire, SHValidationCallback callback, void *userData,
                                                       struct SHInstanceData data);
typedef struct SHRunWireOutput(__cdecl *SHRunWire)(SHWireRef wire, struct SHContext *context, const struct SHVar *input);
typedef SHWireRef(__cdecl *SHGetGlobalWire)(SHString name);
typedef void(__cdecl *SHSetGlobalWire)(SHString name, SHWireRef wire);
typedef void(__cdecl *SHUnsetGlobalWire)(SHString name);

typedef SHMeshRef(__cdecl *SHCreateMesh)();
typedef void(__cdecl *SHDestroyMesh)(SHMeshRef mesh);
typedef void(__cdecl *SHSchedule)(SHMeshRef mesh, SHWireRef wire);
typedef SHBool(__cdecl *SHTick)(SHMeshRef mesh);
typedef void(__cdecl *SHSleep)(double seconds, SHBool runCallbacks);

#define SH_ARRAY_TYPE(_array_, _value_)                                          \
  typedef void(__cdecl * _array_##Free)(_array_ *);                              \
  typedef void(__cdecl * _array_##Push)(_array_ *, const _value_ *);             \
  typedef void(__cdecl * _array_##Insert)(_array_ *, uint32_t, const _value_ *); \
  typedef _value_(__cdecl *_array_##Pop)(_array_ *);                             \
  typedef void(__cdecl * _array_##Resize)(_array_ *, uint32_t);                  \
  typedef void(__cdecl * _array_##FastDelete)(_array_ *, uint32_t);              \
  typedef void(__cdecl * _array_##SlowDelete)(_array_ *, uint32_t)

SH_ARRAY_TYPE(SHSeq, struct SHVar);
SH_ARRAY_TYPE(SHTypesInfo, struct SHTypeInfo);
SH_ARRAY_TYPE(SHParametersInfo, struct SHParameterInfo);
SH_ARRAY_TYPE(Shards, ShardPtr);
SH_ARRAY_TYPE(SHExposedTypesInfo, struct SHExposedTypeInfo);
SH_ARRAY_TYPE(SHEnums, SHEnum);
SH_ARRAY_TYPE(SHStrings, SHString);

#define SH_ARRAY_PROCS(_array_, _short_)   \
  _array_##Free _short_##Free;             \
  _array_##Push _short_##Push;             \
  _array_##Insert _short_##Insert;         \
  _array_##Pop _short_##Pop;               \
  _array_##Resize _short_##Resize;         \
  _array_##FastDelete _short_##FastDelete; \
  _array_##SlowDelete _short_##SlowDelete

typedef struct SHTable(__cdecl *SHTableNew)();

typedef struct SHSet(__cdecl *SHSetNew)();

typedef SHString(__cdecl *SHGetRootPath)();
typedef void(__cdecl *SHSetRootPath)(SHString);

typedef struct SHVar(__cdecl *SHAsyncActivateProc)(struct SHContext *context, void *userData);

typedef void(__cdecl *SHAsyncCancelProc)(struct SHContext *context, void *userData);

typedef struct SHVar(__cdecl *SHRunAsyncActivate)(struct SHContext *context, void *userData, SHAsyncActivateProc call,
                                                  SHAsyncCancelProc cancel_call);

typedef SHStrings(__cdecl *SHGetShards)();

struct SHWireInfo {
  SHString name;
  SHBool looped;
  SHBool unsafe;
  const struct SHWire *wire;
  const Shards shards;
  SHBool isRunning;
  SHBool failed;
  SHString failureMessage;
  struct SHVar *finalOutput;
};

typedef struct SHWireInfo(__cdecl *SHGetWireInfo)(SHWireRef wireref);

// id is generally a crc32 value
// use this when the cache is compiled
typedef SHOptionalString(__cdecl *SHReadCachedString)(uint32_t id);
// the string is not copied! should be constant.
// use this when the cache is not precompiled
typedef SHOptionalString(__cdecl *SHWriteCachedString)(uint32_t id, const char *str);

typedef SHBool(__cdecl *SHIsEqualVar)(const struct SHVar *v1, const struct SHVar *v2);
typedef SHBool(__cdecl *SHIsEqualType)(const struct SHTypeInfo *t1, const struct SHTypeInfo *t2);

typedef struct SHTypeInfo(__cdecl *SHDeriveTypeInfo)(const struct SHVar *v, const struct SHInstanceData *data);
typedef void(__cdecl *SHFreeDerivedTypeInfo)(struct SHTypeInfo *t);

typedef void *(__cdecl *SHAlloc)(uint32_t size);
typedef void(__cdecl *SHFree)(void *ptr);

typedef struct _SHCore {
  // Aligned allocator
  SHAlloc alloc;
  SHFree free;

  // SHTable interface
  SHTableNew tableNew;

  // SHSet interface
  SHSetNew setNew;

  // Utility to use shards within shards
  SHComposeShards composeShards;
  // caller not handling return state
  SHRunShards runShards;
  // caller handles return state
  SHRunShards runShards2;
  // caller not handling return state
  SHRunShardsHashed runShardsHashed;
  // caller handles return state
  SHRunShardsHashed runShardsHashed2;

  // Logging
  SHLog log;
  SHLogLevel logLevel;

  // Wire creation
  SHCreateShard createShard;
  SHValidateSetParam validateSetParam;

  SHCreateWire createWire;
  SHSetWireName setWireName;
  SHSetWireLooped setWireLooped;
  SHSetWireUnsafe setWireUnsafe;
  SHAddShard addShard;
  SHRemShard removeShard;
  SHDestroyWire destroyWire;
  SHStopWire stopWire; // must destroyVar once done
  SHComposeWire composeWire;
  SHRunWire runWire;
  SHGetWireInfo getWireInfo;
  SHGetGlobalWire getGlobalWire;
  SHSetGlobalWire setGlobalWire;
  SHUnsetGlobalWire unsetGlobalWire;

  // Wire scheduling
  SHCreateMesh createMesh;
  SHDestroyMesh destroyMesh;
  SHSchedule schedule;
  SHSchedule unschedule;
  SHTick tick; // returns false if all wires are done!
  SHSleep sleep;

  // Environment utilities
  SHGetRootPath getRootPath;
  SHSetRootPath setRootPath;

  // async execution
  SHRunAsyncActivate asyncActivate;

  // Shards discovery (free after use, only the array, not the strings)
  SHGetShards getShards;

  // Adds a shard to the runtime database
  SHRegisterShard registerShard;
  // Adds a custom object type to the runtime database
  SHRegisterObjectType registerObjectType;
  // Adds a custom enumeration type to the runtime database
  SHRegisterEnumType registerEnumType;

  // Adds a custom call to call every shards sleep/yield internally
  // These call will run on a single thread, usually the main, but they are safe
  // due to the fact it runs after all SHMeshs ticked once
  SHRegisterRunLoopCallback registerRunLoopCallback;
  // Removes a previously added run loop callback
  SHUnregisterRunLoopCallback unregisterRunLoopCallback;

  // Adds a custom call to be called on final application exit
  SHRegisterExitCallback registerExitCallback;
  // Removes a previously added exit callback
  SHUnregisterExitCallback unregisterExitCallback;

  // To be used within shards, to manipulate variables
  SHReferenceVariable referenceVariable;
  SHReferenceWireVariable referenceWireVariable;
  SHReleaseVariable releaseVariable;

  // To add/rem external variables to wires
  SHSetExternalVariable setExternalVariable;
  SHRemoveExternalVariable removeExternalVariable;
  // Alternatives of the above that allocate memory for the variables
  SHAllocExternalVariable allocExternalVariable;
  SHFreeExternalVariable freeExternalVariable;

  // To be used within shards, to suspend the coroutine
  SHSuspend suspend;
  SHGetState getState;
  // To be used within shards, to abort the wire
  // after this call you should return immediately from activate
  SHAbortWire abortWire;

  // Utility to deal with SHVars
  SHCloneVar cloneVar;
  SHDestroyVar destroyVar;

  // Compressed strings utility
  SHReadCachedString readCachedString;
  SHWriteCachedString writeCachedString;

  // equality utilities
  SHIsEqualVar isEqualVar;
  SHIsEqualType isEqualType;
  SHDeriveTypeInfo deriveTypeInfo;
  SHFreeDerivedTypeInfo freeDerivedTypeInfo;

  // Utility to deal with SHSeqs
  SH_ARRAY_PROCS(SHSeq, seq);

  // Utility to deal with SHTypesInfo
  SH_ARRAY_PROCS(SHTypesInfo, types);

  // Utility to deal with SHParamsInfo
  SH_ARRAY_PROCS(SHParametersInfo, params);

  // Utility to deal with Shards
  SH_ARRAY_PROCS(Shards, shards);

  // Utility to deal with SHExposedTypeInfo
  SH_ARRAY_PROCS(SHExposedTypesInfo, expTypes);

  // Utility to deal with SHEnums
  SH_ARRAY_PROCS(SHEnums, enums);

  // Utility to deal with SHStrings
  SH_ARRAY_PROCS(SHStrings, strings);
} SHCore;

typedef SHCore *(__cdecl *SHShardsInterface)(uint32_t abi_version);

#ifdef SHARDS_CORE_DLL
#ifdef _WIN32
#define SHARDS_EXPORT __declspec(dllexport)
#define SHARDS_IMPORT __declspec(dllimport)
#else
#define SHARDS_EXPORT __attribute__((visibility("default")))
#define SHARDS_IMPORT
#endif
#else
#define SHARDS_EXPORT
#define SHARDS_IMPORT
#endif

#ifdef shards_core_EXPORTS
#define SHARDS_API SHARDS_EXPORT
#else
#define SHARDS_API SHARDS_IMPORT
#endif

#define SHARDS_CURRENT_ABI 0x20200101
#define SHARDS_CURRENT_ABI_STR "0x20200101"

#if defined(__cplusplus)
extern "C" {
#endif
SHARDS_API SHCore *__cdecl shardsInterface(uint32_t abi_version);
#if defined(__cplusplus)
};
#endif

// For future proper use
// Basically we want during development of wires to have those on
// When baking exe/lib/release we turn them off
#define shassert assert
#ifndef NDEBUG
#define SH_DEBUG_MODE 1
#else
#define SH_DEBUG_MODE 0
#endif
#define sh_debug_only(__CODE__) \
  if (SH_DEBUG_MODE) {          \
    __CODE__;                   \
  }

#define SH_CAT_IMPL(s1, s2) s1##s2
#define SH_CAT(s1, s2) CAT_IMPL(s1, s2)

#ifdef __COUNTER__
#define SH_GENSYM(str) SH_CAT(str, __COUNTER__)
#else
#define SH_GENSYM(str) SH_CAT(str, __LINE__)
#endif

#endif
