/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#define STB_IMAGE_IMPLEMENTATION

#include "shared.hpp"
#include <filesystem>
#include <future>
#include <stb_image.h>
#include <string>

// TODO, make ReadFile and WriteFile use maybe ASIO async to do syscalls in a
// worker thread Maybe as a option, its kinda something to profile, but for now
// lets wait a proper usage

namespace chainblocks {
struct FileBase {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static inline ParamsInfo paramsInfo = ParamsInfo(ParamsInfo::Param(
      "File", "The file to read/write from.", CoreInfo::StringStringVarOrNone));

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

  bool getFilename(CBContext *context, std::string &filename,
                   bool checkExists = true) {
    auto &ctxFile = _filename(context);
    if (ctxFile.valueType != String)
      return false;

    filename = ctxFile.payload.stringValue;

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
      std::string filename;
      if (!getFilename(context, filename, false)) {
        return input;
      }

      _fileStream = std::ofstream(filename, std::ios::app | std::ios::binary);
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
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

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
      std::string filename;
      if (!getFilename(context, filename)) {
        return Empty;
      }

      _fileStream = std::ifstream(filename, std::ios::binary);
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
      throw CBException("File not found!");
    }

    auto asyncRes = std::async(
        std::launch::async,
        [](std::string filename) {
          CBVar res{};
          res.valueType = Image;
          int x, y, n;
          res.payload.imageValue.data =
              stbi_load(filename.c_str(), &x, &y, &n, 0);
          if (!res.payload.imageValue.data) {
            throw CBException("Failed to load image file");
          }
          res.payload.imageValue.width = uint16_t(x);
          res.payload.imageValue.height = uint16_t(y);
          res.payload.imageValue.channels = uint16_t(n);
          return res;
        },
        filename);

    // Wait suspending!
    while (true) {
      auto state = asyncRes.wait_for(std::chrono::seconds(0));
      if (state == std::future_status::ready)
        break;
      auto chainState = chainblocks::suspend(context, 0);
      if (chainState.payload.chainState != Continue) {
        // Here communicate to the thread.. but hmm should be fine without
        // anything in this case, cannot send cancelation anyway
        return chainState;
      }
    }

    // This should also throw if we had exceptions
    return asyncRes.get();
  }
};

typedef BlockWrapper<LoadImage> LoadImageBlock;

void registerSerializationBlocks() {
  REGISTER_CORE_BLOCK(WriteFile);
  REGISTER_CORE_BLOCK(ReadFile);
  registerBlock("LoadImage", &LoadImageBlock::create);
}
}; // namespace chainblocks
