/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#include "shared.hpp"
#include <string>

namespace chainblocks {
struct FileBase {
  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }
  static CBTypesInfo outputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }

  static inline ParamsInfo paramsInfo = ParamsInfo(
      ParamsInfo::Param("File", "The file to read/write from.",
                        CBTypesInfo(SharedTypes::ctxOrStrOrNoneInfo)));

  ParamVar _filename{};

  virtual void cleanup() { _filename.reset(); }

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _filename = value;
      cleanup();
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    auto res = CBVar();
    switch (index) {
    case 0:
      return _filename;
    default:
      break;
    }
    return res;
  }
};

struct WriteFile : public FileBase {
  std::ofstream _fileStream;

  void cleanup() override {
    if (_fileStream.good()) {
      _fileStream.flush();
      _fileStream = {};
    }
    FileBase::cleanup();
  }

  struct Writer {
    std::ofstream &_fileStream;
    Writer(std::ofstream &stream) : _fileStream(stream) {}
    void operator()(const uint8_t *buf, size_t size) {
      _fileStream.write((const char *)buf, size);
    }
  };

  CBVar activate(CBContext *context, const CBVar &input) {
    if (!_fileStream.is_open()) {
      auto &fvar = _filename(context);
      if (fvar.valueType == String) {
        auto filename = fvar.payload.stringValue;
        _fileStream = std::ofstream(filename, std::ios::app | std::ios::binary);
      } else {
        return input;
      }
    }

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

  void cleanup() override {
    Serialization::varFree(_output);
    FileBase::cleanup();
  }

  struct Reader {
    std::ifstream &_fileStream;
    Reader(std::ifstream &stream) : _fileStream(stream) {}
    void operator()(uint8_t *buf, size_t size) {
      _fileStream.read((char *)buf, size);
    }
  };

  CBVar activate(CBContext *context, const CBVar &input) {
    if (!_fileStream.is_open()) {
      auto &fvar = _filename(context);
      if (fvar.valueType == String) {
        auto filename = fvar.payload.stringValue;
        _fileStream = std::ifstream(filename, std::ios::binary);
      } else {
        return Empty;
      }
    }

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
