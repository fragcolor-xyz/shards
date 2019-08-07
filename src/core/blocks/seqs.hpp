#pragma once

#include "shared.hpp"

namespace chainblocks {
static TypesInfo indicesTypes =
    TypesInfo::FromManyTypes(CBTypeInfo(intInfo), CBTypeInfo(intSeqInfo));
static ParamsInfo indicesParamsInfo = ParamsInfo(ParamsInfo::Param(
    "Indices", "One or multiple indices to filter from a sequence.",
    CBTypesInfo(indicesTypes)));

struct GetItems {
  CBSeq cachedResult;
  CBVar indices;

  void destroy() {
    if (cachedResult) {
      stbds_arrfree(cachedResult);
    }
  }

  CBTypesInfo inputTypes() { return CBTypesInfo(anyInfo); }

  CBTypesInfo outputTypes() { return CBTypesInfo(anyInfo); }

  CBParametersInfo parameters() { return CBParametersInfo(indicesParamsInfo); }

  CBTypeInfo inferTypes(CBTypeInfo inputType,
                        CBExposedTypesInfo consumableVariables) {
    // Figure if we output a sequence or not
    if (indices.valueType == Seq &&
        (stbds_arrlen(indices.payload.seqValue) > 1))
      inputType.sequenced = true;
    else
      inputType.sequenced = false;
    return inputType;
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      indices = value;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) { return indices; }

  CBVar activate(CBContext *context, CBVar input) {
    if (input.valueType != Seq) {
      throw CBException("GetItems expected a sequence!");
    }

    auto inputLen = stbds_arrlen(input.payload.seqValue);

    if (indices.valueType == Int) {
      auto index = indices.payload.intValue;
      if (index >= inputLen) {
        LOG(ERROR) << "GetItems out of range! len:" << inputLen
                   << " wanted index: " << index;
        throw CBException("GetItems out of range!");
      }

      return input.payload.seqValue[index];
    }

    // Else it's a seq
    auto nindices = stbds_arrlen(indices.payload.seqValue);
    stbds_arrsetlen(cachedResult, nindices);
    for (auto i = 0; i < nindices; i++) {
      auto index = indices.payload.seqValue[i].payload.intValue;
      if (index >= inputLen) {
        LOG(ERROR) << "GetItems out of range! len:" << inputLen
                   << " wanted index: " << index;
        throw CBException("GetItems out of range!");
      }
      cachedResult[i] = input.payload.seqValue[index];
    }

    CBVar result;
    result.valueType = Seq;
    result.payload.seqLen = -1;
    result.payload.seqValue = cachedResult;
    return result;
  }
};
}; // namespace chainblocks

// Register GetItems
RUNTIME_CORE_BLOCK(chainblocks::GetItems, GetItems)
RUNTIME_BLOCK_destroy(GetItems) RUNTIME_BLOCK_inputTypes(GetItems)
    RUNTIME_BLOCK_outputTypes(GetItems) RUNTIME_BLOCK_parameters(GetItems)
        RUNTIME_BLOCK_inferTypes(GetItems) RUNTIME_BLOCK_setParam(GetItems)
            RUNTIME_BLOCK_getParam(GetItems) RUNTIME_BLOCK_activate(GetItems)
                RUNTIME_CORE_BLOCK_END(GetItems)
