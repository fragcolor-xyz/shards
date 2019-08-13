#include "shared.hpp"

namespace chainblocks {
struct ToInt4 {
  CBTypesInfo inputTypes() { return CBTypesInfo(anyInfo); }
  CBTypesInfo outputTypes() { return CBTypesInfo(int4Info); }

  bool convert(CBVar &dst, int &index, const CBVar &src) {
    // TODO overflows
    switch (src.valueType) {
    case String: {
      dst.payload.int4Value[index] =
          static_cast<int32_t>(std::stoi(src.payload.stringValue));
      index++;
      if (index == 4)
        return true;
      break;
    }
    case Float:
      dst.payload.int4Value[index] =
          static_cast<int32_t>(src.payload.floatValue);
      index++;
      if (index == 4)
        return true;
      break;
    case Float2:
      dst.payload.int4Value[index] =
          static_cast<int32_t>(src.payload.float2Value[0]);
      index++;
      if (index == 4)
        return true;
      dst.payload.int4Value[index] =
          static_cast<int32_t>(src.payload.float2Value[1]);
      index++;
      if (index == 4)
        return true;
      break;
    case Float3:
      dst.payload.int4Value[index] =
          static_cast<int32_t>(src.payload.float3Value[0]);
      index++;
      if (index == 4)
        return true;
      dst.payload.int4Value[index] =
          static_cast<int32_t>(src.payload.float3Value[1]);
      index++;
      if (index == 4)
        return true;
      dst.payload.int4Value[index] =
          static_cast<int32_t>(src.payload.float3Value[2]);
      index++;
      if (index == 4)
        return true;
      break;
    case Float4:
      dst.payload.int4Value[index] =
          static_cast<int32_t>(src.payload.float4Value[0]);
      index++;
      if (index == 4)
        return true;
      dst.payload.int4Value[index] =
          static_cast<int32_t>(src.payload.float4Value[1]);
      index++;
      if (index == 4)
        return true;
      dst.payload.int4Value[index] =
          static_cast<int32_t>(src.payload.float4Value[2]);
      index++;
      if (index == 4)
        return true;
      dst.payload.int4Value[index] =
          static_cast<int32_t>(src.payload.float4Value[3]);
      index++;
      if (index == 4)
        return true;
      break;
    case Int:
      dst.payload.int4Value[index] = src.payload.intValue;
      index++;
      if (index == 4)
        return true;
      break;
    case Int2:
      dst.payload.int4Value[index] = src.payload.int2Value[0];
      index++;
      if (index == 4)
        return true;
      dst.payload.int4Value[index] = src.payload.int2Value[1];
      index++;
      if (index == 4)
        return true;
      break;
    case Int3:
      dst.payload.int4Value[index] = src.payload.int3Value[0];
      index++;
      if (index == 4)
        return true;
      dst.payload.int4Value[index] = src.payload.int3Value[1];
      index++;
      if (index == 4)
        return true;
      dst.payload.int4Value[index] = src.payload.int3Value[2];
      index++;
      if (index == 4)
        return true;
      break;
    case Int4:
      dst.payload.int4Value[index] = src.payload.int4Value[0];
      index++;
      if (index == 4)
        return true;
      dst.payload.int4Value[index] = src.payload.int4Value[1];
      index++;
      if (index == 4)
        return true;
      dst.payload.int4Value[index] = src.payload.int4Value[2];
      index++;
      if (index == 4)
        return true;
      dst.payload.int4Value[index] = src.payload.int4Value[3];
      index++;
      if (index == 4)
        return true;
      break;
    }
    return false;
  }

  CBVar activate(CBContext *context, CBVar input) {
    int index = 0;
    CBVar output{};
    output.valueType = Int4;
    switch (input.valueType) {
    case Seq: {
      for (auto i = 0; i < 4, i < stbds_arrlen(input.payload.seqValue); i++) {
        if (convert(output, index, input.payload.seqValue[i]))
          return output;
      }
      break;
    }
    case Int:
    case Int2:
    case Int3:
    case Int4:
    case Float:
    case Float2:
    case Float3:
    case Float4:
    case String:
      if (convert(output, index, input))
        return output;
      break;
    }
    return output;
  }
};

RUNTIME_CORE_BLOCK(ToInt4);
RUNTIME_BLOCK_inputTypes(ToInt4);
RUNTIME_BLOCK_outputTypes(ToInt4);
RUNTIME_BLOCK_activate(ToInt4);
RUNTIME_BLOCK_END(ToInt4);

void registerCastingBlocks() { REGISTER_CORE_BLOCK(ToInt4); }
}; // namespace chainblocks
