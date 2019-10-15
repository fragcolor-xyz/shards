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

  struct Serializer {
    std::ofstream &_fileStream;
    Serializer(std::ofstream &stream) : _fileStream(stream) {}
    void operator()(const uint8_t *buf, size_t size) {
      _fileStream.write((const char *)buf, size);
    }
  };

  CBVar activate(CBContext *context, const CBVar &input) {
    Serializer s(_fileStream);
    serializeVar(input, s);
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

  void cleanup() { freeVar(_output); }

  void freeVar(CBVar &output) {
    switch (output.valueType) {
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
      break;
    case CBType::Bytes:
      delete[] output.payload.bytesValue;
      break;
    case CBType::String:
    case CBType::ContextVar: {
      delete[] output.payload.stringValue;
      break;
    }
    case CBType::Seq: {
      for (auto i = 0; i < stbds_arrlen(output.payload.seqValue); i++) {
        freeVar(output.payload.seqValue[i]);
      }
      stbds_arrfree(output.payload.seqValue);
      output.payload.seqValue = nullptr;
      break;
    }
    case CBType::Table: {
      for (auto i = 0; i < stbds_shlen(output.payload.tableValue); i++) {
        auto &v = output.payload.tableValue[i];
        freeVar(v.value);
        delete[] v.key;
      }
      stbds_shfree(output.payload.tableValue);
      output.payload.tableValue = nullptr;
      break;
    }
    case CBType::Image: {
      delete[] output.payload.imageValue.data;
      break;
    }
    case CBType::Object:
    case CBType::Chain:
    case CBType::Block:
      break;
    }
    output.valueType = None;
  }

  void readVar(CBVar &output) {
    _fileStream.read((char *)&output.valueType, sizeof(output.valueType));
    switch (output.valueType) {
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
      _fileStream.read((char *)&output.payload, sizeof(output.payload));
      break;
    case CBType::Bytes:
      _fileStream.read((char *)&output.payload.bytesSize,
                       sizeof(output.payload.bytesSize));
      output.payload.bytesValue = new uint8_t[output.payload.bytesSize];
      _fileStream.read((char *)output.payload.bytesValue,
                       output.payload.bytesSize);
      break;
    case CBType::String:
    case CBType::ContextVar: {
      uint64_t len;
      _fileStream.read((char *)&len, sizeof(uint64_t));
      output.payload.stringValue = new char[len + 1];
      _fileStream.read((char *)output.payload.stringValue, len);
      const_cast<char *>(output.payload.stringValue)[len] = 0;
      break;
    }
    case CBType::Seq: {
      uint64_t len;
      _fileStream.read((char *)&len, sizeof(uint64_t));
      stbds_arrsetlen(output.payload.seqValue, len);
      for (auto i = 0; i < len; i++) {
        readVar(output.payload.seqValue[i]);
      }
      break;
    }
    case CBType::Table: {
      uint64_t len;
      _fileStream.read((char *)&len, sizeof(uint64_t));
      for (auto i = 0; i < len; i++) {
        CBNamedVar v;
        uint64_t klen;
        _fileStream.read((char *)&klen, sizeof(uint64_t));
        v.key = new char[klen + 1];
        _fileStream.read((char *)v.key, len);
        const_cast<char *>(v.key)[klen] = 0;
        readVar(v.value);
        stbds_shputs(output.payload.tableValue, v);
      }
      break;
    }
    case CBType::Image: {
      _fileStream.read((char *)&output.payload.imageValue.channels,
                       sizeof(output.payload.imageValue.channels));
      _fileStream.read((char *)&output.payload.imageValue.flags,
                       sizeof(output.payload.imageValue.flags));
      _fileStream.read((char *)&output.payload.imageValue.width,
                       sizeof(output.payload.imageValue.width));
      _fileStream.read((char *)&output.payload.imageValue.height,
                       sizeof(output.payload.imageValue.height));
      auto size = output.payload.imageValue.channels *
                  output.payload.imageValue.height *
                  output.payload.imageValue.width;
      output.payload.imageValue.data = new uint8_t[size];
      _fileStream.read((char *)output.payload.imageValue.data, size);
      break;
    }
    case CBType::Object:
    case CBType::Chain:
    case CBType::Block:
      throw CBException("WriteFile: Type cannot be serialized (yet?). " +
                        type2Name(output.valueType));
    }
    _fileStream.read((char *)output.reserved, sizeof(output.reserved));
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    freeVar(_output);
    if (_fileStream.eof()) {
      return Empty;
    }
    readVar(_output);
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
