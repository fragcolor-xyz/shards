/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

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
#include "rpmalloc/rpmalloc.h"
inline void* rp_init_realloc(void* ptr, size_t size) {
  rpmalloc_initialize();
  return rprealloc(ptr, size);
}
#define STBDS_REALLOC(context, ptr, size) rp_init_realloc(ptr, size)
#define STBDS_FREE(context, ptr) rpfree(ptr)
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
  Bytes, // pointer + size, we don't deep copy, but pass by ref instead

  EndOfBlittableTypes, // anything below this is not blittable (not exactly but for cloneVar mostly)

  String,
  ContextVar, // A string label to find from CBContext variables
  Image,
  Seq,
  Table
};

enum CBChainState : uint8_t {
  Continue, // Nothing happened, continue
  Rebase, // Continue this chain but put the local chain initial input as next input
  Restart, // Restart the local chain from the top (notice not the root!)
  Return, // Used in conditional paths, end this chain and return previous output
  Stop, // Stop the chain execution (including root)
};

// These blocks exist in nim too but they are actually implemented here inline
// into runchain
enum CBInlineBlocks : uint8_t {
  NotInline,

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
  CoreTake,
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

// Forward declarations
struct CBVar;
typedef struct CBVar *CBSeq; // a stb array

struct CBNamedVar;
typedef struct CBNamedVar *CBTable; // a stb string map

struct CBChain;
typedef struct CBChain *CBChainPtr;

struct CBNode;

struct CBlock;
typedef struct CBlock **CBlocks; // a stb array

struct CBTypeInfo;
typedef struct CBTypeInfo *CBTypesInfo; // a stb array

struct CBParameterInfo;
typedef struct CBParameterInfo *CBParametersInfo; // a stb array

struct CBExposedTypeInfo;
typedef struct CBExposedTypeInfo *CBExposedTypesInfo; // a stb array

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

#define ALWAYS_INLINE
#endif

#ifndef _WIN32
#ifdef I386_BUILD
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

    // If we are a seq, the possible type present in this seq
    CBTypeInfo* seqType;

    // If we are a table, the possible types present in this table
    struct {
      CBStrings tableKeys;
      CBTypesInfo tableTypes;
    };
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
  CBTypeInfo outputType;
  CBExposedTypesInfo exposedInfo;
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

ALIGNED struct CBVarPayload // will be 32 bytes, 16 aligned due to
                            // vectors
{
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

    CBSeq seqValue;

    CBTable tableValue;

    CBString stringValue;

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
      uint8_t* bytesValue;
      uint64_t bytesSize;
    };
  };
};

ALIGNED struct CBVar {
  struct CBVarPayload payload;
  enum CBType valueType;
  // reserved to the owner of the var, cloner, block etc to do whatever they
  // need to do :)
  uint8_t reserved[15];
};

ALIGNED struct CBNamedVar {
  const char *key;
  struct CBVar value;
};

enum CBRunChainOutputState { Running, Restarted, Stopped, Failed };

struct CBRunChainOutput {
  struct CBVar output;
  enum CBRunChainOutputState state;
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
typedef void(__cdecl *CBSetParamProc)(struct CBlock *, int, CBVar);
typedef CBVar(__cdecl *CBGetParamProc)(struct CBlock *, int);

typedef CBTypeInfo(__cdecl *CBInferTypesProc)(
    struct CBlock *, CBTypeInfo inputType,
    CBExposedTypesInfo consumableVariables);

// All those happen inside a coroutine
typedef CBVar(__cdecl *CBActivateProc)(struct CBlock *, CBContext *, const CBVar*);

// Generally when stop() is called
typedef void(__cdecl *CBCleanupProc)(struct CBlock *);

struct CBlock {
  CBInlineBlocks inlineBlockId;

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
#ifdef CB_DLL_EXPORT
#define EXPORTED __declspec(dllexport)
#elif DLL_IMPORT
#define EXPORTED __declspec(dllimport)
#else
#define EXPORTED
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
EXPORTED void __cdecl cbRegisterBlock(const char *fullName,
                                                CBBlockConstructor constructor);
// Adds a custom object type to the runtime database
EXPORTED void __cdecl cbRegisterObjectType(int32_t vendorId,
                                                     int32_t typeId,
                                                     CBObjectInfo info);
// Adds a custom enumeration type to the runtime database
EXPORTED void __cdecl cbRegisterEnumType(int32_t vendorId,
                                                   int32_t typeId,
                                                   CBEnumInfo info);
// Adds a custom call to call every chainblocks sleep internally (basically
// every frame)
EXPORTED void __cdecl cbRegisterRunLoopCallback(const char *eventName,
                                                          CBCallback callback);
// Adds a custom call to be called on final application exit
EXPORTED void __cdecl cbRegisterExitCallback(const char *eventName,
                                                       CBCallback callback);
// Removes a previously added run loop callback
EXPORTED void __cdecl cbUnregisterRunLoopCallback(
    const char *eventName);
// Removes a previously added exit callback
EXPORTED void __cdecl cbUnregisterExitCallback(const char *eventName);

// To be used within blocks, to fetch context variables
EXPORTED CBVar *__cdecl cbContextVariable(
    CBContext *context,
    const char *name); // remember those are valid only inside preChain,
                       // activate, postChain!

// To be used within blocks, to set error messages and exceptions
EXPORTED void __cdecl cbSetError(CBContext *context,
                                           const char *errorText);
EXPORTED void __cdecl cbThrowException(const char *errorText);

// To check if we aborted/canceled in this context, 0 = running, 1 = canceled,
// might add more flags in the future
EXPORTED int __cdecl cbContextState(CBContext *context);

// To be used within blocks, to suspend the coroutine
EXPORTED CBVar __cdecl cbSuspend(CBContext *context, double seconds);

// Utility to deal with CBVars
EXPORTED void __cdecl cbCloneVar(CBVar *dst, const CBVar *src);
EXPORTED void __cdecl cbDestroyVar(CBVar *var);

// Utility to use blocks within blocks
EXPORTED __cdecl CBRunChainOutput
cbRunSubChain(CBChain *chain, CBContext *context, CBVar input);
EXPORTED CBValidationResult __cdecl cbValidateChain(
    CBChain *chain, CBValidationCallback callback, void *userData,
    CBTypeInfo inputType);
EXPORTED void __cdecl cbActivateBlock(CBlock *block,
                                                CBContext *context,
                                                CBVar *input, CBVar *output);
EXPORTED CBValidationResult __cdecl cbValidateConnections(
    CBlocks chain, CBValidationCallback callback, void *userData,
    CBTypeInfo inputType);
EXPORTED void __cdecl cbFreeValidationResult(CBValidationResult result);

// Logging
EXPORTED void __cdecl cbLog(int level, const char *msg, ...);

#define CB_DEBUG 1
#define CB_INFO 2
#define CB_TRACE 3

#ifdef NDEBUG
#define CB_LOG_DEBUG(...) (void)0
#else
#define CB_LOG_DEBUG(...) cbLog(CB_DEBUG, __VA_ARGS__)
#endif

#ifdef CB_TRACING
#define CB_LOG_TRACE(...) cbLog(CB_TRACE, __VA_ARGS__)
#else
#define CB_LOG_TRACE(...) (void)0
#endif

#define CB_LOG(...) cbLog(CB_INFO, __VA_ARGS__)

#ifdef __cplusplus
};
#endif
fo(CBType type) {
    basicType = type;
    tableKeys = nullptr;
    tableTypes = nullptr;
  }

