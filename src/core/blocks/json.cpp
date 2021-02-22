/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2021 Giovanni Petrantoni */

#include "nlohmann/json.hpp"
#include "chainblocks.h"
#include "shared.hpp"
#include <magic_enum.hpp>

using json = nlohmann::json;

void from_json(const json &j, CBChainRef &chain);
void to_json(json &j, const CBChainRef &chain);

void _releaseMemory(CBVar &var) {
  // Used by Block and Chain from_json
  switch (var.valueType) {
  case CBType::Path:
  case CBType::ContextVar:
  case CBType::String:
    delete[] var.payload.stringValue;
    break;
  case CBType::Image:
    delete[] var.payload.imageValue.data;
    break;
  case CBType::Bytes:
    delete[] var.payload.bytesValue;
    break;
  case CBType::Seq:
    for (uint32_t i = 0; i < var.payload.seqValue.len; i++) {
      _releaseMemory(var.payload.seqValue.elements[i]);
    }
    chainblocks::arrayFree(var.payload.seqValue);
    break;
  case CBType::Table: {
    auto map = (chainblocks::CBMap *)var.payload.tableValue.opaque;
    delete map;
  } break;
  case CBType::Set: {
    auto set = (chainblocks::CBHashSet *)var.payload.setValue.opaque;
    delete set;
  } break;
  case CBType::Chain: {
    CBChain::deleteRef(var.payload.chainValue);
  } break;
  case CBType::Object: {
    if ((var.flags & CBVAR_FLAGS_USES_OBJINFO) == CBVAR_FLAGS_USES_OBJINFO &&
        var.objectInfo && var.objectInfo->release) {
      var.objectInfo->release(var.payload.objectValue);
    }
  } break;
  default:
    break;
  }
  var = {};
}

