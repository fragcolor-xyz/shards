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

  CBVar activate(CBContext *context, const CBVar &input) {
    _outFile.write((const char *)&input.valueType, sizeof(input.valueType));
    if (input.valueType <= EndOfBlittableTypes) {
      _outFile.write((const char *)&input.payload, sizeof(input.payload));
    }
    _outFile.write((const char *)input.reserved, sizeof(input.reserved));
    return input;
  }
};

// Register Log
RUNTIME_CORE_BLOCK(Append);
RUNTIME_BLOCK_inputTypes(Append);
RUNTIME_BLOCK_outputTypes(Append);
RUNTIME_BLOCK_activate(Append);
RUNTIME_BLOCK_END(Append);

void registerSerializationBlocks() { REGISTER_CORE_BLOCK(Append); }
}; // namespace chainblocks