  TypeInfo(CBTypeInfo other) {
    basicType = other.basicType;
    tableKeys = nullptr;
    tableTypes = nullptr;

    switch (basicType) {
    case CBType::Object: {
      objectVendorId = other.objectVendorId;
      objectTypeId = other.objectTypeId;
    } break;
    case CBType::Enum: {
      enumVendorId = other.enumVendorId;
      enumTypeId = other.enumTypeId;
    } break;
    case Seq: {
      if (other.seqType) {
        seqType = other.seqType;
      }
    } break;
    case Table: {
      if (other.tableTypes) {
        for (auto i = 0; i < stbds_arrlen(other.tableTypes); i++) {
          stbds_arrpush(tableTypes, other.tableTypes[i]);
        }
        for (auto i = 0; i < stbds_arrlen(other.tableKeys); i++) {
          stbds_arrpush(tableKeys, other.tableKeys[i]);
        }
      }
    } break;
    default:
      break;
    }
  }

  static TypeInfo Object(int32_t objectVendorId, int32_t objectTypeId) {
    TypeInfo result;
    result.basicType = CBType::Object;
    result.objectVendorId = objectVendorId;
    result.objectTypeId = objectTypeId;
    return result;
  }

  static TypeInfo Enum(int32_t enumVendorId, int32_t enumTypeId) {
    TypeInfo result;
    result.basicType = CBType::Enum;
    result.enumVendorId = enumVendorId;
    result.enumTypeId = enumTypeId;
    return result;
  }

  static TypeInfo Sequence(TypeInfo &contentType) {
    TypeInfo result;
    result.basicType = Seq;
    result.seqType = &contentType;
    return result;
  }

  static TypeInfo TableRecord(CBTypeInfo contentType, const char *keyName) {
    TypeInfo result;
    result.basicType = Table;
    result.tableTypes = nullptr;
    result.tableKeys = nullptr;
    stbds_arrpush(result.tableTypes, contentType);
    stbds_arrpush(result.tableKeys, keyName);
    return result;
  }

  TypeInfo(const TypeInfo &other) {
    basicType = other.basicType;
    tableKeys = nullptr;
    tableTypes = nullptr;

    switch (basicType) {
    case CBType::Object: {
      objectVendorId = other.objectVendorId;
      objectTypeId = other.objectTypeId;
    } break;
    case CBType::Enum: {
      enumVendorId = other.enumVendorId;
      enumTypeId = other.enumTypeId;
    } break;
    case Seq: {
      if (other.seqType) {
        seqType = other.seqType;
      }
    } break;
    case Table: {
      tableKeys = nullptr;
      tableTypes = nullptr;
      if (other.tableTypes) {
        for (auto i = 0; i < stbds_arrlen(other.tableTypes); i++) {
          stbds_arrpush(tableTypes, other.tableTypes[i]);
        }
        for (auto i = 0; i < stbds_arrlen(other.tableKeys); i++) {
          stbds_arrpush(tableKeys, other.tableKeys[i]);
        }
      }
    } break;
    default:
      break;
    }
  }

  TypeInfo &operator=(const TypeInfo &other) {
    switch (basicType) {
    case Seq: {
      if (seqType)
        delete seqType;
    } break;
    case Table: {
      if (other.tableTypes) {
        stbds_arrfree(tableKeys);
        stbds_arrfree(tableTypes);
      }
    } break;
    default:
      break;
    }

    basicType = other.basicType;
    tableKeys = nullptr;
    tableTypes = nullptr;

    switch (basicType) {
    case CBType::Object: {
      objectVendorId = other.objectVendorId;
      objectTypeId = other.objectTypeId;
    } break;
    case CBType::Enum: {
      enumVendorId = other.enumVendorId;
      enumTypeId = other.enumTypeId;
    } break;
    case Seq: {
      if (other.seqType) {
        seqType = other.seqType;
      }
    } break;
    case Table: {
      if (other.tableTypes) {
        for (auto i = 0; i < stbds_arrlen(other.tableTypes); i++) {
          stbds_arrpush(tableTypes, other.tableTypes[i]);
        }
        for (auto i = 0; i < stbds_arrlen(other.tableKeys); i++) {
          stbds_arrpush(tableKeys, other.tableKeys[i]);
        }
      }
    } break;
    default:
      break;
    }

    return *this;
  }