void to_json(json &j, const CBVar &var) {
  auto valType = magic_enum::enum_name(var.valueType);
  switch (var.valueType) {
  case CBType::Any:
  case CBType::EndOfBlittableTypes:
  case CBType::None: {
    j = json{{"type", valType}};
    break;
  }
  case CBType::Bool: {
    j = json{{"type", valType}, {"value", var.payload.boolValue}};
    break;
  }
  case CBType::Int: {
    j = json{{"type", valType}, {"value", var.payload.intValue}};
    break;
  }
  case CBType::Int2: {
    auto vec = {var.payload.int2Value[0], var.payload.int2Value[1]};
    j = json{{"type", valType}, {"value", vec}};
    break;
  }
  case CBType::Int3: {
    auto vec = {var.payload.int3Value[0], var.payload.int3Value[1],
                var.payload.int3Value[2]};
    j = json{{"type", valType}, {"value", vec}};
    break;
  }
  case CBType::Int4: {
    auto vec = {var.payload.int4Value[0], var.payload.int4Value[1],
                var.payload.int4Value[2], var.payload.int4Value[3]};
    j = json{{"type", valType}, {"value", vec}};
    break;
  }
  case CBType::Int8: {
    auto vec = {var.payload.int8Value[0], var.payload.int8Value[1],
                var.payload.int8Value[2], var.payload.int8Value[3],
                var.payload.int8Value[4], var.payload.int8Value[5],
                var.payload.int8Value[6], var.payload.int8Value[7]};
    j = json{{"type", valType}, {"value", vec}};
    break;
  }
  case CBType::Int16: {
    auto vec = {var.payload.int16Value[0],  var.payload.int16Value[1],
                var.payload.int16Value[2],  var.payload.int16Value[3],
                var.payload.int16Value[4],  var.payload.int16Value[5],
                var.payload.int16Value[6],  var.payload.int16Value[7],
                var.payload.int16Value[8],  var.payload.int16Value[9],
                var.payload.int16Value[10], var.payload.int16Value[11],
                var.payload.int16Value[12], var.payload.int16Value[13],
                var.payload.int16Value[14], var.payload.int16Value[15]};
    j = json{{"type", valType}, {"value", vec}};
    break;
  }
  case CBType::Float: {
    j = json{{"type", valType}, {"value", var.payload.floatValue}};
    break;
  }
  case CBType::Float2: {
    auto vec = {var.payload.float2Value[0], var.payload.float2Value[1]};
    j = json{{"type", valType}, {"value", vec}};
    break;
  }
  case CBType::Float3: {
    auto vec = {var.payload.float3Value[0], var.payload.float3Value[1],
                var.payload.float3Value[2]};
    j = json{{"type", valType}, {"value", vec}};
    break;
  }
  case CBType::Float4: {
    auto vec = {var.payload.float4Value[0], var.payload.float4Value[1],
                var.payload.float4Value[2], var.payload.float4Value[3]};
    j = json{{"type", valType}, {"value", vec}};
    break;
  }
  case CBType::Path:
  case CBType::ContextVar:
  case CBType::String: {
    j = json{{"type", valType}, {"value", var.payload.stringValue}};
    break;
  }
  case CBType::Color: {
    j = json{{"type", valType},
             {"value",
              {var.payload.colorValue.r, var.payload.colorValue.g,
               var.payload.colorValue.b, var.payload.colorValue.a}}};
    break;
  }
  case CBType::Image: {
    if (var.payload.imageValue.data) {
      auto pixsize = 1;
      if ((var.payload.imageValue.flags & CBIMAGE_FLAGS_16BITS_INT) ==
          CBIMAGE_FLAGS_16BITS_INT)
        pixsize = 2;
      else if ((var.payload.imageValue.flags & CBIMAGE_FLAGS_32BITS_FLOAT) ==
               CBIMAGE_FLAGS_32BITS_FLOAT)
        pixsize = 4;
      auto binsize = var.payload.imageValue.width *
                     var.payload.imageValue.height *
                     var.payload.imageValue.channels * pixsize;
      std::vector<uint8_t> buffer;
      buffer.resize(binsize);
      memcpy(&buffer[0], var.payload.imageValue.data, binsize);
      j = json{{"type", valType},
               {"width", var.payload.imageValue.width},
               {"height", var.payload.imageValue.height},
               {"channels", var.payload.imageValue.channels},
               {"flags", var.payload.imageValue.flags},
               {"data", buffer}};
    } else {
      j = json{{"type", 0}, {"value", int(Continue)}};
    }
    break;
  }
  case CBType::Bytes: {
    std::vector<uint8_t> buffer;
    buffer.resize(var.payload.bytesSize);
    if (var.payload.bytesSize > 0)
      memcpy(&buffer[0], var.payload.bytesValue, var.payload.bytesSize);
    j = json{{"type", valType}, {"data", buffer}};
    break;
  }
  case CBType::Array: {
    std::vector<uint8_t> buffer;
    buffer.resize(var.payload.arrayValue.len * sizeof(CBVarPayload));
    if (var.payload.arrayValue.len > 0)
      memcpy(&buffer[0], &var.payload.arrayValue.elements[0],
             var.payload.arrayValue.len * sizeof(CBVarPayload));
    j = json{{"type", valType},
             {"inner", magic_enum::enum_name(var.innerType)},
             {"data", buffer}};
    break;
  }
  case Enum: {
    j = json{{"type", valType},
             {"value", int32_t(var.payload.enumValue)},
             {"vendorId", var.payload.enumVendorId},
             {"typeId", var.payload.enumTypeId}};
    break;
  }
  case CBType::Seq: {
    std::vector<json> items;
    for (uint32_t i = 0; i < var.payload.seqValue.len; i++) {
      auto &v = var.payload.seqValue.elements[i];
      items.emplace_back(v);
    }
    j = json{{"type", valType}, {"values", items}};
    break;
  }
  case CBType::Table: {
    std::vector<json> items;
    auto &t = var.payload.tableValue;
    CBTableIterator tit;
    t.api->tableGetIterator(t, &tit);
    CBString k;
    CBVar v;
    while (t.api->tableNext(t, &tit, &k, &v)) {
      json entry{{"key", k}, {"value", v}};
      items.emplace_back(entry);
    }
    j = json{{"type", valType}, {"values", items}};
    break;
  }
  case CBType::Set: {
    std::vector<json> items;
    auto &s = var.payload.setValue;
    CBSetIterator sit;
    s.api->setGetIterator(s, &sit);
    CBVar v;
    while (s.api->setNext(s, &sit, &v)) {
      items.emplace_back(v);
    }
    j = json{{"type", valType}, {"values", items}};
    break;
  }
  case CBType::Block: {
    auto blk = var.payload.blockValue;
    std::vector<json> params;
    auto paramsDesc = blk->parameters(blk);
    for (uint32_t i = 0; i < paramsDesc.len; i++) {
      auto &desc = paramsDesc.elements[i];
      auto value = blk->getParam(blk, i);
      json param_obj = {{"name", desc.name}, {"value", value}};
      params.push_back(param_obj);
    }
    if (blk->getState) {
      j = json{{"type", valType},
               {"name", blk->name(blk)},
               {"params", params},
               {"state", blk->getState(blk)}};
    } else {
      j = json{{"type", valType}, {"name", blk->name(blk)}, {"params", params}};
    }
    break;
  }
  case CBType::Chain: {
    json jchain = var.payload.chainValue;
    j = json{{"type", valType}, {"value", jchain}};
    break;
  }
  case CBType::Object: {
    j = json{{"type", valType},
             {"vendorId", var.payload.objectVendorId},
             {"typeId", var.payload.objectTypeId}};
    if ((var.flags & CBVAR_FLAGS_USES_OBJINFO) == CBVAR_FLAGS_USES_OBJINFO &&
        var.objectInfo && var.objectInfo->serialize) {
      size_t len = 0;
      uint8_t *data = nullptr;
      CBPointer handle = nullptr;
      if (!var.objectInfo->serialize(var.payload.objectValue, &data, &len,
                                     &handle)) {
        throw chainblocks::CBException(
            "Failed to serialize custom object variable!");
      }
      std::vector<uint8_t> buf;
      buf.resize(len);
      memcpy(&buf.front(), data, len);
      j.emplace("data", buf);
      var.objectInfo->free(handle);
    } else {
      throw chainblocks::ActivationError("Custom object cannot be serialized.");
    }
    break;
  }
  };
}

