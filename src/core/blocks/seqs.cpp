#include "shared.hpp"

namespace chainblocks {
struct Flatten {
  CBVar outputCache{};
  TypeInfo outputType{};
  TypeInfo seqType{};
  TypesInfo outputFinalType{};

  void destroy() {
    if (outputCache.valueType == Seq) {
      stbds_arrfree(outputCache.payload.seqValue);
    }
  }

  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }
  static CBTypesInfo outputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }

  void verifyInnerType(CBTypeInfo &info, CBTypeInfo &currentType) {
    if (currentType.basicType != None) {
      if (currentType != info) {
        throw CBException("Flatten failed, inner types are not matching.");
      }
    }
    switch (info.basicType) {
    case None:
    case CBType::Any:
    case EndOfBlittableTypes:
      // Nothing
      break;
    case Chain:
    case Block:
    case Object:
    case Enum:
    case String:
    case ContextVar:
    case Image:
    case Int:
    case Float:
    case Bytes:
    case Bool: {
      currentType = info;
      break;
    }
    case Color:
    case Int2:
    case Int3:
    case Int4:
    case Int8:
    case Int16: {
      currentType = CBTypeInfo(SharedTypes::intInfo);
      break;
    }
    case Float2:
    case Float3:
    case Float4: {
      currentType = CBTypeInfo(SharedTypes::floatInfo);
      break;
    }
    case Seq:
      if (info.seqType) {
        verifyInnerType(*info.seqType, currentType);
      }
      break;
    case Table: {
      for (auto i = 0; i < stbds_arrlen(info.tableTypes); i++) {
        verifyInnerType(info.tableTypes[i], currentType);
      }
      break;
    }
    }
  }

  CBTypeInfo inferTypes(CBTypeInfo inputType,
                        CBExposedTypesInfo consumableVariables) {
    CBTypeInfo current{};
    verifyInnerType(inputType, current);
    outputType = TypeInfo(current);
    seqType = TypeInfo::Sequence(outputType);
    outputFinalType = TypesInfo(seqType);
    return CBTypeInfo(outputFinalType);
  }

  void add(const CBVar &input) {
    switch (input.valueType) {
    case None:
    case CBType::Any:
    case EndOfBlittableTypes:
      // Nothing
      break;
    case Chain:
    case Block:
    case Object:
    case Enum:
    case String:
    case ContextVar:
    case Image:
    case Int:
    case Float:
    case Bytes:
    case Bool:
      stbds_arrpush(outputCache.payload.seqValue, input);
      break;
    case Color: {
      stbds_arrpush(outputCache.payload.seqValue,
                    Var(input.payload.colorValue.r));
      stbds_arrpush(outputCache.payload.seqValue,
                    Var(input.payload.colorValue.g));
      stbds_arrpush(outputCache.payload.seqValue,
                    Var(input.payload.colorValue.b));
      stbds_arrpush(outputCache.payload.seqValue,
                    Var(input.payload.colorValue.a));
      break;
    }
    case Int2:
      for (auto i = 0; i < 2; i++)
        stbds_arrpush(outputCache.payload.seqValue,
                      Var(input.payload.int2Value[i]));
      break;
    case Int3:
      for (auto i = 0; i < 3; i++)
        stbds_arrpush(outputCache.payload.seqValue,
                      Var(input.payload.int3Value[i]));
      break;
    case Int4:
      for (auto i = 0; i < 4; i++)
        stbds_arrpush(outputCache.payload.seqValue,
                      Var(input.payload.int4Value[i]));
      break;
    case Int8:
      for (auto i = 0; i < 8; i++)
        stbds_arrpush(outputCache.payload.seqValue,
                      Var(input.payload.int8Value[i]));
      break;
    case Int16:
      for (auto i = 0; i < 16; i++)
        stbds_arrpush(outputCache.payload.seqValue,
                      Var(input.payload.int16Value[i]));
      break;
    case Float2:
      for (auto i = 0; i < 2; i++)
        stbds_arrpush(outputCache.payload.seqValue,
                      Var(input.payload.float2Value[i]));
      break;
    case Float3:
      for (auto i = 0; i < 3; i++)
        stbds_arrpush(outputCache.payload.seqValue,
                      Var(input.payload.float3Value[i]));
      break;
    case Float4:
      for (auto i = 0; i < 4; i++)
        stbds_arrpush(outputCache.payload.seqValue,
                      Var(input.payload.float4Value[i]));
      break;
    case Seq:
      for (auto i = 0; i < stbds_arrlen(input.payload.seqValue); i++) {
        add(input.payload.seqValue[i]);
      }
      break;
    case Table: {
      for (auto i = 0; i < stbds_arrlen(input.payload.tableValue); i++) {
        add(input.payload.tableValue[i].value);
      }
      break;
      break;
    }
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    outputCache.valueType = Seq;
    outputCache.payload.seqLen = -1; // TODO seqLen figure out or leave/remove
    stbds_arrsetlen(outputCache.payload.seqValue,
                    0); // Quick reset no deallocs, slow first run only
    add(input);
    return outputCache;
  }
};

// Register Flatten
RUNTIME_CORE_BLOCK(Flatten);
RUNTIME_BLOCK_destroy(Flatten);
RUNTIME_BLOCK_inputTypes(Flatten);
RUNTIME_BLOCK_outputTypes(Flatten);
RUNTIME_BLOCK_inferTypes(Flatten);
RUNTIME_BLOCK_activate(Flatten);
RUNTIME_BLOCK_END(Flatten);

void registerSeqsBlocks() { REGISTER_CORE_BLOCK(Flatten); }
}; // namespace chainblocks