  ~TypeInfo() {
    if (basicType == Table) {
      if (tableTypes) {
        stbds_arrfree(tableTypes);
        stbds_arrfree(tableKeys);
      }
    }
  }
};

struct TypesInfo {
  TypesInfo() { _innerInfo = nullptr; }

  TypesInfo(const TypesInfo &other) {
    _innerTypes = other._innerTypes;
    _innerInfo = nullptr;
    for (auto &type : _innerTypes) {
      stbds_arrpush(_innerInfo, type);
    }
  }

  TypesInfo &operator=(const TypesInfo &other) {
    _innerTypes = other._innerTypes;
    stbds_arrsetlen(_innerInfo, 0);
    for (auto &type : _innerTypes) {
      stbds_arrpush(_innerInfo, type);
    }
    return *this;
  }

  explicit TypesInfo(TypeInfo singleType, bool canBeSeq = false) {
    _innerInfo = nullptr;
    _innerTypes.reserve(canBeSeq ? 2 : 1);
    _innerTypes.push_back(singleType);
    stbds_arrpush(_innerInfo, _innerTypes.back());
    if (canBeSeq) {
      _innerTypes.push_back(TypeInfo::Sequence(_innerTypes.back()));
      stbds_arrpush(_innerInfo, _innerTypes.back());
    }
  }

  template <typename... Args>
  static TypesInfo FromMany(bool canBeSeq, Args... types) {
    TypesInfo result;
    result._innerInfo = nullptr;
    std::vector<TypeInfo> vec = {types...};

    // Preallocate in order to be able to have always valid addresses
    auto size = vec.size();
    if (canBeSeq)
      size *= 2;
    result._innerTypes.reserve(size);

    for (auto &type : vec) {
      result._innerTypes.push_back(type);
      auto &nonSeq = result._innerTypes.back();
      stbds_arrpush(result._innerInfo, nonSeq);
      if (canBeSeq) {
        result._innerTypes.push_back(TypeInfo::Sequence(nonSeq));
        auto &seqType = result._innerTypes.back();
        stbds_arrpush(result._innerInfo, seqType);
      }
    }
    return result;
  }

  ~TypesInfo() {
    if (_innerInfo) {
      stbds_arrfree(_innerInfo);
    }
  }

  explicit operator CBTypesInfo() const { return _innerInfo; }

  explicit operator CBTypeInfo() const { return _innerInfo[0]; }

  CBTypesInfo _innerInfo{};
  std::vector<TypeInfo> _innerTypes;
};

struct ParamsInfo {
  ParamsInfo(const ParamsInfo &other) {
    stbds_arrsetlen(_innerInfo, 0);
    for (auto i = 0; i < stbds_arrlen(other._innerInfo); i++) {
      stbds_arrpush(_innerInfo, other._innerInfo[i]);
    }
  }

  ParamsInfo &operator=(const ParamsInfo &other) {
    _innerInfo = nullptr;
    for (auto i = 0; i < stbds_arrlen(other._innerInfo); i++) {
      stbds_arrpush(_innerInfo, other._innerInfo[i]);
    }
    return *this;
  }

  template <typename... Types>
  explicit ParamsInfo(CBParameterInfo first, Types... types) {
    std::vector<CBParameterInfo> vec = {first, types...};
    _innerInfo = nullptr;
    for (auto pi : vec) {
      stbds_arrpush(_innerInfo, pi);
    }
  }

  template <typename... Types>
  explicit ParamsInfo(const ParamsInfo &other, CBParameterInfo first,
                      Types... types) {
    _innerInfo = nullptr;

    for (auto i = 0; i < stbds_arrlen(other._innerInfo); i++) {
      stbds_arrpush(_innerInfo, other._innerInfo[i]);
    }

    std::vector<CBParameterInfo> vec = {first, types...};
    for (auto pi : vec) {
      stbds_arrpush(_innerInfo, pi);
    }
  }

  static CBParameterInfo Param(const char *name, const char *help,
                               CBTypesInfo types) {
    CBParameterInfo res = {name, help, types};
    return res;
  }

  ~ParamsInfo() {
    if (_innerInfo)
      stbds_arrfree(_innerInfo);
  }

  explicit operator CBParametersInfo() const { return _innerInfo; }

  CBParametersInfo _innerInfo{};
};

struct ExposedInfo {
  ExposedInfo() { _innerInfo = nullptr; }

  ExposedInfo(const ExposedInfo &other) {
    _innerInfo = nullptr;
    for (auto i = 0; i < stbds_arrlen(other._innerInfo); i++) {
      stbds_arrpush(_innerInfo, other._innerInfo[i]);
    }
  }

  ExposedInfo &operator=(const ExposedInfo &other) {
    stbds_arrsetlen(_innerInfo, 0);
    for (auto i = 0; i < stbds_arrlen(other._innerInfo); i++) {
      stbds_arrpush(_innerInfo, other._innerInfo[i]);
    }
    return *this;
  }

  template <typename... Types>
  explicit ExposedInfo(const ExposedInfo &other, Types... types) {
    _innerInfo = nullptr;

    for (auto i = 0; i < stbds_arrlen(other._innerInfo); i++) {
      stbds_arrpush(_innerInfo, other._innerInfo[i]);
    }

    std::vector<CBExposedTypeInfo> vec = {types...};
    for (auto pi : vec) {
      stbds_arrpush(_innerInfo, pi);
    }
  }

  template <typename... Types>
  explicit ExposedInfo(CBExposedTypeInfo first, Types... types) {
    std::vector<CBExposedTypeInfo> vec = {first, types...};
    _innerInfo = nullptr;
    for (auto pi : vec) {
      stbds_arrpush(_innerInfo, pi);
    }
  }

