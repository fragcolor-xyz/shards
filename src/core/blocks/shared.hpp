#pragma once

#include "../runtime.hpp"
#include "../blocks_macros.hpp"

namespace chainblocks
{
  static CBTypesInfo noneType;
  static CBTypesInfo intSeqInfo;
  static CBTypesInfo boolInfo;
  static CBTypesInfo anyInOutInfo;
  static CBTypesInfo strInfo;
  static CBParametersInfo indicesParamsInfo;
  static CBEnumInfo boolOpEnumInfo = { "BoolOp" };

  static void initEnums()
  {
    // These leak for now
    stbds_arrpush(boolOpEnumInfo.labels, "Equal");
    stbds_arrpush(boolOpEnumInfo.labels, "More");
    stbds_arrpush(boolOpEnumInfo.labels, "Less");
    stbds_arrpush(boolOpEnumInfo.labels, "MoreEqual");
    stbds_arrpush(boolOpEnumInfo.labels, "LessEqual");
    registerEnumType('frag', 'bool', boolOpEnumInfo);
  }
};