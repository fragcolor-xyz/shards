/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#pragma once

#include "../blocks_macros.hpp"
#include "../runtime.hpp"

namespace chainblocks {
struct SharedTypes {
  static inline TypeInfo strType = TypeInfo(CBType::String);
  static inline TypesInfo &strInfo = CoreInfo::strInfo;
  static inline TypeInfo strSeq = TypeInfo::Sequence(strType);
  static inline TypesInfo strSeqInfo = TypesInfo(strSeq);
  static inline TypesInfo &anyInfo = CoreInfo::anyInfo;
  static inline TypesInfo &noneInfo = CoreInfo::noneInfo;
  static inline TypesInfo &boolInfo = CoreInfo::boolInfo;
  static inline TypesInfo &floatInfo = CoreInfo::floatInfo;
  static inline TypesInfo chainInfo = TypesInfo(CBType::Chain);
  static inline TypesInfo &bytesInfo = CoreInfo::bytesInfo;
  static inline TypesInfo &intInfo = CoreInfo::intInfo;
  static inline TypesInfo &tableInfo = CoreInfo::tableInfo;
  static inline TypesInfo &blockInfo = CoreInfo::blockInfo;
  static inline TypesInfo &blocksInfo = CoreInfo::blocksInfo;
  static inline TypesInfo &blocksSeqInfo = CoreInfo::blocksSeqInfo;
  static inline TypesInfo &intsInfo = CoreInfo::intsInfo;
  static inline TypesInfo colorInfo = TypesInfo(CBType::Color);
  static inline TypesInfo imageInfo = TypesInfo(CBType::Image);
  static inline TypesInfo ctxVarInfo = TypesInfo(CBType::ContextVar);
  static inline TypesInfo int2Info = TypesInfo(CBType::Int2);
  static inline TypesInfo int3Info = TypesInfo(CBType::Int3);
  static inline TypesInfo int4Info = TypesInfo(CBType::Int4);
  static inline TypesInfo &float2Info = CoreInfo::float2Info;
  static inline TypesInfo &float3Info = CoreInfo::float3Info;
  static inline TypesInfo &float4Info = CoreInfo::float4Info;
  static inline TypeInfo float2Type = TypeInfo(CBType::Float2);
  static inline TypeInfo float3Type = TypeInfo(CBType::Float3);
  static inline TypeInfo float4Type = TypeInfo(CBType::Float4);
  static inline TypeInfo strTable =
      TypeInfo::SingleTypeTable(CBTypeInfo(strInfo));
  static inline TypesInfo strTableInfo = TypesInfo(CBTypeInfo(strTable));
  static inline TypesInfo strOrBytesInfo =
      TypesInfo::FromMany(false, CBTypeInfo((strInfo)), CBTypeInfo(bytesInfo));
  static inline TypesInfo intOrFloatInfo =
      TypesInfo::FromMany(false, CBType::Int, CBType::Float);
  static inline TypeInfo intTable =
      TypeInfo::SingleTypeTable(CBTypeInfo(intInfo));
  static inline TypesInfo intTableInfo = TypesInfo(CBTypeInfo(intTable));
  static inline TypesInfo blocksOrNoneInfo = TypesInfo::FromMany(
      false, CBTypeInfo((blocksInfo)), CBTypeInfo(noneInfo));
  static inline TypesInfo ctxOrNoneInfo = TypesInfo::FromMany(
      false, CBTypeInfo((ctxVarInfo)), CBTypeInfo(noneInfo));
  static inline TypesInfo blockSeqsOrNoneInfo =
      TypesInfo::FromMany(false, CBTypeInfo((SharedTypes::blocksSeqInfo)),
                          CBTypeInfo(SharedTypes::noneInfo));
  static inline TypesInfo intOrNoneInfo =
      TypesInfo::FromMany(false, CBTypeInfo((intInfo)), CBTypeInfo(noneInfo));
  static inline TypesInfo strOrNoneInfo =
      TypesInfo::FromMany(false, CBTypeInfo((strInfo)), CBTypeInfo(noneInfo));
  static inline TypesInfo colorOrNoneInfo =
      TypesInfo::FromMany(false, CBTypeInfo((colorInfo)), CBTypeInfo(noneInfo));
  static inline CBEnumInfo runChainModeEnumInfo = {"RunChainMode"};
  static inline TypeInfo runChainMode = TypeInfo::Enum('frag', 'runC');
  static inline TypesInfo runChainModeInfo = TypesInfo(runChainMode);
  static inline TypesInfo floatsInfo =
      TypesInfo::FromMany(true, CBTypeInfo(floatInfo));
  static inline TypesInfo vectorsInfo =
      TypesInfo::FromMany(true, float2Type, float3Type, float4Type);
  static inline TypesInfo vectorsCtxInfo = TypesInfo::FromMany(
      true, float2Type, float3Type, float4Type, CBTypeInfo(ctxVarInfo));
  static inline TypeInfo float4Seq = TypeInfo::Sequence(float4Type);
  static inline TypesInfo matrix4x4Info = TypesInfo(float4Seq);

  static void initEnums() {
    // These leak for now
    stbds_arrpush((SharedTypes::runChainModeEnumInfo).labels, "Inline");
    stbds_arrpush((SharedTypes::runChainModeEnumInfo).labels, "Detached");
    stbds_arrpush((SharedTypes::runChainModeEnumInfo).labels, "Stepped");
    registerEnumType('frag', 'runC', (SharedTypes::runChainModeEnumInfo));
  }
};
}; // namespace chainblocks
