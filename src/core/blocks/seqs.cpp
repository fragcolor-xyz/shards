/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "shared.hpp"
#include <unordered_set>

namespace chainblocks {
struct Flatten {
  CBVar outputCache{};
  Type seqType{};
  Types seqTypes{};

  void destroy() {
    if (outputCache.valueType == Seq) {
      chainblocks::arrayFree(outputCache.payload.seqValue);
    }
  }

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  void addInnerType(CBTypeInfo info, std::unordered_set<CBTypeInfo> &types) {
    switch (info.basicType) {
    case None:
    case CBType::Any:
    case EndOfBlittableTypes:
      // Nothing
      break;
    case CBType::Chain:
    case Block:
    case Object:
    case Enum:
    case String:
    case ContextVar:
    case Path:
    case Image:
    case Audio:
    case Int:
    case Float:
    case Bytes:
    case Array:
    case Bool: {
      types.insert(info);
      break;
    }
    case Color:
    case Int2:
    case Int3:
    case Int4:
    case Int8:
    case Int16: {
      types.insert(CoreInfo::IntType);
      break;
    }
    case Float2:
    case Float3:
    case Float4: {
      types.insert(CoreInfo::FloatType);
      break;
    }
    case Seq:
      for (uint32_t i = 0; i < info.seqTypes.len; i++) {
        addInnerType(info.seqTypes.elements[i], types);
      }
      break;
    case CBType::Set:
      for (uint32_t i = 0; i < info.setTypes.len; i++) {
        addInnerType(info.setTypes.elements[i], types);
      }
      break;
    case Table: {
      for (uint32_t i = 0; i < info.table.types.len; i++) {
        addInnerType(info.table.types.elements[i], types);
      }
      break;
    }
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    std::unordered_set<CBTypeInfo> types;
    addInnerType(data.inputType, types);
    std::vector<CBTypeInfo> vtypes(types.begin(), types.end());
    seqTypes = Types(vtypes);
    seqType = Type::SeqOf(seqTypes);
    return seqType;
  }

  void add(const CBVar &input) {
    switch (input.valueType) {
    case None:
    case CBType::Any:
    case EndOfBlittableTypes:
      // Nothing
      break;
    case CBType::Chain:
    case Block:
    case Object:
    case Enum:
    case String:
    case Path:
    case ContextVar:
    case Image:
    case Audio:
    case Int:
    case Float:
    case Bytes:
    case Array:
    case Bool:
      chainblocks::arrayPush(outputCache.payload.seqValue, input);
      break;
    case Color: {
      chainblocks::arrayPush(outputCache.payload.seqValue, Var(input.payload.colorValue.r));
      chainblocks::arrayPush(outputCache.payload.seqValue, Var(input.payload.colorValue.g));
      chainblocks::arrayPush(outputCache.payload.seqValue, Var(input.payload.colorValue.b));
      chainblocks::arrayPush(outputCache.payload.seqValue, Var(input.payload.colorValue.a));
      break;
    }
    case Int2:
      for (auto i = 0; i < 2; i++)
        chainblocks::arrayPush(outputCache.payload.seqValue, Var(input.payload.int2Value[i]));
      break;
    case Int3:
      for (auto i = 0; i < 3; i++)
        chainblocks::arrayPush(outputCache.payload.seqValue, Var(input.payload.int3Value[i]));
      break;
    case Int4:
      for (auto i = 0; i < 4; i++)
        chainblocks::arrayPush(outputCache.payload.seqValue, Var(input.payload.int4Value[i]));
      break;
    case Int8:
      for (auto i = 0; i < 8; i++)
        chainblocks::arrayPush(outputCache.payload.seqValue, Var(input.payload.int8Value[i]));
      break;
    case Int16:
      for (auto i = 0; i < 16; i++)
        chainblocks::arrayPush(outputCache.payload.seqValue, Var(input.payload.int16Value[i]));
      break;
    case Float2:
      for (auto i = 0; i < 2; i++)
        chainblocks::arrayPush(outputCache.payload.seqValue, Var(input.payload.float2Value[i]));
      break;
    case Float3:
      for (auto i = 0; i < 3; i++)
        chainblocks::arrayPush(outputCache.payload.seqValue, Var(input.payload.float3Value[i]));
      break;
    case Float4:
      for (auto i = 0; i < 4; i++)
        chainblocks::arrayPush(outputCache.payload.seqValue, Var(input.payload.float4Value[i]));
      break;
    case Seq:
      for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {
        add(input.payload.seqValue.elements[i]);
      }
      break;
    case Table: {
      auto &t = input.payload.tableValue;
      CBTableIterator tit;
      t.api->tableGetIterator(t, &tit);
      CBString k;
      CBVar v;
      while (t.api->tableNext(t, &tit, &k, &v)) {
        add(Var(k));
        add(v);
      }
    } break;
    case CBType::Set: {
      auto &s = input.payload.setValue;
      CBSetIterator sit;
      s.api->setGetIterator(s, &sit);
      CBVar v;
      while (s.api->setNext(s, &sit, &v)) {
        add(v);
      }
    } break;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    outputCache.valueType = Seq;
    // Quick reset no deallocs, slow first run only
    chainblocks::arrayResize(outputCache.payload.seqValue, 0);
    add(input);
    return outputCache;
  }
};

struct IndexOf {
  ParamVar _item{};
  CBSeq _results = {};
  bool _all = false;

  void destroy() {
    if (_results.elements) {
      chainblocks::arrayFree(_results);
    }
  }

  void cleanup() { _item.cleanup(); }
  void warmup(CBContext *context) { _item.warmup(context); }

  static CBTypesInfo inputTypes() { return CoreInfo::AnySeqType; }
  CBTypesInfo outputTypes() {
    if (_all)
      return CoreInfo::IntSeqType;
    else
      return CoreInfo::IntType;
  }

  static inline ParamsInfo params = ParamsInfo(ParamsInfo::Param("Item",
                                                                 CBCCSTR("The item to find the index of from the input, "
                                                                         "if it's a sequence it will try to match all "
                                                                         "the items in the sequence, in sequence."),
                                                                 CoreInfo::AnyType),
                                               ParamsInfo::Param("All",
                                                                 CBCCSTR("If true will return a sequence with all the indices of "
                                                                         "Item, empty sequence if not found."),
                                                                 CoreInfo::BoolType));

  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  void setParam(int index, const CBVar &value) {
    if (index == 0)
      _item = value;
    else
      _all = value.payload.boolValue;
  }

  CBVar getParam(int index) {
    if (index == 0)
      return _item;
    else
      return Var(_all);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto inputLen = input.payload.seqValue.len;
    auto itemLen = 0;
    auto item = _item.get();
    chainblocks::arrayResize(_results, 0);
    if (item.valueType == Seq) {
      itemLen = item.payload.seqValue.len;
    }

    for (uint32_t i = 0; i < inputLen; i++) {
      if (item.valueType == Seq) {
        if ((i + itemLen) > inputLen) {
          if (!_all)
            return Var(-1); // we are likely at the end
          else
            return Var(_results);
        }

        auto ci = i;
        for (auto y = 0; y < itemLen; y++, ci++) {
          if (item.payload.seqValue.elements[y] != input.payload.seqValue.elements[ci]) {
            goto failed;
          }
        }

        if (!_all)
          return Var((int64_t)i);
        else
          chainblocks::arrayPush(_results, Var((int64_t)i));

      failed:
        continue;
      } else if (input.payload.seqValue.elements[i] == item) {
        if (!_all)
          return Var((int64_t)i);
        else
          chainblocks::arrayPush(_results, Var((int64_t)i));
      }
    }

    if (!_all)
      return Var(-1);
    else
      return Var(_results);
  }
};

void registerSeqsBlocks() {
  REGISTER_CBLOCK("Flatten", Flatten);
  REGISTER_CBLOCK("IndexOf", IndexOf);
}
}; // namespace chainblocks