void from_json(const json &j, CBVar &var) {
  auto valName = j.at("type").get<std::string>();
  auto valType = magic_enum::enum_cast<CBType>(valName);
  if (!valType.has_value()) {
    throw chainblocks::ActivationError("Failed to parse CBVar value type.");
  }
  switch (valType.value()) {
  case CBType::Any:
  case CBType::EndOfBlittableTypes:
  case CBType::None: {
    var = {};
    var.valueType = valType.value();
    break;
  }
  case CBType::Bool: {
    var.valueType = CBType::Bool;
    var.payload.boolValue = j.at("value").get<bool>();
    break;
  }
  case CBType::Int: {
    var.valueType = CBType::Int;
    var.payload.intValue = j.at("value").get<int64_t>();
    break;
  }
  case CBType::Int2: {
    var.valueType = CBType::Int2;
    var.payload.int2Value[0] = j.at("value")[0].get<int64_t>();
    var.payload.int2Value[1] = j.at("value")[1].get<int64_t>();
    break;
  }
  case CBType::Int3: {
    var.valueType = CBType::Int3;
    var.payload.int3Value[0] = j.at("value")[0].get<int32_t>();
    var.payload.int3Value[1] = j.at("value")[1].get<int32_t>();
    var.payload.int3Value[2] = j.at("value")[2].get<int32_t>();
    break;
  }
  case CBType::Int4: {
    var.valueType = CBType::Int4;
    var.payload.int4Value[0] = j.at("value")[0].get<int32_t>();
    var.payload.int4Value[1] = j.at("value")[1].get<int32_t>();
    var.payload.int4Value[2] = j.at("value")[2].get<int32_t>();
    var.payload.int4Value[3] = j.at("value")[3].get<int32_t>();
    break;
  }
  case CBType::Int8: {
    var.valueType = CBType::Int8;
    for (auto i = 0; i < 8; i++) {
      var.payload.int8Value[i] = j.at("value")[i].get<int16_t>();
    }
    break;
  }
  case CBType::Int16: {
    var.valueType = CBType::Int16;
    for (auto i = 0; i < 16; i++) {
      var.payload.int16Value[i] = j.at("value")[i].get<int8_t>();
    }
    break;
  }
  case CBType::Float: {
    var.valueType = CBType::Float;
    var.payload.floatValue = j.at("value").get<double>();
    break;
  }
  case CBType::Float2: {
    var.valueType = CBType::Float2;
    var.payload.float2Value[0] = j.at("value")[0].get<double>();
    var.payload.float2Value[1] = j.at("value")[1].get<double>();
    break;
  }
  case CBType::Float3: {
    var.valueType = CBType::Float3;
    var.payload.float3Value[0] = j.at("value")[0].get<float>();
    var.payload.float3Value[1] = j.at("value")[1].get<float>();
    var.payload.float3Value[2] = j.at("value")[2].get<float>();
    break;
  }
  case CBType::Float4: {
    var.valueType = CBType::Float4;
    var.payload.float4Value[0] = j.at("value")[0].get<float>();
    var.payload.float4Value[1] = j.at("value")[1].get<float>();
    var.payload.float4Value[2] = j.at("value")[2].get<float>();
    var.payload.float4Value[3] = j.at("value")[3].get<float>();
    break;
  }
  case CBType::ContextVar: {
    var.valueType = CBType::ContextVar;
    auto strVal = j.at("value").get<std::string>();
    const auto strLen = strVal.length();
    var.payload.stringValue = new char[strLen + 1];
    var.payload.stringLen = uint32_t(strLen);
    memcpy((void *)var.payload.stringValue, strVal.c_str(), strLen);
    ((char *)var.payload.stringValue)[strLen] = 0;
    break;
  }
  case CBType::String: {
    var.valueType = CBType::String;
    auto strVal = j.at("value").get<std::string>();
    const auto strLen = strVal.length();
    var.payload.stringValue = new char[strLen + 1];
    var.payload.stringLen = uint32_t(strLen);
    memcpy((void *)var.payload.stringValue, strVal.c_str(), strLen);
    ((char *)var.payload.stringValue)[strLen] = 0;
    break;
  }
  case CBType::Path: {
    var.valueType = CBType::Path;
    auto strVal = j.at("value").get<std::string>();
    const auto strLen = strVal.length();
    var.payload.stringValue = new char[strLen + 1];
    var.payload.stringLen = uint32_t(strLen);
    memcpy((void *)var.payload.stringValue, strVal.c_str(), strLen);
    ((char *)var.payload.stringValue)[strLen] = 0;
    break;
  }
  case CBType::Color: {
    var.valueType = CBType::Color;
    var.payload.colorValue.r = j.at("value")[0].get<uint8_t>();
    var.payload.colorValue.g = j.at("value")[1].get<uint8_t>();
    var.payload.colorValue.b = j.at("value")[2].get<uint8_t>();
    var.payload.colorValue.a = j.at("value")[3].get<uint8_t>();
    break;
  }
  case CBType::Image: {
    var.valueType = CBType::Image;
    var.payload.imageValue.width = j.at("width").get<int32_t>();
    var.payload.imageValue.height = j.at("height").get<int32_t>();
    var.payload.imageValue.channels = j.at("channels").get<int32_t>();
    var.payload.imageValue.flags = j.at("flags").get<int32_t>();
    auto pixsize = 1;
    if ((var.payload.imageValue.flags & CBIMAGE_FLAGS_16BITS_INT) ==
        CBIMAGE_FLAGS_16BITS_INT)
      pixsize = 2;
    else if ((var.payload.imageValue.flags & CBIMAGE_FLAGS_32BITS_FLOAT) ==
             CBIMAGE_FLAGS_32BITS_FLOAT)
      pixsize = 4;
    auto binsize = var.payload.imageValue.width *
                   var.payload.imageValue.height *
                   var.payload.imageValue.channels * pixsize;
    var.payload.imageValue.data = new uint8_t[binsize];
    auto buffer = j.at("data").get<std::vector<uint8_t>>();
    memcpy(var.payload.imageValue.data, &buffer[0], binsize);
    break;
  }
  case CBType::Bytes: {
    var.valueType = CBType::Bytes;
    auto buffer = j.at("data").get<std::vector<uint8_t>>();
    var.payload.bytesValue = new uint8_t[buffer.size()];
    memcpy(var.payload.bytesValue, &buffer[0], buffer.size());
    break;
  }
  case CBType::Array: {
    auto innerName = j.at("inner").get<std::string>();
    auto innerType = magic_enum::enum_cast<CBType>(innerName);
    if (!innerType.has_value()) {
      throw chainblocks::ActivationError("Failed to parse CBVar inner type.");
    }
    var.valueType = CBType::Array;
    var.innerType = innerType.value();
    auto buffer = j.at("data").get<std::vector<uint8_t>>();
    auto len = buffer.size() / sizeof(CBVarPayload);
    chainblocks::arrayResize(var.payload.arrayValue, len);
    memcpy(&var.payload.arrayValue.elements[0], &buffer[0], buffer.size());
    break;
  }
  case CBType::Enum: {
    var.valueType = CBType::Enum;
    var.payload.enumValue = CBEnum(j.at("value").get<int32_t>());
    var.payload.enumVendorId = CBEnum(j.at("vendorId").get<int32_t>());
    var.payload.enumTypeId = CBEnum(j.at("typeId").get<int32_t>());
    break;
  }
  case CBType::Seq: {
    var.valueType = CBType::Seq;
    auto items = j.at("values").get<std::vector<json>>();
    var.payload.seqValue = {};
    for (const auto &item : items) {
      chainblocks::arrayPush(var.payload.seqValue, item.get<CBVar>());
    }
    break;
  }
  case CBType::Table: {
    auto map = new chainblocks::CBMap();
    var.valueType = CBType::Table;
    var.payload.tableValue.api = &chainblocks::Globals::TableInterface;
    var.payload.tableValue.opaque = map;
    auto items = j.at("values").get<std::vector<json>>();
    for (const auto &item : items) {
      auto key = item.at("key").get<std::string>();
      auto value = item.at("value").get<CBVar>();
      (*map)[key] = value;
    }
    break;
  }
  case CBType::Set: {
    auto set = new chainblocks::CBHashSet();
    var.valueType = CBType::Set;
    var.payload.setValue.api = &chainblocks::Globals::SetInterface;
    var.payload.setValue.opaque = set;
    auto items = j.at("values").get<std::vector<json>>();
    for (const auto &item : items) {
      set->emplace(item.get<CBVar>());
    }
    break;
  }
  case CBType::Block: {
    var.valueType = CBType::Block;
    auto blkname = j.at("name").get<std::string>();
    auto blk = chainblocks::createBlock(blkname.c_str());
    if (!blk) {
      auto errmsg = "Failed to create block of type: " + std::string("blkname");
      throw chainblocks::ActivationError(errmsg.c_str());
    }
    var.payload.blockValue = blk;

    // Setup
    blk->setup(blk);

    // Set params
    auto jparams = j.at("params");
    auto blkParams = blk->parameters(blk);
    for (auto jparam : jparams) {
      auto paramName = jparam.at("name").get<std::string>();
      auto value = jparam.at("value").get<CBVar>();
      if (value.valueType != None) {
        for (uint32_t i = 0; blkParams.len > i; i++) {
          auto &paramInfo = blkParams.elements[i];
          if (paramName == paramInfo.name) {
            blk->setParam(blk, i, &value);
            break;
          }
        }
      }
      // Assume block copied memory internally so we can clean up here!!!
      _releaseMemory(value);
    }

    if (blk->setState) {
      auto state = j.at("state").get<CBVar>();
      blk->setState(blk, &state);
      _releaseMemory(state);
    }
    break;
  }
  case CBType::Chain: {
    var.valueType = CBType::Chain;
    var.payload.chainValue = j.at("value").get<CBChainRef>();
    break;
  }
  case CBType::Object: {
    var.valueType = CBType::Object;
    var.payload.objectVendorId = CBEnum(j.at("vendorId").get<int32_t>());
    var.payload.objectTypeId = CBEnum(j.at("typeId").get<int32_t>());
    var.payload.objectValue = nullptr;
    int64_t id =
        (int64_t)var.payload.objectVendorId << 32 | var.payload.objectTypeId;
    auto it = chainblocks::Globals::ObjectTypesRegister.find(id);
    if (it != chainblocks::Globals::ObjectTypesRegister.end()) {
      auto &info = it->second;
      auto data = j.at("data").get<std::vector<uint8_t>>();
      var.payload.objectValue = info.deserialize(&data.front(), data.size());
      var.flags |= CBVAR_FLAGS_USES_OBJINFO;
      var.objectInfo = &info;
      if (info.reference)
        info.reference(var.payload.objectValue);
    } else {
      throw chainblocks::ActivationError(
          "Failed to find object type in registry.");
    }
    break;
  }
  }
}

