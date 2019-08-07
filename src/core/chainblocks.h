#pragma once

/*
  TODO:
  Make this header *should be/TODO* C compatible.
*/

// Use only basic types and libs, we want full ABI compatibility between blocks
// Cannot afford to use any C++ std as any block maker should be free to use
// their versions

#include <stdbool.h>
#include <stdint.h>

// Included 3rdparty
#include "3rdparty/gb_string.h"
#include "3rdparty/stb_ds.h"

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

  EndOfBlittableTypes, // anything below this is not blittable

  String,
  ContextVar, // A string label to find from CBContext variables
  Image,
  Seq,
  Table
};

enum CBChainState : uint8_t {
  Continue, // Even if None returned, continue to next block
  Restart,  // Restart the chain from the top
  Stop      // Stop the chain execution
};

// These blocks exist in nim too but they are actually implemented here inline
// into runchain
enum CBInlineBlocks : uint8_t {
  NotInline,

  CoreConst,
  CoreSleep,
  CoreRepeat,
  CoreIf,
  CoreGetVariable,
  CoreSwapVariables,

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

// Forward declarations
struct CBVar;
typedef CBVar *CBSeq; // a stb array

struct CBNamedVar;
typedef CBNamedVar *CBTable; // a stb string map

struct CBChain;
typedef CBChain *CBChainPtr;

struct CBNode;

struct CBlock;
typedef CBlock **CBlocks; // a stb array

struct CBTypeInfo;
typedef CBTypeInfo *CBTypesInfo; // a stb array

struct CBParameterInfo;
typedef CBParameterInfo *CBParametersInfo; // a stb array

struct CBExposedTypeInfo;
typedef CBExposedTypeInfo *CBExposedTypesInfo; // a stb array

struct CBContext;

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

typedef int64_t CBInt2 __attribute__((vector_size(16)));
typedef int32_t CBInt3 __attribute__((vector_size(16)));
typedef int32_t CBInt4 __attribute__((vector_size(16)));
typedef int16_t CBInt8 __attribute__((vector_size(16)));
typedef int8_t CBInt16 __attribute__((vector_size(16)));

typedef double CBFloat2 __attribute__((vector_size(16)));
typedef float CBFloat3 __attribute__((vector_size(16)));
typedef float CBFloat4 __attribute__((vector_size(16)));

#define ALIGNED
#else
typedef int64_t CBInt2[2];
typedef int32_t CBInt3[3];
typedef int32_t CBInt4[4];
typedef int16_t CBInt8[8];
typedef int8_t CBInt16[16];

typedef double CBFloat2[2];
typedef float CBFloat3[3];
typedef float CBFloat4[4];

#define ALIGNED __declspec(align(16))
#endif

#ifndef _WIN32
#define __cdecl __attribute__((__cdecl__))
#endif

struct CBColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
};

struct CBImage {
  uint16_t width;
  uint16_t height;
  uint8_t channels;
  uint8_t *data;
};

struct CBTypeInfo {
  CBType basicType;

  // this is a way to express in a simpler manner that this type might also be
  // an array instead of basically using seqTypes, populate it and make this Seq
  // as basicType Usually this represents a flat single dimentional arrays of
  // CBTypes, and generally basicType == Seq and sequenced == true is
  // discouraged
  bool sequenced;

  union {
    struct {
      int32_t objectVendorId;
      int32_t objectTypeId;
    };

    struct {
      int32_t enumVendorId;
      int32_t enumTypeId;
    };

    // If we are a seq, the possible types present in this seq
    CBTypesInfo seqTypes;

