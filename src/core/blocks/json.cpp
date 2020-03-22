/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#include "nlohmann/json.hpp"
#include "shared.hpp"
#include <magic_enum.hpp>

using json = nlohmann::json;

void from_json(const json &j, CBChainRef &chain);
void to_json(json &j, const CBChainRef &chain);

void _releaseMemory(CBVar &var) {
  // Used by Block and Chain from_json
  switch (var.valueType) {
  case Path:
  case ContextVar:
  case String:
    delete[] var.payload.stringValue;
    break;
  case Image:
    delete[] var.payload.imageValue.data;
    break;
  case Bytes:
    delete[] var.payload.bytesValue;
    break;
  case Seq:
    for (uint32_t i = 0; i < var.payload.seqValue.len; i++) {
      _releaseMemory(var.payload.seqValue.elements[i]);
    }
    chainblocks::arrayFree(var.payload.seqValue);
    break;
  case Table: {
    auto map = (chainblocks::CBMap *)var.payload.tableValue.opaque;
    delete map;
  } break;
  case Chain: {
    CBChain::deleteRef(var.payload.chainValue);
  } break;
  default:
    break;
  }
  var = {};
}

void to_json(json &j, const CBVar &var) {
  auto valType = magic_enum::enum_name(var.valueType);
  switch (var.valueType) {
  case Any:
  case Object:
  case Chain: {
    json jchain = var.payload.chainValue;
    j = json{{"type", valType}, {"value", jchain}};
    break;
  }
  case EndOfBlittableTypes: {
    j = json{{"type", 0}, {"value", int(Continue)}};
    break;
  }
  case None: {
    j = json{{"type", 0}, {"value", int(var.payload.chainState)}};
    break;
  }
  case Bool: {
    j = json{{"type", valType}, {"value", var.payload.boolValue}};
    break;
  }
  case StackIndex: {
    j = json{{"type", valType}, {"value", var.payload.stackIndexValue}};
    break;
  }
  case Int: {
    j = json{{"type", valType}, {"value", var.payload.intValue}};
    break;
  }
  case Int2: {
    auto vec = {var.payload.int2Value[0], var.payload.int2Value[1]};
    j = json{{"type", valType}, {"value", vec}};
    break;
  }
  case Int3: {
    auto vec = {var.payload.int3Value[0], var.payload.int3Value[1],
                var.payload.int3Value[2]};
    j = json{{"type", valType}, {"value", vec}};
    break;
  }
  case Int4: {
    auto vec = {var.payload.int4Value[0], var.payload.int4Value[1],
                var.payload.int4Value[2], var.payload.int4Value[3]};
    j = json{{"type", valType}, {"value", vec}};
    break;
  }
  case Int8: {
    auto vec = {var.payload.int8Value[0], var.payload.int8Value[1],
                var.payload.int8Value[2], var.payload.int8Value[3],
                var.payload.int8Value[4], var.payload.int8Value[5],
                var.payload.int8Value[6], var.payload.int8Value[7]};
    j = json{{"type", valType}, {"value", vec}};
    break;
  }
  case Int16: {
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
  case Float: {
    j = json{{"type", valType}, {"value", var.payload.floatValue}};
    break;
  }
  case Float2: {
    auto vec = {var.payload.float2Value[0], var.payload.float2Value[1]};
    j = json{{"type", valType}, {"value", vec}};
    break;
  }
  case Float3: {
    auto vec = {var.payload.float3Value[0], var.payload.float3Value[1],
                var.payload.float3Value[2]};
    j = json{{"type", valType}, {"value", vec}};
    break;
  }
  case Float4: {
    auto vec = {var.payload.float4Value[0], var.payload.float4Value[1],
                var.payload.float4Value[2], var.payload.float4Value[3]};
    j = json{{"type", valType}, {"value", vec}};
    break;
  }
  case Path:
  case ContextVar:
  case String: {
    j = json{{"type", valType}, {"value", var.payload.stringValue}};
    break;
  }
  case Color: {
    j = json{{"type", valType},
             {"value",
              {var.payload.colorValue.r, var.payload.colorValue.g,
               var.payload.colorValue.b, var.payload.colorValue.a}}};
    break;
  }
  case Image: {
    if (var.payload.imageValue.data) {
      auto binsize = var.payload.imageValue.width *
                     var.payload.imageValue.height *
                     var.payload.imageValue.channels;
      std::vector<uint8_t> buffer;
      buffer.resize(binsize);
      memcpy(&buffer[0], var.payload.imageValue.data, binsize);
      j = json{{"type", valType},
               {"width", var.payload.imageValue.width},
               {"height", var.payload.imageValue.height},
               {"channels", var.payload.imageValue.channels},
               {"data", buffer}};
    } else {
      j = json{{"type", 0}, {"value", int(Continue)}};
    }
    break;
  }
  case Bytes: {
    std::vector<uint8_t> buffer;
    buffer.resize(var.payload.bytesSize);
    if (var.payload.bytesSize > 0)
      memcpy(&buffer[0], var.payload.bytesValue, var.payload.bytesSize);
    j = json{{"type", valType}, {"data", buffer}};
    break;
  }
  case Array: {
    std::vector<uint8_t> buffer;
    buffer.resize(var.payload.arrayLen * sizeof(CBVarPayload));
    if (var.payload.arrayLen > 0)
      memcpy(&buffer[0], var.payload.arrayValue,
             var.payload.arrayLen * sizeof(CBVarPayload));
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
  case Seq: {
    std::vector<json> items;
    for (uint32_t i = 0; i < var.payload.seqValue.len; i++) {
      auto &v = var.payload.seqValue.elements[i];
      items.emplace_back(v);
    }
    j = json{{"type", valType}, {"values", items}};
    break;
  }
  case Table: {
    std::vector<json> items;
    auto &ta = var.payload.tableValue;
    ta.api->tableForEach(
        ta,
        [](const char *key, CBVar *value, void *data) {
          auto items = (std::vector<json> *)data;
          json entry{{"key", key}, {"value", *value}};
          items->push_back(entry);
          return true;
        },
        &items);
    j = json{{"type", valType}, {"values", items}};
    break;
  }
  case Block: {
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
  };
}

void from_json(const json &j, CBVar &var) {
  auto valName = j.at("type").get<std::string>();
  auto valType = magic_enum::enum_cast<CBType>(valName);
  if (!valType.has_value()) {
    throw chainblocks::CBException("Failed to parse CBVar value type.");
  }
  switch (valType.value()) {
  case Any:
  case Object:
  case Chain: {
    var.valueType = Chain;
    var.payload.chainValue = j.at("value").get<CBChainRef>();
    break;
  }
  case EndOfBlittableTypes: {
    var = {};
    break;
  }
  case None: {
    var.valueType = None;
    var.payload.chainState = CBChainState(j.at("value").get<int>());
    break;
  }
  case Bool: {
    var.valueType = Bool;
    var.payload.boolValue = j.at("value").get<bool>();
    break;
  }
  case Int: {
    var.valueType = Int;
    var.payload.intValue = j.at("value").get<int64_t>();
    break;
  }
  case StackIndex: {
    var.valueType = StackIndex;
    var.payload.stackIndexValue = j.at("value").get<int64_t>();
    break;
  }
  case Int2: {
    var.valueType = Int2;
    var.payload.int2Value[0] = j.at("value")[0].get<int64_t>();
    var.payload.int2Value[1] = j.at("value")[1].get<int64_t>();
    break;
  }
  case Int3: {
    var.valueType = Int3;
    var.payload.int3Value[0] = j.at("value")[0].get<int32_t>();
    var.payload.int3Value[1] = j.at("value")[1].get<int32_t>();
    var.payload.int3Value[2] = j.at("value")[2].get<int32_t>();
    break;
  }
  case Int4: {
    var.valueType = Int4;
    var.payload.int4Value[0] = j.at("value")[0].get<int32_t>();
    var.payload.int4Value[1] = j.at("value")[1].get<int32_t>();
    var.payload.int4Value[2] = j.at("value")[2].get<int32_t>();
    var.payload.int4Value[3] = j.at("value")[3].get<int32_t>();
    break;
  }
  case Int8: {
    var.valueType = Int8;
    for (auto i = 0; i < 8; i++) {
      var.payload.int8Value[i] = j.at("value")[i].get<int16_t>();
    }
    break;
  }
  case Int16: {
    var.valueType = Int16;
    for (auto i = 0; i < 16; i++) {
      var.payload.int16Value[i] = j.at("value")[i].get<int8_t>();
    }
    break;
  }
  case Float: {
    var.valueType = Float;
    var.payload.floatValue = j.at("value").get<double>();
    break;
  }
  case Float2: {
    var.valueType = Float2;
    var.payload.float2Value[0] = j.at("value")[0].get<double>();
    var.payload.float2Value[1] = j.at("value")[1].get<double>();
    break;
  }
  case Float3: {
    var.valueType = Float3;
    var.payload.float3Value[0] = j.at("value")[0].get<float>();
    var.payload.float3Value[1] = j.at("value")[1].get<float>();
    var.payload.float3Value[2] = j.at("value")[2].get<float>();
    break;
  }
  case Float4: {
    var.valueType = Float4;
    var.payload.float4Value[0] = j.at("value")[0].get<float>();
    var.payload.float4Value[1] = j.at("value")[1].get<float>();
    var.payload.float4Value[2] = j.at("value")[2].get<float>();
    var.payload.float4Value[3] = j.at("value")[3].get<float>();
    break;
  }
  case ContextVar: {
    var.valueType = ContextVar;
    auto strVal = j.at("value").get<std::string>();
    var.payload.stringValue = new char[strVal.length() + 1];
    memcpy((void *)var.payload.stringValue, strVal.c_str(), strVal.length());
    ((char *)var.payload.stringValue)[strVal.length()] = 0;
    break;
  }
  case String: {
    var.valueType = String;
    auto strVal = j.at("value").get<std::string>();
    var.payload.stringValue = new char[strVal.length() + 1];
    memcpy((void *)var.payload.stringValue, strVal.c_str(), strVal.length());
    ((char *)var.payload.stringValue)[strVal.length()] = 0;
    break;
  }
  case Path: {
    var.valueType = Path;
    auto strVal = j.at("value").get<std::string>();
    var.payload.stringValue = new char[strVal.length() + 1];
    memcpy((void *)var.payload.stringValue, strVal.c_str(), strVal.length());
    ((char *)var.payload.stringValue)[strVal.length()] = 0;
    break;
  }
  case Color: {
    var.valueType = Color;
    var.payload.colorValue.r = j.at("value")[0].get<uint8_t>();
    var.payload.colorValue.g = j.at("value")[1].get<uint8_t>();
    var.payload.colorValue.b = j.at("value")[2].get<uint8_t>();
    var.payload.colorValue.a = j.at("value")[3].get<uint8_t>();
    break;
  }
  case Image: {
    var.valueType = Image;
    var.payload.imageValue.width = j.at("width").get<int32_t>();
    var.payload.imageValue.height = j.at("height").get<int32_t>();
    var.payload.imageValue.channels = j.at("channels").get<int32_t>();
    auto binsize = var.payload.imageValue.width *
                   var.payload.imageValue.height *
                   var.payload.imageValue.channels;
    var.payload.imageValue.data = new uint8_t[binsize];
    auto buffer = j.at("data").get<std::vector<uint8_t>>();
    memcpy(var.payload.imageValue.data, &buffer[0], binsize);
    break;
  }
  case Bytes: {
    var.valueType = Bytes;
    auto buffer = j.at("data").get<std::vector<uint8_t>>();
    var.payload.bytesValue = new uint8_t[buffer.size()];
    memcpy(var.payload.bytesValue, &buffer[0], buffer.size());
    break;
  }
  case Array: {
    auto innerName = j.at("inner").get<std::string>();
    auto innerType = magic_enum::enum_cast<CBType>(innerName);
    if (!innerType.has_value()) {
      throw chainblocks::CBException("Failed to parse CBVar inner type.");
    }
    var.valueType = Array;
    var.innerType = innerType.value();
    auto buffer = j.at("data").get<std::vector<uint8_t>>();
    var.payload.arrayValue =
        new CBVarPayload[buffer.size() / sizeof(CBVarPayload)];
    memcpy(var.payload.arrayValue, &buffer[0], buffer.size());
    break;
  }
  case Enum: {
    var.valueType = Enum;
    var.payload.enumValue = CBEnum(j.at("value").get<int32_t>());
    var.payload.enumVendorId = CBEnum(j.at("vendorId").get<int32_t>());
    var.payload.enumTypeId = CBEnum(j.at("typeId").get<int32_t>());
    break;
  }
  case Seq: {
    var.valueType = Seq;
    auto items = j.at("values").get<std::vector<json>>();
    var.payload.seqValue = {};
    for (const auto &item : items) {
      chainblocks::arrayPush(var.payload.seqValue, item.get<CBVar>());
    }
    break;
  }
  case Table: {
    auto map = new chainblocks::CBMap();
    var.valueType = Table;
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
  case Block: {
    var.valueType = Block;
    auto blkname = j.at("name").get<std::string>();
    auto blk = chainblocks::createBlock(blkname.c_str());
    if (!blk) {
      auto errmsg = "Failed to create block of type: " + std::string("blkname");
      throw chainblocks::CBException(errmsg.c_str());
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
            blk->setParam(blk, i, value);
            break;
          }
        }
      }
      // Assume block copied memory internally so we can clean up here!!!
      _releaseMemory(value);
    }

    if (blk->setState) {
      auto state = j.at("state").get<CBVar>();
      blk->setState(blk, state);
      _releaseMemory(state);
    }
    break;
  }
  }
}

void to_json(json &j, const CBChainRef &chainref) {
  auto chain = CBChain::sharedFromRef(chainref);
  std::vector<json> blocks;
  for (auto blk : chain->blocks) {
    std::vector<json> params;
    auto paramsDesc = blk->parameters(blk);
    for (uint32_t i = 0; paramsDesc.len > i; i++) {
      auto &desc = paramsDesc.elements[i];
      auto value = blk->getParam(blk, i);

      json param_obj = {{"name", desc.name}, {"value", value}};

      params.push_back(param_obj);
    }

    json block_obj = {{"name", blk->name(blk)}, {"params", params}};

    blocks.push_back(block_obj);
  }

  j = {
      {"blocks", blocks},        {"name", chain->name},
      {"looped", chain->looped}, {"unsafe", chain->unsafe},
      {"version", 0.1},
  };
}

void from_json(const json &j, CBChainRef &chainref) {
  auto chainName = j.at("name").get<std::string>();
  auto findIt = chainblocks::Globals::GlobalChains.find(chainName);
  std::shared_ptr<CBChain> chain;
  if (findIt != chainblocks::Globals::GlobalChains.end()) {
    chain = findIt->second;
    // Need to clean it up for rewrite!
    chain->clear();
  } else {
    chain.reset(new CBChain(chainName.c_str()));
    chainblocks::Globals::GlobalChains[chainName] = chain;
  }

  chain->looped = j.at("looped").get<bool>();
  chain->unsafe = j.at("unsafe").get<bool>();

  auto jblocks = j.at("blocks");
  for (auto jblock : jblocks) {
    auto blkname = jblock.at("name").get<std::string>();
    auto blk = chainblocks::createBlock(blkname.c_str());
    if (!blk) {
      auto errmsg = "Failed to create block of type: " + std::string(blkname);
      throw chainblocks::CBException(errmsg.c_str());
    }

    // Setup
    blk->setup(blk);

    // Set params
    auto jparams = jblock.at("params");
    auto blkParams = blk->parameters(blk);
    for (auto jparam : jparams) {
      auto paramName = jparam.at("name").get<std::string>();
      auto value = jparam.at("value").get<CBVar>();

      if (value.valueType != None) {
        for (uint32_t i = 0; blkParams.len > i; i++) {
          auto &paramInfo = blkParams.elements[i];
          if (paramName == paramInfo.name) {
            blk->setParam(blk, i, value);
            break;
          }
        }
      }

      // Assume block copied memory internally so we can clean up here!!!
      _releaseMemory(value);
    }

    // From now on this chain owns the block
    chain->addBlock(blk);
    chainref = chain->newRef();
  }
}

namespace chainblocks {
struct ToJson {
  int64_t _indent = 0;
  std::string _output;

  static inline ParamsInfo params = ParamsInfo(ParamsInfo::Param(
      "Indent", "How many spaces to use as json prettify indent.",
      CoreInfo::IntType));

  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  void setParam(int index, CBVar value) { _indent = value.payload.intValue; }

  CBVar getParam(int index) { return Var(_indent); }

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }
  CBVar activate(CBContext *context, const CBVar &input) {
    json j = input;
    if (_indent == 0)
      _output = j.dump();
    else
      _output = j.dump(_indent);
    return Var(_output);
  }
};

struct FromJson {
  CBVar _output;

  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  void cleanup() { _releaseMemory(_output); }

  void anyParse(json &j, CBVar &storage) {
    if (j.is_array()) {
      storage.valueType = Seq;
      for (json::iterator it = j.begin(); it != j.end(); ++it) {
        CBVar tmp{};
        anyParse(*it, tmp);
        arrayPush(storage.payload.seqValue, tmp);
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
      storage.payload.stringValue = new char[strVal.length() + 1];
      memcpy((void *)storage.payload.stringValue, strVal.c_str(),
             strVal.length());
      ((char *)storage.payload.stringValue)[strVal.length()] = 0;
    } else if (j.is_boolean()) {
      storage.valueType = Bool;
      storage.payload.boolValue = j.get<bool>();
    } else if (j.is_object()) {
      storage.valueType = Table;
      auto map = new chainblocks::CBMap();
      storage.payload.tableValue.api = &chainblocks::Globals::TableInterface;
      storage.payload.tableValue.opaque = map;
      for (auto &[key, value] : j.items()) {
        CBVar tmp{};
        anyParse(value, tmp);
        (*map)[key] = tmp;
      }
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    AsyncOp<InternalCore> asyncParse(context);
    // FIXME reading chains and blocks might not be thread safe!
    return asyncParse([&]() {
      _releaseMemory(_output); // release previous
      json j = json::parse(input.payload.stringValue);
      try {
        _output = j.get<CBVar>();
      } catch (json::exception &ex) {
        // Parsing as CBVar failed, try some generic value parsing
        // Filling Seq + Tables
        anyParse(j, _output);
      }
      return _output;
    });
  }
};

void registerJsonBlocks() {
  REGISTER_CBLOCK("FromJson", FromJson);
  REGISTER_CBLOCK("ToJson", ToJson);
}
}; // namespace chainblocks