  static CBExposedTypeInfo Variable(const char *name, const char *help,
                                    CBTypeInfo type, bool isMutable = false) {
    CBExposedTypeInfo res = {name, help, type, isMutable};
    return res;
  }

  ~ExposedInfo() {
    if (_innerInfo)
      stbds_arrfree(_innerInfo);
  }

  explicit operator CBExposedTypesInfo() const { return _innerInfo; }

  CBExposedTypesInfo _innerInfo;
};

struct CachedStreamBuf : std::streambuf {
  std::vector<char> data;
  void reset() { data.clear(); }

  int overflow(int c) override {
    data.push_back(static_cast<char>(c));
    return 0;
  }

  void done() { data.push_back('\0'); }

  const char *str() { return &data[0]; }
};

struct VarStringStream {
  CachedStreamBuf cache;
  CBVar previousValue{};

  ~VarStringStream() { cbDestroyVar(&previousValue); }

  void write(const CBVar &var) {
    if (var != previousValue) {
      cache.reset();
      std::ostream stream(&cache);
      stream << var;
      cache.done();
      cbCloneVar(&previousValue, &var);
    }
  }

  void tryWriteHex(const CBVar &var) {
    if (var != previousValue) {
      cache.reset();
      std::ostream stream(&cache);
      if (var.valueType == Int) {
        stream << "0x" << std::hex << var.payload.intValue << std::dec;
      } else {
        stream << var;
      }
      cache.done();
      cbCloneVar(&previousValue, &var);
    }
  }

  const char *str() { return cache.str(); }
};
}; // namespace chainblocks
ing>();
      auto findIt = chainblocks::ObjectTypesRegister.find(
          std::make_tuple(vendorId, typeId));
      if (findIt != chainblocks::ObjectTypesRegister.end() &&
          findIt->second.deserialize) {
        var.payload.objectValue =
            findIt->second.deserialize(const_cast<CBString>(value.c_str()));
      } else {
        throw chainblocks::CBException(
            "Failed to find a custom object deserializer.");
      }
    } else {
      var.payload.objectValue = nullptr;
    }
    var.payload.objectVendorId = vendorId;
    var.payload.objectTypeId = typeId;
    break;
  }
  case Bool: {
    var.valueType = Bool;
    var.payload.boolValue = j.at("value").get<bool>();
    break;
  }
  case Int: {
    var.valueType = Int;
    var.payload.intValue = j.at("value").get<int64_t>();
    break;
  }
  case Int2: {
    var.valueType = Int2;
    var.payload.int2Value[0] = j.at("value")[0].get<int64_t>();
    var.payload.int2Value[1] = j.at("value")[1].get<int64_t>();
    break;
  }
  case Int3: {
    var.valueType = Int3;
    var.payload.int3Value[0] = j.at("value")[0].get<int32_t>();
    var.payload.int3Value[1] = j.at("value")[1].get<int32_t>();
    var.payload.int3Value[2] = j.at("value")[2].get<int32_t>();
    break;
  }
  case Int4: {
    var.valueType = Int4;
    var.payload.int4Value[0] = j.at("value")[0].get<int32_t>();
    var.payload.int4Value[1] = j.at("value")[1].get<int32_t>();
    var.payload.int4Value[2] = j.at("value")[2].get<int32_t>();
    var.payload.int4Value[3] = j.at("value")[3].get<int32_t>();
    break;
  }
  case Int8: {
    var.valueType = Int8;
    for (auto i = 0; i < 8; i++) {
      var.payload.int8Value[i] = j.at("value")[i].get<int16_t>();
    }
    break;
  }
  case Int16: {
    var.valueType = Int16;
    for (auto i = 0; i < 16; i++) {
      var.payload.int16Value[i] = j.at("value")[i].get<int8_t>();
    }
    break;
  }
  case Float: {
    var.valueType = Float;
    var.payload.floatValue = j.at("value").get<double>();
    break;
  }
  case Float2: {
    var.valueType = Float2;
    var.payload.float2Value[0] = j.at("value")[0].get<double>();
    var.payload.float2Value[1] = j.at("value")[1].get<double>();
    break;
  }
  case Float3: {
    var.valueType = Float3;
    var.payload.float3Value[0] = j.at("value")[0].get<float>();
    var.payload.float3Value[1] = j.at("value")[1].get<float>();
    var.payload.float3Value[2] = j.at("value")[2].get<float>();
    break;
  }
  case Float4: {
    var.valueType = Float4;
    var.payload.float4Value[0] = j.at("value")[0].get<float>();
    var.payload.float4Value[1] = j.at("value")[1].get<float>();
    var.payload.float4Value[2] = j.at("value")[2].get<float>();
    var.payload.float4Value[3] = j.at("value")[3].get<float>();
    break;
  }
  case ContextVar: {
    var.valueType = ContextVar;
    auto strVal = j.at("value").get<std::string>();
    var.payload.stringValue = new char[strVal.length() + 1];
    memset((void *)var.payload.stringValue, 0x0, strVal.length() + 1);
    memcpy((void *)var.payload.stringValue, strVal.c_str(), strVal.length());
    break;
  }
  case String: {
    var.valueType = String;
    auto strVal = j.at("value").get<std::string>();
    var.payload.stringValue = new char[strVal.length() + 1];
    memset((void *)var.payload.stringValue, 0x0, strVal.length() + 1);
    memcpy((void *)var.payload.stringValue, strVal.c_str(), strVal.length());
    break;
  }
  case Color: {
    var.valueType = Color;
    var.payload.colorValue.r = j.at("value")[0].get<uint8_t>();
    var.payload.colorValue.g = j.at("value")[1].get<uint8_t>();
    var.payload.colorValue.b = j.at("value")[2].get<uint8_t>();
    var.payload.colorValue.a = j.at("value")[3].get<uint8_t>();
    break;
  }
  case Image: {
    var.valueType = Image;
    var.payload.imageValue.width = j.at("width").get<int32_t>();
    var.payload.imageValue.height = j.at("height").get<int32_t>();
    var.payload.imageValue.channels = j.at("channels").get<int32_t>();
    var.payload.imageValue.data = nullptr;
    alloc(var.payload.imageValue);
    auto buffer = j.at("data").get<std::vector<uint8_t>>();
    auto binsize = var.payload.imageValue.width *
                   var.payload.imageValue.height *
                   var.payload.imageValue.channels;
    memcpy(var.payload.imageValue.data, &buffer[0], binsize);
    break;
  }
  case Bytes: {
    var.valueType = Bytes;
    auto buffer = j.at("data").get<std::vector<uint8_t>>();
    var.payload.bytesValue = new uint8_t[buffer.size()];
    memcpy(var.payload.bytesValue, &buffer[0], buffer.size());
    break;
  }
  case Enum: {
    var.valueType = Enum;
    var.payload.enumValue = CBEnum(j.at("value").get<int32_t>());
    var.payload.enumVendorId = CBEnum(j.at("vendorId").get<int32_t>());
    var.payload.enumTypeId = CBEnum(j.at("typeId").get<int32_t>());
    break;
  }
  case Seq: {
    var.valueType = Seq;
    auto items = j.at("values").get<std::vector<json>>();
    var.payload.seqValue = nullptr;
    for (const auto &item : items) {
      stbds_arrpush(var.payload.seqValue, item.get<CBVar>());
    }
    break;
  }
  case Table: {
    var.valueType = Seq;
    auto items = j.at("values").get<std::vector<json>>();
    var.payload.tableValue = nullptr;
    stbds_shdefault(var.payload.tableValue, CBVar());
    for (const auto &item : items) {
      auto key = item.at("key").get<std::string>();
      auto value = item.at("value").get<CBVar>();
      CBNamedVar named{};
      named.key = new char[key.length() + 1];
      memset((void *)named.key, 0x0, key.length() + 1);
      memcpy((char *)named.key, key.c_str(), key.length());
      named.value = value;
      stbds_shputs(var.payload.tableValue, named);
    }
    break;
  }
  case Chain: {
    var.valueType = Chain;
    auto chainName = j.at("name").get<std::string>();
    auto findIt = chainblocks::GlobalChains.find(chainName);
    if (findIt != chainblocks::GlobalChains.end()) {
      var.payload.chainValue = findIt->second;
    } else {
      // create it anyway, deserialize when we can
      var.payload.chainValue = new CBChain(chainName.c_str());
      chainblocks::GlobalChains[chainName] = var.payload.chainValue;
    }
    break;
  }
  case Block: {
    var.valueType = Block;
    auto blkname = j.at("name").get<std::string>();
    auto blk = chainblocks::createBlock(blkname.c_str());
    if (!blk) {
      auto errmsg = "Failed to create block of type: " + std::string("blkname");
      throw chainblocks::CBException(errmsg.c_str());
    }
    var.payload.blockValue = blk;

    // Setup
    blk->setup(blk);

    // Set params
    auto jparams = j.at("params");
    auto blkParams = blk->parameters(blk);
    for (auto jparam : jparams) {
      auto paramName = jparam.at("name").get<std::string>();
      auto value = jparam.at("value").get<CBVar>();

      if (value.valueType != None) {
        for (auto i = 0; stbds_arrlen(blkParams) > i; i++) {
          auto &paramInfo = blkParams[i];
          if (paramName == paramInfo.name) {
            blk->setParam(blk, i, value);
            break;
          }
        }
      }

      // Assume block copied memory internally so we can clean up here!!!
      releaseMemory(value);
    }
    break;
  }
  }
}