    // If we are a table, the possible types present in this table
    CBTypesInfo tableTypes;
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
  CBTypeInfo exposedType;
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

// ### Type safety of outputs
// * A block should return a StopChain variable and set error if there is any
// error and it cannot provide the expected output type. None doesn't mean safe,
// stop/restart is safe

ALIGNED struct CBVarPayload // will be 32 bytes, must be 16 aligned due to
                            // vectors
{
  union {
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
    CBInt8 int8Value;
    CBInt16 int16Value;

    CBFloat floatValue;
    CBFloat2 float2Value;
    CBFloat3 float3Value;
    CBFloat4 float4Value;

    struct {
      CBSeq seqValue;
      // If seqLen is -1, use stbds_arrlen, assume it's a stb dynamic array
      // Operations between blocks are always using dynamic arrays
      // Len should be used only internally
      int32_t seqLen;
    };

    struct {
      CBTable tableValue;
      // If tableLen is -1, use stbds_shlen, assume it's a stb string map
      // Operations between blocks are always using dynamic arrays
      // Len should be used only internally
      int32_t tableLen;
    };

    CBString stringValue;

    CBColor colorValue;

    CBImage imageValue;

    CBChainPtr chainValue;

    CBlock *blockValue;

    struct {
      CBEnum enumValue;
      int32_t enumVendorId;
      int32_t enumTypeId;
    };
  };
};

ALIGNED struct CBVar {
  CBVarPayload payload;
  CBType valueType;
  // reserved to the owner of the var, cloner, block etc to do whatever they
  // need to do :)
  uint8_t reserved[15];
};

ALIGNED struct CBNamedVar {
  const char *key;
  CBVar value;
};

typedef CBlock *(__cdecl *CBBlockConstructor)();
typedef void(__cdecl *CBCallback)();

typedef const char *(__cdecl *CBNameProc)(CBlock *);
typedef const char *(__cdecl *CBHelpProc)(CBlock *);

// Construction/Destruction
typedef void(__cdecl *CBSetupProc)(CBlock *);
typedef void(__cdecl *CBDestroyProc)(CBlock *);

typedef CBTypesInfo(__cdecl *CBInputTypesProc)(CBlock *);
typedef CBTypesInfo(__cdecl *CBOutputTypesProc)(CBlock *);

typedef CBExposedTypesInfo(__cdecl *CBExposedVariablesProc)(CBlock *);
typedef CBExposedTypesInfo(__cdecl *CBConsumedVariablesProc)(CBlock *);

typedef CBParametersInfo(__cdecl *CBParametersProc)(CBlock *);
typedef void(__cdecl *CBSetParamProc)(CBlock *, int, CBVar);
typedef CBVar(__cdecl *CBGetParamProc)(CBlock *, int);

typedef CBTypeInfo(__cdecl *CBInferTypesProc)(
    CBlock *, CBTypeInfo inputType, CBExposedTypesInfo consumableVariables);

// All those happen inside a coroutine
typedef void(__cdecl *CBPreChainProc)(CBlock *, CBContext *);
typedef CBVar(__cdecl *CBActivateProc)(CBlock *, CBContext *, CBVar);
typedef void(__cdecl *CBPostChainProc)(CBlock *, CBContext *);

// Generally when stop() is called
typedef void(__cdecl *CBCleanupProc)(CBlock *);

struct CBlock {
  CBInlineBlocks inlineBlockId;

  CBNameProc name; // Returns the name of the block, do not free the string,
                   // generally const
  CBHelpProc help; // Returns the help text of the block, do not free the
                   // string, generally const

  CBSetupProc setup;     // A one time construtor setup for the block
  CBDestroyProc destroy; // A one time finalizer for the block, blocks should
                         // also free all the memory in here!

  CBPreChainProc
      preChain; // Called inside the coro before a chain starts, optional
  CBPostChainProc
      postChain; // Called inside the coro afer a chain ends, optional

  CBInputTypesProc inputTypes;
  CBOutputTypesProc outputTypes;

  CBExposedVariablesProc exposedVariables;
  CBConsumedVariablesProc consumedVariables;

  CBInferTypesProc
      inferTypes; // Optional call used during validation to fixup "Any" input
                  // type and provide valid output and exposed variable types

  CBParametersProc parameters;
  CBSetParamProc setParam; // Set a parameter, the block will copy the value, so
                           // if you allocated any memory you should free it
  CBGetParamProc getParam; // Gets a parameter, the block is the owner of any
                           // allocated stuff, DO NOT free them

