#pragma once

/*
  TODO:
  Make this header C compatible when CHAINBLOCKS_RUNTIME is not defined.
*/

// Use only basic types and libs, we want full ABI compatibility between blocks
// Cannot afford to use any C++ std as any block maker should be free to use their versions

#include <stdint.h>
#include "3rdparty/stb_ds.h"

// All the available types
enum CBType : uint8_t
{
  None,
  Any,
  Object,
  Enum,
  Bool,
  Int,    // A 64bits int
  Int2,   // A vector of 2 64bits ints
  Int3,   // A vector of 3 32bits int
  Int4,   // A vector of 4 32bits int
  Float,  // A 64bits float
  Float2, // A vector of 2 64bits floats
  Float3, // A vector of 3 32bits floats
  Float4, // A vector of 4 32bits floats
  String,
  Color,  // A vector of 4 uint8
  Image,
  Seq,
  Table,
  Chain, // sub chains, e.g. IF/ELSE
  Block, // a block, useful for future introspection blocks!
  Type,
  ContextVar, // A string label to find from CBContext variables
};

enum CBChainState : uint8_t
{
  Continue, // Even if None returned, continue to next block
  Restart, // Restart the chain from the top
  Stop // Stop the chain execution
};

enum CBInlineBlocks : uint8_t
{
  NotInline,

  CoreConst,
  CoreSleep,
  CoreRepeat,
  CoreIf,
  CoreSetVariable,
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
typedef CBVar* CBSeq;
struct CBNamedVar;
typedef CBNamedVar* CBTable;
struct CBChain;
typedef CBChain* CBChainPtr;
struct CBRuntimeBlock;
struct CBTypeInfo;
typedef CBTypeInfo* CBTypesInfo;
struct CBParameterInfo;
typedef CBParameterInfo* CBParametersInfo;
struct CBContext;

typedef void* CBPointer;
typedef int64_t CBInt;
typedef double CBFloat;
typedef bool CBBool;
typedef int32_t CBEnum;
typedef const char* CBString;
typedef CBString* CBStrings;

#if defined(__clang__) || defined(__GNUC__)
  #define likely(x)       __builtin_expect((x),1)
  #define unlikely(x)     __builtin_expect((x),0)
  
  typedef int64_t CBInt2 __attribute__((vector_size(16)));
  typedef int32_t CBInt3 __attribute__((vector_size(16)));
  typedef int32_t CBInt4 __attribute__((vector_size(16)));

  typedef double CBFloat2 __attribute__((vector_size(16)));
  typedef float CBFloat3 __attribute__((vector_size(16)));
  typedef float CBFloat4 __attribute__((vector_size(16)));
#else
  typedef int64_t CBInt2[2];
  typedef int32_t CBInt3[3];
  typedef int32_t CBInt4[4];

  typedef double CBFloat2[2];
  typedef float CBFloat3[3];
  typedef float CBFloat4[4];
#endif

#ifndef _WIN32 
#define __cdecl __attribute__((__cdecl__))
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
    {
      delete[] data;
      data = nullptr;
    }
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

typedef const char* (__cdecl *CBObjectSerializer)(CBPointer);
typedef CBPointer (__cdecl *CBObjectDeserializer)(const char*);
typedef CBPointer (__cdecl *CBObjectDestroy)(CBPointer);

struct CBObjectInfo
{
  const char* name;
  CBObjectSerializer serialize;
  CBObjectDeserializer deserialize;
  CBObjectDestroy destroy;
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
  
  ### What about exposed/consumedVariables, parameters and input/outputTypes:
  * Same for them, they are just read only basically
*/

#ifdef _MSC_VER
__declspec(align(16))
#endif
struct CBVar // will be 48 bytes, must be 16 aligned due to vectors
{
  CBVar() : valueType(None), intValue(0) {}
  
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

    CBTable tableValue;

    CBString stringValue;

    CBColor colorValue;
    
    CBImage imageValue;

    CBChainPtr* chainValue;

    CBRuntimeBlock* blockValue;

    struct {
      CBEnum enumValue;
      int32_t enumVendorId;
      int32_t enumTypeId;
    };
  };

  CBType valueType;

  uint8_t reserved[15];

  // Use with care, mostly internal (json) and only if you know you own the memory, this is just utility
  void releaseMemory();
};

struct CBNamedVar
{
  const char* key;
  CBVar value;
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
    inlineBlockId(NotInline),
    name(nullptr), help(nullptr), setup(nullptr), destroy(nullptr),
    preChain(nullptr), postChain(nullptr), inputTypes(nullptr), outputTypes(nullptr),
    exposedVariables(nullptr), consumedVariables(nullptr), parameters(nullptr), setParam(nullptr),
    getParam(nullptr), activate(nullptr), cleanup(nullptr)
  {}

  CBInlineBlocks inlineBlockId;

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
