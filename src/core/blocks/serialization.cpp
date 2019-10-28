/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#include "shared.hpp"
#include <string>

namespace chainblocks {
struct FileBase {
  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }
  static CBTypesInfo outputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }

  static inline ParamsInfo paramsInfo =
      ParamsInfo(ParamsInfo::Param("File", "The file to read/write from.",
                                   CBTypesInfo(SharedTypes::strInfo)));

  std::string _fileName;

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  void setParam(int index, CBVar inValue) {
    switch (index) {
    case 0:
      _fileName = inValue.payload.stringValue;
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
};

struct WriteFile : public FileBase {
  std::ofstream _fileStream;

  void cleanup() {
    if (_fileStream.good()) {
      _fileStream.flush();
    }
  }

  void setParam(int index, CBVar inValue) {
    switch (index) {
    case 0:
      FileBase::setParam(index, inValue);
      _fileStream = std::ofstream(_fileName, std::ios::app | std::ios::binary);
      break;
    default:
      break;
    }
  }

  struct Writer {
    std::ofstream &_fileStream;
    Writer(std::ofstream &stream) : _fileStream(stream) {}
    void operator()(const uint8_t *buf, size_t size) {
      _fileStream.write((const char *)buf, size);
    }
  };

  CBVar activate(CBContext *context, const CBVar &input) {
    Writer s(_fileStream);
    Serialization::serialize(input, s);
    return input;
  }
};

// Register
RUNTIME_CORE_BLOCK(WriteFile);
RUNTIME_BLOCK_inputTypes(WriteFile);
RUNTIME_BLOCK_outputTypes(WriteFile);
RUNTIME_BLOCK_cleanup(WriteFile);
RUNTIME_BLOCK_parameters(WriteFile);
RUNTIME_BLOCK_setParam(WriteFile);
RUNTIME_BLOCK_getParam(WriteFile);
RUNTIME_BLOCK_activate(WriteFile);
RUNTIME_BLOCK_END(WriteFile);

struct ReadFile : public FileBase {
  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::noneInfo); }

  std::ifstream _fileStream;
  CBVar _output{};

  void setParam(int index, CBVar inValue) {
    switch (index) {
    case 0:
      FileBase::setParam(index, inValue);
      _fileStream = std::ifstream(_fileName, std::ios::binary);
      break;
    default:
      break;
    }
  }

  void cleanup() { Serialization::varFree(_output); }

  struct Reader {
    std::ifstream &_fileStream;
    Reader(std::ifstream &stream) : _fileStream(stream) {}
    void operator()(uint8_t *buf, size_t size) {
      _fileStream.read((char *)buf, size);
    }
  };

  CBVar activate(CBContext *context, const CBVar &input) {
    if (_fileStream.eof()) {
      return Empty;
    }
    Reader r(_fileStream);
    Serialization::deserialize(r, _output);
    return _output;
  }
};

// Register
RUNTIME_CORE_BLOCK(ReadFile);
RUNTIME_BLOCK_inputTypes(ReadFile);
RUNTIME_BLOCK_outputTypes(ReadFile);
RUNTIME_BLOCK_cleanup(ReadFile);
RUNTIME_BLOCK_parameters(ReadFile);
RUNTIME_BLOCK_setParam(ReadFile);
RUNTIME_BLOCK_getParam(ReadFile);
RUNTIME_BLOCK_activate(ReadFile);
RUNTIME_BLOCK_END(ReadFile);

void registerSerializationBlocks() {
  REGISTER_CORE_BLOCK(WriteFile);
  REGISTER_CORE_BLOCK(ReadFile);
}
}; // namespace chainblocks