void to_json(json &j, const CBChainRef &chainref) {
  auto chain = CBChain::sharedFromRef(chainref);
  std::vector<json> blocks;
  for (auto blk : chain->blocks) {
    CBVar blkVar{};
    blkVar.valueType = CBType::Block;
    blkVar.payload.blockValue = blk;
    blocks.emplace_back(blkVar);
  }
  j = {
      {"blocks", blocks},
      {"name", chain->name},
      {"looped", chain->looped},
      {"unsafe", chain->unsafe},
      {"version", CHAINBLOCKS_CURRENT_ABI},
  };
}

void from_json(const json &j, CBChainRef &chainref) {
  auto chainName = j.at("name").get<std::string>();
  auto chain = CBChain::make(chainName);

  chain->looped = j.at("looped").get<bool>();
  chain->unsafe = j.at("unsafe").get<bool>();

  auto blks = j.at("blocks").get<std::vector<CBVar>>();
  for (auto blk : blks) {
    chain->addBlock(blk.payload.blockValue);
    chainref = chain->newRef();
  }
}

namespace chainblocks {
struct ToJson {
  std::string _output;
  int64_t _indent = 0;
  bool _pure{true};

  static CBParametersInfo parameters() {
    static Parameters params{
        {"Pure",
         CBCCSTR("If the input string is generic pure json rather then "
                 "chainblocks flavored json."),
         {CoreInfo::BoolType}},
        {"Indent",
         CBCCSTR("How many spaces to use as json prettify indent."),
         {CoreInfo::IntType}}};
    return params;
  }

