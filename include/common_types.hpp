/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

/*
Utility to auto load auto discover blocks from DLLs
All that is needed is to declare a chainblocks::registerBlocks
At runtime just dlopen the dll, that's it!
*/

#ifndef CB_COMMON_TYPES_HPP
#define CB_COMMON_TYPES_HPP

#include "chainblocks.hpp"

namespace chainblocks {

enum class BasicTypes {
  None,
  Any,
  Bool,
  Int,
  Int2,
  Int3,
  Int4,
  Int8,
  Int16,
  Float,
  Float2,
  Float3,
  Float4,
  Color,
  Chain,
  Block,
  Bytes,
  String,
  Image,
  Audio
};

struct CoreInfo {
  static inline Type NoneType{{CBType::None}};

#define CB_CORE_TYPE_DEF(_cbtype_)                                                                       \
  static inline Type _cbtype_##Type{{CBType::_cbtype_}};                                                 \
  static inline Type _cbtype_##SeqType{{CBType::Seq, {.seqTypes = _cbtype_##Type}}};                     \
  static inline Type _cbtype_##TableType{{CBType::Table, {.table = {.types = _cbtype_##Type}}}};         \
  static inline Type _cbtype_##VarType{{CBType::ContextVar, {.contextVarTypes = _cbtype_##Type}}};       \
  static inline Type _cbtype_##VarSeqType{{CBType::ContextVar, {.contextVarTypes = _cbtype_##SeqType}}}; \
  static inline Type _cbtype_##VarTableType {                                                            \
    {                                                                                                    \
      CBType::ContextVar, { .contextVarTypes = _cbtype_##TableType }                                     \
    }                                                                                                    \
  }

  CB_CORE_TYPE_DEF(Any);
  CB_CORE_TYPE_DEF(Bool);
  CB_CORE_TYPE_DEF(Int);
  CB_CORE_TYPE_DEF(Int2);
  CB_CORE_TYPE_DEF(Int3);
  CB_CORE_TYPE_DEF(Int4);
  CB_CORE_TYPE_DEF(Int8);
  CB_CORE_TYPE_DEF(Int16);
  CB_CORE_TYPE_DEF(Float);
  CB_CORE_TYPE_DEF(Float2);
  CB_CORE_TYPE_DEF(Float3);
  CB_CORE_TYPE_DEF(Float4);
  CB_CORE_TYPE_DEF(Color);
  CB_CORE_TYPE_DEF(Chain);
  CB_CORE_TYPE_DEF(Block);
  CB_CORE_TYPE_DEF(Bytes);
  CB_CORE_TYPE_DEF(String);
  CB_CORE_TYPE_DEF(Path);
  CB_CORE_TYPE_DEF(Image);
  CB_CORE_TYPE_DEF(Set);
  CB_CORE_TYPE_DEF(Audio);

  static inline Type AnyEnumType = Type::Enum(0, 0);

  static inline Type Float4x4Type{{CBType::Seq, {.seqTypes = Float4Type}, 4}};
  static inline Type Float4x4SeqType = Type::SeqOf(Float4x4Type);
  static inline Types Float4x4Types{{Float4x4Type, Float4x4SeqType}};
  static inline Type Float3x3Type{{CBType::Seq, {.seqTypes = Float3Type}, 3}};
  static inline Type Float3x3SeqType = Type::SeqOf(Float3x3Type);
  static inline Types Float3x3Types{{Float3x3Type, Float3x3SeqType}};

  static inline Types IntOrFloat{{IntType, FloatType}};

  static inline Types FloatOrFloatSeq{{FloatType, FloatSeqType}};

  static inline Types NoneIntOrFloat{{NoneType, IntType, FloatType}};

  static inline Types Indexables{{Int2Type, Int3Type, Int4Type, Int8Type, Int16Type, Float2Type, Float3Type, Float4Type,
                                  BytesType, ColorType, StringType, AnySeqType, AnyTableType}};

  static inline Types RIndexables{{BytesType, StringType, AnySeqType}};

  static inline Types FloatVectors{{Float2Type, Float2SeqType, Float3Type, Float3SeqType, Float4Type, Float4SeqType}};
  static inline Types FloatSeqs{{FloatSeqType, Float2SeqType, Float3SeqType, Float4SeqType}};
  static inline Types FloatSeqsOrAudio{FloatSeqs, {AudioType}};
  static inline Types FloatVectorsOrFloatSeq{
      {FloatSeqType, Float2Type, Float2SeqType, Float3Type, Float3SeqType, Float4Type, Float4SeqType}};
  static inline Types FloatVectorsOrVar{
      FloatVectors, {Float2VarType, Float2VarSeqType, Float3VarType, Float3VarSeqType, Float4VarType, Float4VarSeqType}};

  static inline Types IntOrNone{{IntType, NoneType}};

  static inline Types IntsVar{{IntType, IntSeqType, IntVarType, IntVarSeqType}};

  static inline Types TakeTypes{
      {IntType, IntSeqType, IntVarType, IntVarSeqType, StringType, StringSeqType, StringVarType, StringVarSeqType}};

  static inline Types RTakeTypes{{IntType, IntSeqType, IntVarType, IntVarSeqType}};

  static inline Types IntsVarOrNone{IntsVar, {NoneType}};

  static inline Types IntIntSeqOrNone{{IntType, IntSeqType, NoneType}};

  static inline Types BlockSeqOrNone{{BlockSeqType, NoneType}};

  static inline Types Blocks{{BlockType, BlockSeqType}};

  static inline Types BlocksOrNone{Blocks, {NoneType}};

  static inline Types ChainOrNone{{ChainType, NoneType}};

  static inline Type BlocksOrNoneSeq{{CBType::Seq, {.seqTypes = BlocksOrNone}}};

  static inline Types StringOrBytes{{StringType, BytesType}};

  static inline Types StringOrNone{{StringType, NoneType}};

  static inline Types StringOrStringVar{{StringType, StringVarType}};

  static inline Types StringOrAnyVar{{StringType, AnyVarType}};

  static inline Types ColorOrNone{{ColorType, NoneType}};

  static inline Types AnyNumbers{{IntType,    IntSeqType,    Int2Type,   Int2SeqType,   Int3Type,   Int3SeqType,
                                  Int4Type,   Int4SeqType,   Int8Type,   Int8SeqType,   Int16Type,  Int16SeqType,
                                  FloatType,  FloatSeqType,  Float2Type, Float2SeqType, Float3Type, Float3SeqType,
                                  Float4Type, Float4SeqType, ColorType,  ColorSeqType}};

  static inline Types StringOrBytesVarOrNone{{StringVarType, BytesVarType, NoneType}};

  static inline Types StringStringVarOrNone{{StringType, StringVarType, NoneType}};

  static inline Types IntOrIntVar{{IntType, IntVarType}};
};
} // namespace chainblocks

#endif