void to_json(json &j, const CBChainPtr &chain) {
  std::vector<json> blocks;
  for (auto blk : chain->blocks) {
    std::vector<json> params;
    auto paramsDesc = blk->parameters(blk);
    for (int i = 0; stbds_arrlen(paramsDesc) > i; i++) {
      auto &desc = paramsDesc[i];
      auto value = blk->getParam(blk, i);

      json param_obj = {{"name", desc.name}, {"value", value}};

      params.push_back(param_obj);
    }

    json block_obj = {{"name", blk->name(blk)}, {"params", params}};

    blocks.push_back(block_obj);
  }

  j = {
      {"blocks", blocks},        {"name", chain->name},
      {"looped", chain->looped}, {"unsafe", chain->unsafe},
      {"version", 0.1},
  };
}

void from_json(const json &j, CBChainPtr &chain) {
  auto chainName = j.at("name").get<std::string>();
  auto findIt = chainblocks::GlobalChains.find(chainName);
  if (findIt != chainblocks::GlobalChains.end()) {
    chain = findIt->second;
    // Need to clean it up for rewrite!
    chain->cleanup();
  } else {
    chain = new CBChain(chainName.c_str());
    chainblocks::GlobalChains[chainName] = chain;
  }

  chain->looped = j.at("looped").get<bool>();
  chain->unsafe = j.at("unsafe").get<bool>();

  auto jblocks = j.at("blocks");
  for (auto jblock : jblocks) {
    auto blkname = jblock.at("name").get<std::string>();
    auto blk = chainblocks::createBlock(blkname.c_str());
    if (!blk) {
      auto errmsg = "Failed to create block of type: " + std::string(blkname);
      throw chainblocks::CBException(errmsg.c_str());
    }

    // Setup
    blk->setup(blk);

    // Set params
    auto jparams = jblock.at("params");
    auto blkParams = blk->parameters(blk);
    for (auto jparam : jparams) {
      auto paramName = jparam.at("name").get<std::string>();
      auto value = jparam.at("value").get<CBVar>();

      if (value.valueType != None) {
        for (auto i = 0; stbds_arrlen(blkParams) > i; i++) {
          auto &paramInfo = blkParams[i];
          if (paramName == paramInfo.name) {
            blk->setParam(blk, i, value);
            break;
          }
        }
      }

      // Assume block copied memory internally so we can clean up here!!!
      releaseMemory(value);
    }

    // From now on this chain owns the block
    chain->addBlock(blk);
  }
}