  void setParam(int index, const CBVar &value) {
    if (index == 0)
      _pure = value.payload.boolValue;
    else
      _indent = value.payload.intValue;
  }

  CBVar getParam(int index) {
    if (index == 0)
      return Var(_pure);
    else
      return Var(_indent);
  }

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }

  void anyDump(json &j, const CBVar &input) {
    switch (input.valueType) {
    case Table: {
      std::unordered_map<std::string, json> table;
      auto &tab = input.payload.tableValue;
      ForEach(tab, [&](auto key, auto &val) {
        json sj{};
        anyDump(sj, val);
        table.emplace(key, sj);
        return true;
      });
      j = table;
    } break;
    case Seq: {
      std::vector<json> array;
      auto &seq = input.payload.seqValue;
      for (uint32_t i = 0; i < seq.len; i++) {
        json js{};
        anyDump(js, seq.elements[i]);
        array.emplace_back(js);
      }
      j = array;
    } break;
    case String: {
      j = input.payload.stringValue;
    } break;
    case Int: {
      j = input.payload.intValue;
    } break;
    case Float: {
      j = input.payload.floatValue;
    } break;
    case Bool: {
      j = input.payload.boolValue;
    } break;
    case None: {
      j = nullptr;
    } break;
    default: {
      LOG(ERROR) << "Unexpected type for pure JSON conversion: "
                 << type2Name(input.valueType);
      throw ActivationError("Type not supported for pure JSON conversion");
    }
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (!_pure) {
      json j = input;
      if (_indent == 0)
        _output = j.dump();
      else
        _output = j.dump(_indent);
    } else {
      json j{};
      anyDump(j, input);
      if (_indent == 0)
        _output = j.dump();
      else
        _output = j.dump(_indent);
    }
    return Var(_output);
  }
};

struct FromJson {
  CBVar _output{};
  bool _pure{true};

  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static CBParametersInfo parameters() {
    static Parameters params{
        {"Pure",
         CBCCSTR("If the input string is generic pure json rather then "
                 "chainblocks flavored json."),
         {CoreInfo::BoolType}}};
    return params;
  }

