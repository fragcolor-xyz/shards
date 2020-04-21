/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "shared.hpp"
#include <filesystem>
#include <future>
#include <stb_image.h>
#include <stb_image_write.h>
#include <string>
#include <taskflow/taskflow.hpp>

namespace chainblocks {
struct FileBase {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static inline Parameters params{{"File",
                                   "The file to read/write from.",
                                   {CoreInfo::StringStringVarOrNone}}};

  ParamVar _filename{};

  virtual void cleanup() { _filename.cleanup(); }
  void warmup(CBContext *context) { _filename.warmup(context); }

  static CBParametersInfo parameters() { return params; }

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

  bool getFilename(CBContext *context, std::string &filename,
                   bool checkExists = true) {
    auto &ctxFile = _filename.get();
    if (ctxFile.valueType != String)
      return false;

    filename = ctxFile.payload.stringValue;

    // if absolute we are fine to begin with
    std::filesystem::path fp(filename);
    if (fp.is_absolute()) {
      if (checkExists) {
        return std::filesystem::exists(fp);
      } else {
        return true;
      }
    } else {
      // check if script root path is populated if so use it
      std::filesystem::path cp(Globals::RootPath);
      if (std::filesystem::exists(cp)) {
        auto fullpath = cp / filename;
        if (checkExists && !std::filesystem::exists(fullpath)) {
          return false;
        }
        filename = fullpath.string();
        return true;
      } else {
        return false;
      }
    }
  }
};

struct WriteFile : public FileBase {
  std::ofstream _fileStream;
  bool _append = false;

  static inline Parameters params{
      FileBase::params,
      {{"Append",
        "If we should append to the file if existed already or truncate. "
        "(default: false).",
        {CoreInfo::BoolType}}}};

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 1:
      _append = value.payload.boolValue;
      break;
    default:
      FileBase::setParam(index, value);
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 1:
      return Var(_append);
    default:
      return FileBase::getParam(index);
    }
  }

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
      std::string filename;
      if (!getFilename(context, filename, false)) {
        return input;
      }

      if (_append)
        _fileStream = std::ofstream(filename, std::ios::app | std::ios::binary);
      else
        _fileStream =
            std::ofstream(filename, std::ios::trunc | std::ios::binary);
    }

    Writer s(_fileStream);
    Serialization::serialize(input, s);
    return input;
  }
};

struct ReadFile : public FileBase {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  std::ifstream _fileStream;
  CBVar _output{};

  void cleanup() override {
    Serialization::varFree(_output);
    FileBase::cleanup();
    if (_fileStream.is_open()) {
      _fileStream.close();
    }
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
      std::string filename;
      if (!getFilename(context, filename)) {
        return CBVar();
      }

      _fileStream = std::ifstream(filename, std::ios::binary);
    }

    if (_fileStream.eof()) {
      return CBVar();
    }

    Reader r(_fileStream);
    Serialization::deserialize(r, _output);
    return _output;
  }
};

struct LoadImage : public FileBase {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::ImageType; }

  CBVar _output{};

  void cleanup() override {
    if (_output.valueType == Image && _output.payload.imageValue.data) {
      stbi_image_free(_output.payload.imageValue.data);
      _output = {};
    }
    FileBase::cleanup();
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    std::string filename;
    if (!getFilename(context, filename)) {
      throw ActivationError("File not found!");
    }

    CBVar res{};
    res.valueType = Image;
    int x, y, n;
    res.payload.imageValue.data = stbi_load(filename.c_str(), &x, &y, &n, 0);
    if (!res.payload.imageValue.data) {
      throw ActivationError("Failed to load image file");
    }
    res.payload.imageValue.width = uint16_t(x);
    res.payload.imageValue.height = uint16_t(y);
    res.payload.imageValue.channels = uint16_t(n);
    return res;
  }
};

struct WritePNG : public FileBase {
  static CBTypesInfo inputTypes() { return CoreInfo::ImageType; }
  static CBTypesInfo outputTypes() { return CoreInfo::ImageType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    std::string filename;
    if (!getFilename(context, filename, false)) {
      throw ActivationError("Path does not exist!");
    }

    int w = int(input.payload.imageValue.width);
    int h = int(input.payload.imageValue.height);
    int c = int(input.payload.imageValue.channels);
    if (0 == stbi_write_png(filename.c_str(), w, h, c,
                            input.payload.imageValue.data, w * c))
      throw ActivationError("Failed to write PNG file.");
    return input;
  }
};

void registerSerializationBlocks() {
  REGISTER_CBLOCK("WriteFile", WriteFile);
  REGISTER_CBLOCK("ReadFile", ReadFile);
  REGISTER_CBLOCK("LoadImage", LoadImage);
  REGISTER_CBLOCK("WritePNG", WritePNG);
}
}; // namespace chainblocks