bool matchTypes(const CBTypeInfo &exposedType, const CBTypeInfo &consumedType,
                bool isParameter, bool strict) {
  if (consumedType.basicType == Any ||
      (!isParameter && consumedType.basicType == None))
    return true;

  if (exposedType.basicType != consumedType.basicType) {
    // Fail if basic type differs
    return false;
  }

  switch (exposedType.basicType) {
  case Object: {
    if (exposedType.objectVendorId != consumedType.objectVendorId ||
        exposedType.objectTypeId != consumedType.objectTypeId) {
      return false;
    }
    break;
  }
  case Enum: {
    if (exposedType.enumVendorId != consumedType.enumVendorId ||
        exposedType.enumTypeId != consumedType.enumTypeId) {
      return false;
    }
    break;
  }
  case Seq: {
    if (strict) {
      if (exposedType.seqType && consumedType.seqType) {
        if (!matchTypes(*exposedType.seqType, *consumedType.seqType,
                        isParameter, strict)) {
          return false;
        }
      } else if (exposedType.seqType == nullptr ||
                 consumedType.seqType == nullptr) {
        return false;
      } else if (exposedType.seqType && consumedType.seqType == nullptr &&
                 !isParameter) {
        // Assume consumer is not strict
        return true;
      }
    }
    break;
  }
  case Table: {
    if (strict) {
      auto atypes = stbds_arrlen(exposedType.tableTypes);
      auto btypes = stbds_arrlen(consumedType.tableTypes);
      //  btypes != 0 assume consumer is not strict
      for (auto i = 0; i < atypes && (isParameter || btypes != 0); i++) {
        // Go thru all exposed types and make sure we get a positive match with
        // the consumer
        auto atype = exposedType.tableTypes[i];
        auto matched = false;
        for (auto y = 0; y < btypes; y++) {
          auto btype = consumedType.tableTypes[y];
          if (matchTypes(atype, btype, isParameter, strict)) {
            matched = true;
            break;
          }
        }
        if (!matched) {
          return false;
        }
      }
    }
    break;
  }
  default:
    return true;
  }
  return true;
}

namespace std {
template <> struct hash<CBExposedTypeInfo> {
  std::size_t operator()(const CBExposedTypeInfo &typeInfo) const {
    using std::hash;
    using std::size_t;
    using std::string;
    auto res = hash<string>()(typeInfo.name);
    res = res ^ hash<int>()(typeInfo.exposedType.basicType);
    res = res ^ hash<int>()(typeInfo.isMutable);
    if (typeInfo.exposedType.basicType == Table &&
        typeInfo.exposedType.tableTypes && typeInfo.exposedType.tableKeys) {
      for (auto i = 0; i < stbds_arrlen(typeInfo.exposedType.tableKeys); i++) {
        res = res ^ hash<string>()(typeInfo.exposedType.tableKeys[i]);
      }
    } else if (typeInfo.exposedType.basicType == Seq &&
               typeInfo.exposedType.seqType) {
      res = res ^ hash<int>()(typeInfo.exposedType.seqType->basicType);
    }
    return res;
  }
};
} // namespace std

struct ValidationContext {
  phmap::flat_hash_map<std::string, phmap::flat_hash_set<CBExposedTypeInfo>>
      exposed;
  phmap::flat_hash_set<std::string> variables;
  phmap::flat_hash_set<std::string> references;

  CBTypeInfo previousOutputType{};
  CBTypeInfo originalInputType{};

  CBlock *bottom{};

  CBValidationCallback cb{};
  void *userData{};
};

struct ConsumedParam {
  const char *key;
  CBParameterInfo value;
};

