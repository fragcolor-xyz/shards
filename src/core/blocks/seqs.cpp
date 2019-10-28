/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

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
      for (auto i = 0; i < stbds_shlen(input.payload.tableValue); i++) {
        add(input.payload.tableValue[i].value);
      }
      break;
    }
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    outputCache.valueType = Seq;
    // Quick reset no deallocs, slow first run only
    stbds_arrsetlen(outputCache.payload.seqValue, 0);
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

struct IndexOf {
  ContextableVar _item{};
  CBSeq _results = nullptr;
  bool _all = false;

  void destroy() {
    if (_results) {
      stbds_arrfree(_results);
    }
  }

  static CBTypesInfo inputTypes() { return CBTypesInfo(CoreInfo::anySeqInfo); }
  CBTypesInfo outputTypes() {
    if (_all)
      return CBTypesInfo(CoreInfo::intSeqInfo);
    else
      return CBTypesInfo(CoreInfo::intInfo);
  }

  static inline ParamsInfo params = ParamsInfo(
      ParamsInfo::Param(
          "Item",
          "The item to find the index of from the input, if it's a sequence it "
          "will try to match all the items in the sequence, in sequence.",
          CBTypesInfo(CoreInfo::anyInfo)),
      ParamsInfo::Param("All",
                        "If true will return a sequence with all the indices "
                        "of Item, empty sequence if not found.",
                        CBTypesInfo(CoreInfo::boolInfo)));

  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  void setParam(int index, CBVar value) {
    if (index == 0)
      _item.setParam(value);
    else
      _all = value.payload.boolValue;
  }

  CBVar getParam(int index) {
    if (index == 0)
      return _item.getParam();
    else
      return Var(_all);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto inputLen = stbds_arrlen(input.payload.seqValue);
    auto itemLen = 0;
    auto item = _item.get(context);
    stbds_arrsetlen(_results, 0);
    if (item.valueType == Seq) {
      itemLen = stbds_arrlen(item.payload.seqValue);
    }

    for (auto i = 0; i < inputLen; i++) {
      if (item.valueType == Seq) {
        if ((i + itemLen) > inputLen) {
          if (!_all)
            return Var(-1); // we are likely at the end
          else
            return Var(_results);
        }

        auto ci = i;
        for (auto y = 0; y < itemLen; y++, ci++) {
          if (item.payload.seqValue[y] != input.payload.seqValue[ci]) {
            goto failed;
          }
        }

        if (!_all)
          return Var(i);
        else
          stbds_arrpush(_results, Var(i));

      failed:
        continue;
      } else if (input.payload.seqValue[i] == item) {
        if (!_all)
          return Var(i);
        else
          stbds_arrpush(_results, Var(i));
      }
    }

    if (!_all)
      return Var(-1);
    else
      return Var(_results);
  }
};

// Register
RUNTIME_CORE_BLOCK(IndexOf);
RUNTIME_BLOCK_inputTypes(IndexOf);
RUNTIME_BLOCK_outputTypes(IndexOf);
RUNTIME_BLOCK_parameters(IndexOf);
RUNTIME_BLOCK_setParam(IndexOf);
RUNTIME_BLOCK_getParam(IndexOf);
RUNTIME_BLOCK_activate(IndexOf);
RUNTIME_BLOCK_END(IndexOf);

void registerSeqsBlocks() {
  REGISTER_CORE_BLOCK(Flatten);
  REGISTER_CORE_BLOCK(IndexOf);
}
}; // namespace chainblocks
