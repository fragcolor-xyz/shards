#include "shared.hpp"
#include <string>

namespace chainblocks {
struct Base {
  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }
  static CBTypesInfo outputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }
};

struct Append : public Base {
  static inline ParamsInfo paramsInfo = ParamsInfo(ParamsInfo::Param(
      "File", "The file to append.", CBTypesInfo(SharedTypes::strInfo)));

  std::ofstream _outFile;
  std::string _fileName;

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  void cleanup() {
    if (_outFile.good()) {
      _outFile.flush();
    }
  }

  void setParam(int index, CBVar inValue) {
    switch (index) {
    case 0:
      _fileName = inValue.payload.stringValue;
      _outFile = std::ofstream(_fileName, std::ios::app | std::ios::binary);
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    auto res = CBVar();
    switch (index) {
    case 0:
      return Var(_fileName);
    default:
      break;
    }
    return res;
  }

  void writeVar(const CBVar &input) {
    _outFile.write((const char *)&input.valueType, sizeof(input.valueType));
    switch (input.valueType) {
    case CBType::None:
    case CBType::EndOfBlittableTypes:
    case CBType::Any:
    case CBType::Enum:
    case CBType::Bool:
    case CBType::Int:
    case CBType::Int2:
    case CBType::Int3:
    case CBType::Int4:
    case CBType::Int8:
    case CBType::Int16:
    case CBType::Float:
    case CBType::Float2:
    case CBType::Float3:
    case CBType::Float4:
    case CBType::Color:
      _outFile.write((const char *)&input.payload, sizeof(input.payload));
      break;
    case CBType::Bytes:
      _outFile.write((const char *)&input.payload.bytesSize, sizeof(uint64_t));
      _outFile.write((const char *)input.payload.bytesValue,
                     input.payload.bytesSize);
      break;
    case CBType::String:
    case CBType::ContextVar: {
      uint64_t len = strlen(input.payload.stringValue);
      _outFile.write((const char *)&len, sizeof(uint64_t));
      _outFile.write(input.payload.stringValue, len);
      break;
    }
    case CBType::Seq: {
      uint64_t len = stbds_arrlen(input.payload.seqValue);
      _outFile.write((const char *)&len, sizeof(uint64_t));
      for (auto i = 0; i < len; i++) {
        writeVar(input.payload.seqValue[i]);
      }
      break;
    }
    case CBType::Table: {
      uint64_t len = stbds_shlen(input.payload.tableValue);
      _outFile.write((const char *)&len, sizeof(uint64_t));
      for (auto i = 0; i < len; i++) {
        auto &v = input.payload.tableValue[i];
        auto klen = strlen(v.key);
        _outFile.write(v.key, len);
        writeVar(v.value);
      }
      break;
    }
    case CBType::Image: {
      _outFile.write((const char *)&input.payload.imageValue.channels,
                     sizeof(input.payload.imageValue.channels));
      _outFile.write((const char *)&input.payload.imageValue.flags,
                     sizeof(input.payload.imageValue.flags));
      _outFile.write((const char *)&input.payload.imageValue.width,
                     sizeof(input.payload.imageValue.width));
      _outFile.write((const char *)&input.payload.imageValue.height,
                     sizeof(input.payload.imageValue.height));
      auto size = input.payload.imageValue.channels *
                  input.payload.imageValue.height *
                  input.payload.imageValue.width;
      _outFile.write((const char *)input.payload.imageValue.data, size);
      break;
    }
    case CBType::Object:
    case CBType::Chain:
    case CBType::Block:
      throw CBException("Append: Type cannot be serialized (yet?). " +
                        type2Name(input.valueType));
    }
    _outFile.write((const char *)input.reserved, sizeof(input.reserved));
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    writeVar(input);
    return input;
  }
};

// Register Log
RUNTIME_CORE_BLOCK(Append);
RUNTIME_BLOCK_inputTypes(Append);
RUNTIME_BLOCK_outputTypes(Append);
RUNTIME_BLOCK_cleanup(Append);
RUNTIME_BLOCK_parameters(Append);
RUNTIME_BLOCK_setParam(Append);
RUNTIME_BLOCK_getParam(Append);
RUNTIME_BLOCK_activate(Append);
RUNTIME_BLOCK_END(Append);

void registerSerializationBlocks() { REGISTER_CORE_BLOCK(Append); }
}; // namespace chainblocks