void validateConnection(ValidationContext &ctx) {
  auto previousOutput = ctx.previousOutputType;

  auto inputInfos = ctx.bottom->inputTypes(ctx.bottom);
  auto inputMatches = false;
  // validate our generic input
  for (auto i = 0; stbds_arrlen(inputInfos) > i; i++) {
    auto &inputInfo = inputInfos[i];
    if (matchTypes(previousOutput, inputInfo, false, false)) {
      inputMatches = true;
      break;
    }
  }

  if (!inputMatches) {
    std::string err("Could not find a matching input type, block: " +
                    std::string(ctx.bottom->name(ctx.bottom)));
    ctx.cb(ctx.bottom, err.c_str(), false, ctx.userData);
  }

  // infer and specialize types if we need to
  // If we don't we assume our output will be of the same type of the previous!
  if (ctx.bottom->inferTypes) {
    CBExposedTypesInfo consumables = nullptr;
    // Pass all we got in the context!
    for (auto &info : ctx.exposed) {
      for (auto &type : info.second) {
        stbds_arrpush(consumables, type);
      }
    }
    // this ensures e.g. SetVariable exposedVars have right type from the actual
    // input type (previousOutput)!
    ctx.previousOutputType =
        ctx.bottom->inferTypes(ctx.bottom, previousOutput, consumables);

    stbds_arrfree(consumables);
  } else {
    // Short-cut if it's just one type and not any type
    // Any type tho means keep previous output type!
    auto outputTypes = ctx.bottom->outputTypes(ctx.bottom);
    if (stbds_arrlen(outputTypes) == 1 && outputTypes[0].basicType != Any) {
      ctx.previousOutputType = outputTypes[0];
    }
  }

  // Grab those after type inference!
  auto exposedVars = ctx.bottom->exposedVariables(ctx.bottom);
  // Add the vars we expose
  for (auto i = 0; stbds_arrlen(exposedVars) > i; i++) {
    auto &exposed_param = exposedVars[i];
    std::string name(exposed_param.name);
    ctx.exposed[name].insert(exposed_param);

    // Reference mutability checks
    if (strcmp(ctx.bottom->name(ctx.bottom), "Ref") == 0) {
      // make sure we are not Ref-ing a Set
      // meaning target would be overwritten, yet Set will try to deallocate
      // it/manage it
      if (ctx.variables.contains(name)) {
        // Error
        std::string err(
            "Ref variable name already used as Set. Overwriting a previously "
            "Set variable with Ref is not allowed, name: " +
            name);
        ctx.cb(ctx.bottom, err.c_str(), false, ctx.userData);
      }
      ctx.references.insert(name);
    } else if (strcmp(ctx.bottom->name(ctx.bottom), "Set") == 0) {
      // make sure we are not Set-ing a Ref
      // meaning target memory could be any block temporary buffer, yet Set will
      // try to deallocate it/manage it
      if (ctx.references.contains(name)) {
        // Error
        std::string err(
            "Set variable name already used as Ref. Overwriting a previously "
            "Ref variable with Set is not allowed, name: " +
            name);
        ctx.cb(ctx.bottom, err.c_str(), false, ctx.userData);
      }
      ctx.variables.insert(name);
    } else if (strcmp(ctx.bottom->name(ctx.bottom), "Update") == 0) {
      // make sure we are not Set-ing a Ref
      // meaning target memory could be any block temporary buffer, yet Set will
      // try to deallocate it/manage it
      if (ctx.references.contains(name)) {
        // Error
        std::string err("Update variable name already used as Ref. Overwriting "
                        "a previously "
                        "Ref variable with Update is not allowed, name: " +
                        name);
        ctx.cb(ctx.bottom, err.c_str(), false, ctx.userData);
      }
    } else if (strcmp(ctx.bottom->name(ctx.bottom), "Push") == 0) {
      // make sure we are not Push-ing a Ref
      // meaning target memory could be any block temporary buffer, yet Push
      // will try to deallocate it/manage it
      if (ctx.references.contains(name)) {
        // Error
        std::string err(
            "Push variable name already used as Ref. Overwriting a previously "
            "Ref variable with Push is not allowed, name: " +
            name);
        ctx.cb(ctx.bottom, err.c_str(), false, ctx.userData);
      }
      ctx.variables.insert(name);
    }
  }

  // Finally do checks on what we consume
  auto consumedVar = ctx.bottom->consumedVariables(ctx.bottom);

  phmap::node_hash_map<std::string, std::vector<CBExposedTypeInfo>>
      consumedVars;
  for (auto i = 0; stbds_arrlen(consumedVar) > i; i++) {
    auto &consumed_param = consumedVar[i];
    std::string name(consumed_param.name);
    consumedVars[name].push_back(consumed_param);
  }

  // make sure we have the vars we need, collect first
  for (const auto &consumed : consumedVars) {
    auto matching = false;

    for (const auto &consumed_param : consumed.second) {
      std::string name(consumed_param.name);
      if (name.find(' ') != -1) { // take only the first part of variable name
        // the remaining should be a table key which we don't care here
        name = name.substr(0, name.find(' '));
      }
      auto findIt = ctx.exposed.find(name);
      if (findIt == ctx.exposed.end()) {
        std::string err("Required consumed variable not found: " + name);
        // Warning only, delegate inferTypes to decide
        ctx.cb(ctx.bottom, err.c_str(), true, ctx.userData);
      } else {

        for (auto type : findIt->second) {
          auto exposedType = type.exposedType;
          auto requiredType = consumed_param.exposedType;
          // Finally deep compare types
          if (matchTypes(exposedType, requiredType, false, true)) {
            matching = true;
            break;
          }
        }
      }
      if (matching)
        break;
    }

    if (!matching) {
      std::string err(
          "Required consumed types do not match currently exposed ones: " +
          consumed.first);
      err += " exposed types:";
      for (auto info : ctx.exposed) {
        err += " (" + info.first + " [";

        for (auto type : info.second) {
          if (type.exposedType.basicType == Table &&
              type.exposedType.tableTypes && type.exposedType.tableKeys) {
            err += "(" + type2Name(type.exposedType.basicType) + " [" +
                   type2Name(type.exposedType.tableTypes[0].basicType) + " " +
                   type.exposedType.tableKeys[0] + "]) ";
          } else if (type.exposedType.basicType == Seq &&
                     type.exposedType.seqType) {
            err += "(" + type2Name(type.exposedType.basicType) + " [" +
                   type2Name(type.exposedType.seqType->basicType) + "]) ";
          } else {
            err += type2Name(type.exposedType.basicType) + " ";
          }
        }
        err.erase(err.end() - 1);

        err += "])";
      }
      ctx.cb(ctx.bottom, err.c_str(), false, ctx.userData);
    }
  }
}

CBValidationResult validateConnections(const std::vector<CBlock *> &chain,
                                       CBValidationCallback callback,
                                       void *userData, CBTypeInfo inputType,
                                       CBExposedTypesInfo consumables) {
  auto ctx = ValidationContext();
  ctx.originalInputType = inputType;
  ctx.previousOutputType = inputType;
  ctx.cb = callback;
  ctx.userData = userData;

  if (consumables) {
    for (auto i = 0; i < stbds_arrlen(consumables); i++) {
      auto &info = consumables[i];
      ctx.exposed[info.name].insert(info);
    }
  }

  for (auto blk : chain) {

    if (strcmp(blk->name(blk), "Input") == 0 ||
        strcmp(blk->name(blk), "And") == 0 ||
        strcmp(blk->name(blk), "Or") == 0) {
      // Hard code behavior for Input block and And and Or, in order to validate
      // with actual chain input the followup
      ctx.previousOutputType = ctx.originalInputType;
    } else {
      ctx.bottom = blk;
      validateConnection(ctx);
    }
  }

  CBValidationResult result = {ctx.previousOutputType};
  for (auto &exposed : ctx.exposed) {
    for (auto &type : exposed.second) {
      stbds_arrpush(result.exposedInfo, type);
    }
  }
  return result;
}

