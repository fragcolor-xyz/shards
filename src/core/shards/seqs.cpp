/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "shared.hpp"
#include <unordered_set>

namespace shards {
struct Flatten {
  SHVar outputCache{};
  Type seqType{};
  Types seqTypes{};

  void destroy() {
    if (outputCache.valueType == Seq) {
      shards::arrayFree(outputCache.payload.seqValue);
    }
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  void addInnerType(SHTypeInfo info, std::unordered_set<SHTypeInfo> &types) {
    switch (info.basicType) {
    case None:
    case SHType::Any:
    case EndOfBlittableTypes:
    case SHType::Error: // this is not a valid type we want to serialize...
      // Nothing
      break;
    case SHType::Wire:
    case ShardRef:
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
    case SHType::Set:
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

  SHTypeInfo compose(const SHInstanceData &data) {
    std::unordered_set<SHTypeInfo> types;
    addInnerType(data.inputType, types);
    std::vector<SHTypeInfo> vtypes(types.begin(), types.end());
    seqTypes = Types(vtypes);
    seqType = Type::SeqOf(seqTypes);
    return seqType;
  }

  void add(const SHVar &input) {
    switch (input.valueType) {
    case None:
    case SHType::Any:
    case EndOfBlittableTypes:
    case SHType::Error: // this is not a valid type we want to serialize...
      // Nothing
      break;
    case SHType::Wire:
    case ShardRef:
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
      shards::arrayPush(outputCache.payload.seqValue, input);
      break;
    case Color: {
      shards::arrayPush(outputCache.payload.seqValue, Var(input.payload.colorValue.r));
      shards::arrayPush(outputCache.payload.seqValue, Var(input.payload.colorValue.g));
      shards::arrayPush(outputCache.payload.seqValue, Var(input.payload.colorValue.b));
      shards::arrayPush(outputCache.payload.seqValue, Var(input.payload.colorValue.a));
      break;
    }
    case Int2:
      for (auto i = 0; i < 2; i++)
        shards::arrayPush(outputCache.payload.seqValue, Var(input.payload.int2Value[i]));
      break;
    case Int3:
      for (auto i = 0; i < 3; i++)
        shards::arrayPush(outputCache.payload.seqValue, Var(input.payload.int3Value[i]));
      break;
    case Int4:
      for (auto i = 0; i < 4; i++)
        shards::arrayPush(outputCache.payload.seqValue, Var(input.payload.int4Value[i]));
      break;
    case Int8:
      for (auto i = 0; i < 8; i++)
        shards::arrayPush(outputCache.payload.seqValue, Var(input.payload.int8Value[i]));
      break;
    case Int16:
      for (auto i = 0; i < 16; i++)
        shards::arrayPush(outputCache.payload.seqValue, Var(input.payload.int16Value[i]));
      break;
    case Float2:
      for (auto i = 0; i < 2; i++)
        shards::arrayPush(outputCache.payload.seqValue, Var(input.payload.float2Value[i]));
      break;
    case Float3:
      for (auto i = 0; i < 3; i++)
        shards::arrayPush(outputCache.payload.seqValue, Var(input.payload.float3Value[i]));
      break;
    case Float4:
      for (auto i = 0; i < 4; i++)
        shards::arrayPush(outputCache.payload.seqValue, Var(input.payload.float4Value[i]));
      break;
    case Seq:
      for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {
        add(input.payload.seqValue.elements[i]);
      }
      break;
    case Table: {
      auto &t = input.payload.tableValue;
      SHTableIterator tit;
      t.api->tableGetIterator(t, &tit);
      SHString k;
      SHVar v;
      while (t.api->tableNext(t, &tit, &k, &v)) {
        add(Var(k));
        add(v);
      }
    } break;
    case SHType::Set: {
      auto &s = input.payload.setValue;
      SHSetIterator sit;
      s.api->setGetIterator(s, &sit);
      SHVar v;
      while (s.api->setNext(s, &sit, &v)) {
        add(v);
      }
    } break;
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    outputCache.valueType = Seq;
    // Quick reset no deallocs, slow first run only
    shards::arrayResize(outputCache.payload.seqValue, 0);
    add(input);
    return outputCache;
  }
};

struct IndexOf {
  ParamVar _item{};
  SHSeq _results = {};
  bool _all = false;

  void destroy() {
    if (_results.elements) {
      shards::arrayFree(_results);
    }
  }

  void cleanup() { _item.cleanup(); }
  void warmup(SHContext *context) { _item.warmup(context); }

  static SHTypesInfo inputTypes() { return CoreInfo::AnySeqType; }
  SHTypesInfo outputTypes() {
    if (_all)
      return CoreInfo::IntSeqType;
    else
      return CoreInfo::IntType;
  }

  static inline ParamsInfo params = ParamsInfo(ParamsInfo::Param("Item",
                                                                 SHCCSTR("The item to find the index of from the input, "
                                                                         "if it's a sequence it will try to match all "
                                                                         "the items in the sequence, in sequence."),
                                                                 CoreInfo::AnyType),
                                               ParamsInfo::Param("All",
                                                                 SHCCSTR("If true will return a sequence with all the indices of "
                                                                         "Item, empty sequence if not found."),
                                                                 CoreInfo::BoolType));

  static SHParametersInfo parameters() { return SHParametersInfo(params); }

  void setParam(int index, const SHVar &value) {
    if (index == 0)
      _item = value;
    else
      _all = value.payload.boolValue;
  }

  SHVar getParam(int index) {
    if (index == 0)
      return _item;
    else
      return Var(_all);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto inputLen = input.payload.seqValue.len;
    auto itemLen = 0;
    auto item = _item.get();
    shards::arrayResize(_results, 0);
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
          shards::arrayPush(_results, Var((int64_t)i));

      failed:
        continue;
      } else if (input.payload.seqValue.elements[i] == item) {
        if (!_all)
          return Var((int64_t)i);
        else
          shards::arrayPush(_results, Var((int64_t)i));
      }
    }

    if (!_all)
      return Var(-1);
    else
      return Var(_results);
  }
};

void registerSeqsShards() {
  REGISTER_SHARD("Flatten", Flatten);
  REGISTER_SHARD("IndexOf", IndexOf);
}
}; // namespace shards
