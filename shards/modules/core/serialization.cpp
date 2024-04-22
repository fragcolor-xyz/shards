/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "file_base.hpp"
#include <shards/core/serialization.hpp>
#include <fstream>
#include <future>
#include <string>

namespace shards {
struct WriteFile : public FileBase {
  std::ofstream _fileStream;
  bool _append = false;
  bool _flush = false;

  static inline Parameters params{
      FileBase::params,
      {{"Append",
        SHCCSTR("If we should append to the file if existed already or "
                "truncate. (default: false)."),
        {CoreInfo::BoolType}},
       {"Flush", SHCCSTR("If the file should be flushed to disk after every write."), {CoreInfo::BoolType}}}};

  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 1:
      _append = value.payload.boolValue;
      break;
    case 2:
      _flush = value.payload.boolValue;
      break;
    default:
      FileBase::setParam(index, value);
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 1:
      return Var(_append);
    case 2:
      return Var(_flush);
    default:
      return FileBase::getParam(index);
    }
  }

  void cleanup(SHContext* context) {
    if (_fileStream.good()) {
      _fileStream.flush();
    }
    _fileStream = {};
    FileBase::cleanup(context);
  }

  struct Writer {
    std::ofstream &_fileStream;
    Writer(std::ofstream &stream) : _fileStream(stream) {}
    void operator()(const uint8_t *buf, size_t size) { _fileStream.write((const char *)buf, size); }
  };

  Serialization serial;

  SHVar activate(SHContext *context, const SHVar &input) {
    if (!_fileStream.is_open() || (_filename.isVariable() && _filename.get() != _currentFileName)) {
      std::string filename;
      if (!getFilename(context, filename, false)) {
        return input;
      }

      namespace fs = boost::filesystem;

      // make sure to create directories
      fs::path p(filename);
      auto parent_path = p.parent_path();
      if (!parent_path.empty() && !fs::exists(parent_path))
        fs::create_directories(p.parent_path());

      if (_append)
        _fileStream = std::ofstream(filename, std::ios::app | std::ios::binary);
      else
        _fileStream = std::ofstream(filename, std::ios::trunc | std::ios::binary);
    }

    Writer s(_fileStream);
    serial.reset();
    serial.serialize(input, s);
    if (_flush) {
      _fileStream.flush();
    }
    return input;
  }
};

struct ReadFile : public FileBase {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }

  std::ifstream _fileStream;
  SHVar _output{};

  void cleanup(SHContext* context) {
    destroyVar(_output);
    _fileStream = {};
    FileBase::cleanup(context);
  }

  struct Reader {
    std::ifstream &_fileStream;
    Reader(std::ifstream &stream) : _fileStream(stream) {}
    void operator()(uint8_t *buf, size_t size) { _fileStream.read((char *)buf, size); }
  };

  Serialization serial;

  SHVar activate(SHContext *context, const SHVar &input) {
    if (!_fileStream.is_open() || (_filename.isVariable() && _filename.get() != _currentFileName)) {
      std::string filename;
      if (!getFilename(context, filename)) {
        return Var::Empty;
      }

      _fileStream = std::ifstream(filename, std::ios::binary);
    }

    if (_fileStream.peek() == EOF)
      return Var::Empty;

    Reader r(_fileStream);
    serial.reset();
    serial.deserialize(r, _output);
    return _output;
  }
};

struct ToBytes {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }

  Serialization serial;
  std::vector<uint8_t> _buffer;

  void cleanup(SHContext* context) { _buffer.clear(); }

  SHVar activate(SHContext *context, const SHVar &input) {
    BufferRefWriter s(_buffer);
    _buffer.clear();
    serial.reset();
    serial.serialize(input, s);
    return Var(&_buffer.front(), _buffer.size());
  }
};

struct FromBytes {
  static SHTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  Serialization serial;
  SHVar _output{};

  void destroy() { destroyVar(_output); }

  SHVar activate(SHContext *context, const SHVar &input) {
    VarReader r(input);
    serial.reset();
    serial.deserialize(r, _output);
    return _output;
  }
};

SHARDS_REGISTER_FN(serialization) {
  REGISTER_SHARD("WriteFile", WriteFile);
  REGISTER_SHARD("ReadFile", ReadFile);
  REGISTER_SHARD("FromBytes", FromBytes);
  REGISTER_SHARD("ToBytes", ToBytes);
}
}; // namespace shards