CBValidationResult validateConnections(const CBChain *chain,
                                       CBValidationCallback callback,
                                       void *userData, CBTypeInfo inputType,
                                       CBExposedTypesInfo consumables) {
  return validateConnections(chain->blocks, callback, userData, inputType,
                             consumables);
}

CBValidationResult validateConnections(const CBlocks chain,
                                       CBValidationCallback callback,
                                       void *userData, CBTypeInfo inputType,
                                       CBExposedTypesInfo consumables) {
  std::vector<CBlock *> blocks;
  for (auto i = 0; stbds_arrlen(chain) > i; i++) {
    blocks.push_back(chain[i]);
  }
  return validateConnections(blocks, callback, userData, inputType,
                             consumables);
}

void freeDerivedInfo(CBTypeInfo info) {
  switch (info.basicType) {
  case Seq: {
    freeDerivedInfo(*info.seqType);
    delete info.seqType;
  }
  case Table: {
    for (auto i = 0; stbds_arrlen(info.tableTypes) > i; i++) {
      freeDerivedInfo(info.tableTypes[i]);
    }
    stbds_arrfree(info.tableTypes);
  }
  default:
    break;
  };
}

CBTypeInfo deriveTypeInfo(CBVar &value) {
  // We need to guess a valid CBTypeInfo for this var in order to validate
  // Build a CBTypeInfo for the var
  auto varType = CBTypeInfo();
  varType.basicType = value.valueType;
  varType.seqType = nullptr;
  varType.tableTypes = nullptr;
  switch (value.valueType) {
  case Object: {
    varType.objectVendorId = value.payload.objectVendorId;
    varType.objectTypeId = value.payload.objectTypeId;
    break;
  }
  case Enum: {
    varType.enumVendorId = value.payload.enumVendorId;
    varType.enumTypeId = value.payload.enumTypeId;
    break;
  }
  case Seq: {
    varType.seqType = new CBTypeInfo();
    if (stbds_arrlen(value.payload.seqValue) > 0) {
      *varType.seqType = deriveTypeInfo(value.payload.seqValue[0]);
    }
    break;
  }
  case Table: {
    for (auto i = 0; stbds_arrlen(value.payload.tableValue) > i; i++) {
      stbds_arrpush(varType.tableTypes,
                    deriveTypeInfo(value.payload.tableValue[i].value));
    }
    break;
  }
  default:
    break;
  };
  return varType;
}

bool validateSetParam(CBlock *block, int index, CBVar &value,
                      CBValidationCallback callback, void *userData) {
  auto params = block->parameters(block);
  if (stbds_arrlen(params) <= index) {
    std::string err("Parameter index out of range");
    callback(block, err.c_str(), false, userData);
    return false;
  }

  auto param = params[index];

  // Build a CBTypeInfo for the var
  auto varType = deriveTypeInfo(value);

  for (auto i = 0; stbds_arrlen(param.valueTypes) > i; i++) {
    if (matchTypes(varType, param.valueTypes[i], true, true)) {
      freeDerivedInfo(varType);
      return true; // we are good just exit
    }
  }

  // Failed until now but let's check if the type is a sequenced too
  if (value.valueType == Seq) {
    // Validate each type in the seq
    for (auto i = 0; stbds_arrlen(value.payload.seqValue) > i; i++) {
      if (validateSetParam(block, index, value.payload.seqValue[i], callback,
                           userData)) {
        freeDerivedInfo(varType);
        return true;
      }
    }
  }

  std::string err("Parameter not accepting this kind of variable");
  callback(block, err.c_str(), false, userData);

  freeDerivedInfo(varType);

  return false;
}

void CBChain::cleanup() {
  if (node) {
    node->remove(this);
    node = nullptr;
  }

  for (auto blk : blocks) {
    blk->cleanup(blk);
    blk->destroy(blk);
    // blk is responsible to free itself, as they might use any allocation
    // strategy they wish!
  }
  blocks.clear();

  if (ownedOutput) {
    chainblocks::destroyVar(finishedOutput);
    ownedOutput = false;
  }
}

namespace chainblocks {
void error_handler(int err_sig) {
  std::signal(err_sig, SIG_DFL);

  auto printTrace = false;

  switch (err_sig) {
  case SIGINT:
  case SIGTERM:
    break;
  case SIGFPE:
    LOG(ERROR) << "Fatal SIGFPE";
    printTrace = true;
    break;
  case SIGILL:
    LOG(ERROR) << "Fatal SIGILL";
    printTrace = true;
    break;
  case SIGABRT:
    LOG(ERROR) << "Fatal SIGABRT";
    printTrace = true;
    break;
  case SIGSEGV:
    LOG(ERROR) << "Fatal SIGSEGV";
    printTrace = true;
    break;
  }

  if (printTrace) {
    std::signal(SIGABRT, SIG_DFL); // also make sure to remove this due to
                                   // logger double report on FATAL
    LOG(ERROR) << boost::stacktrace::stacktrace();
  }

  std::raise(err_sig);
}

void installSignalHandlers() {
  std::signal(SIGFPE, &error_handler);
  std::signal(SIGILL, &error_handler);
  std::signal(SIGABRT, &error_handler);
  std::signal(SIGSEGV, &error_handler);
}
}; // namespace chainblocks

#ifdef TESTING
static CBChain mainChain("MainChain");

int main() {
  auto blk = chainblocks::createBlock("SetTableValue");
  LOG(INFO) << blk->name(blk);
  LOG(INFO) << blk->exposedVariables(blk);
}
#endif