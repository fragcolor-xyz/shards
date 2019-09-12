#pragma once

#include "../blocks_macros.hpp"
#include "../runtime.hpp"

namespace chainblocks {
struct SharedTypes {
  static inline TypesInfo &strInfo = CoreInfo::strInfo;
  static inline TypesInfo &anyInfo = CoreInfo::anyInfo;
  static inline TypesInfo &noneInfo = CoreInfo::noneInfo;
  static inline TypesInfo &boolInfo = CoreInfo::boolInfo;
  static inline TypesInfo &floatInfo = CoreInfo::floatInfo;
  static inline TypesInfo chainInfo = TypesInfo(CBType::Chain);
  static inline TypesInfo &intInfo = CoreInfo::intInfo;
  static inline TypesInfo &tableInfo = CoreInfo::tableInfo;
  static inline TypesInfo &blockInfo = CoreInfo::blockInfo;
  static inline TypesInfo &blocksInfo = CoreInfo::blocksInfo;
  static inline TypesInfo &blocksSeqInfo = CoreInfo::blocksSeqInfo;
  static inline TypesInfo &intsInfo = CoreInfo::intsInfo;
  static inline TypesInfo colorInfo = TypesInfo(CBType::Color);
  static inline TypesInfo ctxVarInfo = TypesInfo(CBType::ContextVar);
  static inline TypesInfo int2Info = TypesInfo(CBType::Int2);
  static inline TypesInfo int3Info = TypesInfo(CBType::Int3);
  static inline TypesInfo int4Info = TypesInfo(CBType::Int4);
  static inline TypesInfo float2Info = TypesInfo(CBType::Float2);
  static inline TypesInfo float3Info = TypesInfo(CBType::Float3);
  static inline TypesInfo float4Info = TypesInfo(CBType::Float4);
  static inline TypeInfo strTable =
      TypeInfo::TableRecord(CBTypeInfo(strInfo), "");
  static inline TypesInfo strTableInfo = TypesInfo(CBTypeInfo(strTable));
  static inline TypeInfo intTable =
      TypeInfo::TableRecord(CBTypeInfo(intInfo), "");
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
  static inline CBEnumInfo boolOpEnumInfo = {"BoolOp"};
  static inline CBEnumInfo runChainModeEnumInfo = {"RunChainMode"};
  static inline TypeInfo runChainMode = TypeInfo::Enum('frag', 'runC');
  static inline TypesInfo runChainModeInfo = TypesInfo(runChainMode);
};

static void initEnums() {
  // These leak for now
  stbds_arrpush((SharedTypes::boolOpEnumInfo).labels, "Equal");
  stbds_arrpush((SharedTypes::boolOpEnumInfo).labels, "More");
  stbds_arrpush((SharedTypes::boolOpEnumInfo).labels, "Less");
  stbds_arrpush((SharedTypes::boolOpEnumInfo).labels, "MoreEqual");
  stbds_arrpush((SharedTypes::boolOpEnumInfo).labels, "LessEqual");
  registerEnumType('frag', 'bool', (SharedTypes::boolOpEnumInfo));

  stbds_arrpush((SharedTypes::runChainModeEnumInfo).labels, "Inline");
  stbds_arrpush((SharedTypes::runChainModeEnumInfo).labels, "Detached");
  stbds_arrpush((SharedTypes::runChainModeEnumInfo).labels, "Stepped");
  registerEnumType('frag', 'runC', (SharedTypes::runChainModeEnumInfo));
}
}; // namespace chainblocks