  void setParam(int index, const CBVar &value) {
    _pure = value.payload.boolValue;
  }

  CBVar getParam(int index) { return Var(_pure); }

  void cleanup() { _releaseMemory(_output); }

  void anyParse(json &j, CBVar &storage) {
    if (j.is_array()) {
      storage.valueType = Seq;
      for (json::iterator it = j.begin(); it != j.end(); ++it) {
        const auto len = storage.payload.seqValue.len;
        arrayResize(storage.payload.seqValue, len + 1);
        anyParse(*it, storage.payload.seqValue.elements[len]);
      }
    } else if (j.is_number_integer()) {
      storage.valueType = Int;
      storage.payload.intValue = j.get<int64_t>();
    } else if (j.is_number_float()) {
      storage.valueType = Float;
      storage.payload.floatValue = j.get<double>();
    } else if (j.is_string()) {
      storage.valueType = String;
      auto strVal = j.get<std::string>();
      const auto strLen = strVal.length();
      storage.payload.stringValue = new char[strLen + 1];
      storage.payload.stringLen = strLen;
      memcpy((void *)storage.payload.stringValue, strVal.c_str(), strLen);
      ((char *)storage.payload.stringValue)[strLen] = 0;
    } else if (j.is_boolean()) {
      storage.valueType = Bool;
      storage.payload.boolValue = j.get<bool>();
    } else if (j.is_object()) {
      storage.valueType = Table;
      auto map = new chainblocks::CBMap();
      storage.payload.tableValue.api = &chainblocks::Globals::TableInterface;
      storage.payload.tableValue.opaque = map;
      for (auto &[key, value] : j.items()) {
        anyParse(value, (*map)[key]);
      }
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    _releaseMemory(_output); // release previous

    try {
      json j = json::parse(input.payload.stringValue);

      if (_pure) {
        anyParse(j, _output);
      } else {
        _output = j.get<CBVar>();
      }
    } catch (const json::exception &ex) {
      // re-throw with our type to allow Maybe etc
      throw ActivationError(ex.what());
    }

    return _output;
  }
};

void registerJsonBlocks() {
  REGISTER_CBLOCK("FromJson", FromJson);
  REGISTER_CBLOCK("ToJson", ToJson);
}
}; // namespace chainblocks