  CBActivateProc activate;
  CBCleanupProc cleanup; // Called every time you stop a coroutine or sometimes
                         // internally to clean up the block state
};

typedef void(__cdecl *CBValidationCallback)(const CBlock *errorBlock,
                                            const char *errorTxt,
                                            bool nonfatalWarning,
                                            void *userData);

#ifdef _WIN32
#ifdef DLL_EXPORT
#define EXPORTED __declspec(dllexport)
#else
#define EXPORTED __declspec(dllimport)
#endif
#else
#define EXPORTED
#endif

#ifdef __cplusplus
extern "C" {
#endif

// The runtime (even if it is an exe), will export the following, they need to
// be available in order to load and work with blocks collections within dlls

// Adds a block to the runtime database
EXPORTED void __cdecl chainblocks_RegisterBlock(const char *fullName,
                                                CBBlockConstructor constructor);
// Adds a custom object type to the runtime database
EXPORTED void __cdecl chainblocks_RegisterObjectType(int32_t vendorId,
                                                     int32_t typeId,
                                                     CBObjectInfo info);
// Adds a custom enumeration type to the runtime database
EXPORTED void __cdecl chainblocks_RegisterEnumType(int32_t vendorId,
                                                   int32_t typeId,
                                                   CBEnumInfo info);
// Adds a custom call to call every chainblocks sleep internally (basically
// every frame)
EXPORTED void __cdecl chainblocks_RegisterRunLoopCallback(const char *eventName,
                                                          CBCallback callback);
// Adds a custom call to be called on final application exit
EXPORTED void __cdecl chainblocks_RegisterExitCallback(const char *eventName,
                                                       CBCallback callback);
// Removes a previously added run loop callback
EXPORTED void __cdecl chainblocks_UnregisterRunLoopCallback(
    const char *eventName);
// Removes a previously added exit callback
EXPORTED void __cdecl chainblocks_UnregisterExitCallback(const char *eventName);

// To be used within blocks, to fetch context variables
EXPORTED CBVar *__cdecl chainblocks_ContextVariable(
    CBContext *context,
    const char *name); // remember those are valid only inside preChain,
                       // activate, postChain!

// To be used within blocks, to set error messages and exceptions
EXPORTED void __cdecl chainblocks_SetError(CBContext *context,
                                           const char *errorText);
EXPORTED void __cdecl chainblocks_ThrowException(const char *errorText);

// To check if we aborted/canceled in this context, 0 = running, 1 = canceled,
// might add more flags in the future
EXPORTED int __cdecl chainblocks_ContextState(CBContext *context);

// To be used within blocks, to suspend the coroutine
EXPORTED CBVar __cdecl chainblocks_Suspend(double seconds);

// Utility to deal with CBVars
EXPORTED void __cdecl chainblocks_CloneVar(CBVar *dst, const CBVar *src);
EXPORTED void __cdecl chainblocks_DestroyVar(CBVar *var);

// Utility to use blocks within blocks
EXPORTED void __cdecl chainblocks_ActivateBlock(CBlock *block,
                                                CBContext *context,
                                                CBVar *input, CBVar *output);
EXPORTED CBTypeInfo __cdecl chainblocks_ValidateConnections(
    CBlocks chain, CBValidationCallback callback, void *userData,
    CBTypeInfo inputType);

// Logging
EXPORTED void __cdecl chainblocks_Log(int level, const char *msg, ...);

#define CB_DEBUG 1
#define CB_INFO 2
#define CB_TRACE 3

#ifdef NDEBUG
#define CB_LOG_DEBUG(...) (void)0
#else
#define CB_LOG_DEBUG(...) chainblocks_Log(CB_DEBUG, __VA_ARGS__)
#endif

#ifdef CB_TRACING
#define CB_LOG_TRACE(...) chainblocks_Log(CB_TRACE, __VA_ARGS__)
#else
#define CB_LOG_TRACE(...) (void)0
#endif

#define CB_LOG(...) chainblocks_Log(CB_INFO, __VA_ARGS__)

#ifdef __cplusplus
};
#endif
