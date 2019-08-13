#pragma once

#ifndef CHAINBLOCKS_RUNTIME
#define CHAINBLOCKS_RUNTIME 1
#endif

#include "../blocks_macros.hpp"
#include "../runtime.hpp"

namespace chainblocks {
struct SharedTypes {
  static inline TypesInfo strInfo = TypesInfo(CBType::String);
  static inline TypesInfo anyInfo = TypesInfo(CBType::Any);
  static inline TypesInfo noneInfo = TypesInfo(CBType::None);
  static inline TypesInfo boolInfo = TypesInfo(CBType::Bool);
  static inline TypesInfo floatInfo = TypesInfo(CBType::Float);
  static inline TypesInfo chainInfo = TypesInfo(CBType::Chain);
  static inline TypesInfo intInfo = TypesInfo(CBType::Int);
  static inline TypesInfo blockInfo = TypesInfo(CBType::Block);
  static inline TypesInfo blocksInfo = TypesInfo(CBType::Block, true);
  static inline TypesInfo blockSeqInfo =
      TypesInfo(CBType::Seq, CBTypesInfo(blocksInfo));
  static inline TypesInfo intSeqInfo =
      TypesInfo(CBType::Seq, CBTypesInfo(intInfo));
  static inline TypesInfo int2Info = TypesInfo(CBType::Int2);
  static inline TypesInfo int3Info = TypesInfo(CBType::Int3);
  static inline TypesInfo int4Info = TypesInfo(CBType::Int4);
  static inline TypesInfo strTableInfo =
      TypesInfo(CBType::Table, CBTypesInfo(strInfo));
  static inline TypesInfo intTableInfo =
      TypesInfo(CBType::Table, CBTypesInfo(intInfo));
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