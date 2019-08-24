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
  static inline TypesInfo &intSeqInfo = CoreInfo::intSeqInfo;
  static inline TypesInfo int2Info = TypesInfo(CBType::Int2);
  static inline TypesInfo int3Info = TypesInfo(CBType::Int3);
  static inline TypesInfo int4Info = TypesInfo(CBType::Int4);
  static inline TypesInfo float2Info = TypesInfo(CBType::Float2);
  static inline TypesInfo float3Info = TypesInfo(CBType::Float3);
  static inline TypesInfo float4Info = TypesInfo(CBType::Float4);
  static inline TypesInfo strTableInfo =
      TypesInfo(CBType::Table, CBTypesInfo(strInfo));
  static inline TypesInfo intTableInfo =
      TypesInfo(CBType::Table, CBTypesInfo(intInfo));
  static inline TypesInfo blocksOrNoneInfo =
      TypesInfo::FromManyTypes(CBTypeInfo((blocksInfo)), CBTypeInfo(noneInfo));
  static inline TypesInfo blockSeqsOrNoneInfo =
      TypesInfo::FromManyTypes(CBTypeInfo((SharedTypes::blocksSeqInfo)),
                               CBTypeInfo(SharedTypes::noneInfo));
  static inline TypesInfo intOrNoneInfo =
      TypesInfo::FromManyTypes(CBTypeInfo((intInfo)), CBTypeInfo(noneInfo));
  static inline CBEnumInfo boolOpEnumInfo = {"BoolOp"};
};

static void initEnums() {
  // These leak for now
  stbds_arrpush((SharedTypes::boolOpEnumInfo).labels, "Equal");
  stbds_arrpush((SharedTypes::boolOpEnumInfo).labels, "More");
  stbds_arrpush((SharedTypes::boolOpEnumInfo).labels, "Less");
  stbds_arrpush((SharedTypes::boolOpEnumInfo).labels, "MoreEqual");
  stbds_arrpush((SharedTypes::boolOpEnumInfo).labels, "LessEqual");
  registerEnumType('frag', 'bool', (SharedTypes::boolOpEnumInfo));
}
}; // namespace chainblocks