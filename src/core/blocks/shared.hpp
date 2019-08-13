#pragma once

#ifndef CHAINBLOCKS_RUNTIME
#define CHAINBLOCKS_RUNTIME 1
#endif

#include "../blocks_macros.hpp"
#include "../runtime.hpp"

namespace chainblocks {
static TypesInfo strInfo = TypesInfo(CBType::String);
static TypesInfo anyInfo = TypesInfo(CBType::Any);
static TypesInfo noneInfo = TypesInfo(CBType::None);
static TypesInfo boolInfo = TypesInfo(CBType::Bool);
static TypesInfo floatInfo = TypesInfo(CBType::Float);
static TypesInfo chainInfo = TypesInfo(CBType::Chain);
static TypesInfo intInfo = TypesInfo(CBType::Int);
static TypesInfo blockInfo = TypesInfo(CBType::Block);
static TypesInfo blocksInfo = TypesInfo(CBType::Block, true);
static TypesInfo blockSeqInfo = TypesInfo(CBType::Seq, CBTypesInfo(blocksInfo));
static TypesInfo intSeqInfo = TypesInfo(CBType::Seq, CBTypesInfo(intInfo));
static TypesInfo int2Info = TypesInfo(CBType::Int2);
static TypesInfo int3Info = TypesInfo(CBType::Int3);
static TypesInfo int4Info = TypesInfo(CBType::Int4);
static TypesInfo strTableInfo = TypesInfo(CBType::Table, CBTypesInfo(strInfo));
static TypesInfo intTableInfo = TypesInfo(CBType::Table, CBTypesInfo(intInfo));

static CBEnumInfo boolOpEnumInfo = {"BoolOp"};

static void initEnums() {
  // These leak for now
  stbds_arrpush(boolOpEnumInfo.labels, "Equal");
  stbds_arrpush(boolOpEnumInfo.labels, "More");
  stbds_arrpush(boolOpEnumInfo.labels, "Less");
  stbds_arrpush(boolOpEnumInfo.labels, "MoreEqual");
  stbds_arrpush(boolOpEnumInfo.labels, "LessEqual");
  registerEnumType('frag', 'bool', boolOpEnumInfo);
}
}; // namespace chainblocks