/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "file_base.hpp"
#include <shards/core/serialization.hpp>
#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>
#include <fstream>
#include <future>
#include <string>

namespace shards {

struct WriteFile {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR(""); }

  PARAM_PARAMVAR(_filename, "Filename", "The file to write to.", {CoreInfo::StringStringVarOrNone});
  PARAM_VAR(_append, "Append", "If we should append to the file if existed already or truncate. (default: false).",
            {CoreInfo::BoolType});
  PARAM_VAR(_flush, "Flush", "If the file should be flushed to disk after every write.", {CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_filename), PARAM_IMPL_FOR(_append), PARAM_IMPL_FOR(_flush));

  std::ofstream _fileStream;
  Serialization serial;

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) {
    if (_fileStream.good()) {
      _fileStream.flush();
    }
    _fileStream = {};
    PARAM_CLEANUP(context);
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  bool getAppend() const { return _append->isNone() ? false : _append->payload.boolValue; }
  bool getFlush() const { return _flush->isNone() ? false : _flush->payload.boolValue; }

  struct Writer {
    std::ofstream &_fileStream;
    Writer(std::ofstream &stream) : _fileStream(stream) {}
    void operator()(const uint8_t *buf, size_t size) { _fileStream.write((const char *)buf, size); }
  };

  SHVar activate(SHContext *context, const SHVar &input) {
    if (!_fileStream.is_open() || _filename.isVariable()) {
      std::string filename;
      if (!getPathChecked(filename, _filename, false)) {
        return input;
      }

      namespace fs = boost::filesystem;

      // make sure to create directories
      fs::path p(filename);
      auto parent_path = p.parent_path();
      if (!parent_path.empty() && !fs::exists(parent_path))
        fs::create_directories(p.parent_path());

      if (getAppend())
        _fileStream = std::ofstream(filename, std::ios::app | std::ios::binary);
      else
        _fileStream = std::ofstream(filename, std::ios::trunc | std::ios::binary);
    }

    Writer s(_fileStream);
    serial.reset();
    serial.serialize(input, s);
    if (getFlush()) {
      _fileStream.flush();
    }
    return input;
  }
};

struct ReadFile {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR(""); }

  PARAM_PARAMVAR(_filename, "Filename", "The file to read from.", {CoreInfo::StringStringVarOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_filename));

  std::ifstream _fileStream;
  SHVar _output{};
  Serialization serial;

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) {
    destroyVar(_output);
    _fileStream = {};
    PARAM_CLEANUP(context);
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  struct Reader {
    std::ifstream &_fileStream;
    Reader(std::ifstream &stream) : _fileStream(stream) {}
    void operator()(uint8_t *buf, size_t size) { _fileStream.read((char *)buf, size); }
  };

  SHVar activate(SHContext *context, const SHVar &input) {
    if (!_fileStream.is_open() || _filename.isVariable()) {
      std::string filename;
      if (!getPathChecked(filename, _filename, true)) {
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
  static SHOptionalString help() {
    return SHCCSTR("This shard takes a value and converts it to a serialized binary representation (a serialized byte sequence).");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("This shard will take any value.");
  }

  static SHOptionalString outputHelp() {
    return SHCCSTR("This shard will return a seriliazed byte sequence representing the input value.");
  }
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }

  Serialization serial;
  std::vector<uint8_t> _buffer;

  void cleanup(SHContext *context) { _buffer.clear(); }

  SHVar activate(SHContext *context, const SHVar &input) {
    BufferRefWriter s(_buffer);
    _buffer.clear();
    serial.reset();
    serial.serialize(input, s);
    return Var(&_buffer.front(), _buffer.size());
  }
};

struct FromBytes {
  static SHOptionalString help() {
    return SHCCSTR("This shard takes a serialized binary representation of a value and convert it back to its original type.");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("This shard will take a byte sequence.");
  }

  static SHOptionalString outputHelp() {
    return SHCCSTR("This shard will return the original value converted back to its original type.");
  }
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
