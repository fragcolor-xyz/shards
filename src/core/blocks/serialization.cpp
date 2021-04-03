/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2021 Giovanni Petrantoni */

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "shared.hpp"
#include <filesystem>
#include <fstream>
#include <future>
#include <stb_image.h>
#include <stb_image_write.h>
#include <string>

namespace chainblocks {
struct FileBase {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static inline Parameters params{{"File",
                                   CBCCSTR("The file to read/write from."),
                                   {CoreInfo::StringStringVarOrNone}}};

  ParamVar _filename{};
  OwnedVar _currentFileName{};

  void cleanup() { _filename.cleanup(); }
  void warmup(CBContext *context) { _filename.warmup(context); }

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
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
    _currentFileName = _filename.get();

    // if absolute we are fine to begin with
    std::filesystem::path fp(filename);
    if (checkExists) {
      return std::filesystem::exists(fp);
    } else {
      return true;
    }
  }
};

struct WriteFile : public FileBase {
  std::ofstream _fileStream;
  bool _append = false;

  static inline Parameters params{
      FileBase::params,
      {{"Append",
        CBCCSTR("If we should append to the file if existed already or "
                "truncate. (default: false)."),
        {CoreInfo::BoolType}}}};

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
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

  void cleanup() {
    if (_fileStream.good()) {
      _fileStream.flush();
    }
    _fileStream = {};
    FileBase::cleanup();
  }

  struct Writer {
    std::ofstream &_fileStream;
    Writer(std::ofstream &stream) : _fileStream(stream) {}
    void operator()(const uint8_t *buf, size_t size) {
      _fileStream.write((const char *)buf, size);
    }
  };

  Serialization serial;

  CBVar activate(CBContext *context, const CBVar &input) {
    if (!_fileStream.is_open() ||
        (_filename.isVariable() && _filename.get() != _currentFileName)) {
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
    serial.reset();
    serial.serialize(input, s);
    return input;
  }
};

struct ReadFile : public FileBase {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  std::ifstream _fileStream;
  CBVar _output{};

  void cleanup() {
    Serialization::varFree(_output);
    _fileStream = {};
    FileBase::cleanup();
  }

  struct Reader {
    std::ifstream &_fileStream;
    Reader(std::ifstream &stream) : _fileStream(stream) {}
    void operator()(uint8_t *buf, size_t size) {
      _fileStream.read((char *)buf, size);
    }
  };

  Serialization serial;

  CBVar activate(CBContext *context, const CBVar &input) {
    if (!_fileStream.is_open() ||
        (_filename.isVariable() && _filename.get() != _currentFileName)) {
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
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BytesType; }

  Serialization serial;
  std::vector<uint8_t> _buffer;

  void cleanup() { _buffer.clear(); }

  struct Writer {
    std::vector<uint8_t> &_buffer;
    Writer(std::vector<uint8_t> &stream) : _buffer(stream) {}
    void operator()(const uint8_t *buf, size_t size) {
      _buffer.insert(_buffer.end(), buf, buf + size);
    }
  };

  CBVar activate(CBContext *context, const CBVar &input) {
    Writer s(_buffer);
    _buffer.clear();
    serial.reset();
    serial.serialize(input, s);
    return Var(&_buffer.front(), _buffer.size());
  }
};

struct FromBytes {
  static CBTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  Serialization serial;
  CBVar _output{};

  void destroy() { Serialization::varFree(_output); }

  struct Reader {
    const CBVar &_bytesVar;
    size_t _offset;
    Reader(const CBVar &var) : _bytesVar(var), _offset(0) {}
    void operator()(uint8_t *buf, size_t size) {
      if (_bytesVar.payload.bytesSize < _offset + size) {
        throw ActivationError("FromBytes buffer underrun");
      }

      memcpy(buf, _bytesVar.payload.bytesValue + _offset, size);
      _offset += size;
    }
  };

  CBVar activate(CBContext *context, const CBVar &input) {
    Reader r(input);
    serial.reset();
    serial.deserialize(r, _output);
    return _output;
  }
};

struct LoadImage : public FileBase {
  enum class BPP { u8, u16, f32 };
  static inline EnumInfo<BPP> BPPEnum{"BPP", CoreCC, 'ibpp'};
  static inline Type BPPEnumInfo{
      {CBType::Enum, {.enumeration = {CoreCC, 'ibpp'}}}};

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::ImageType; }

  static inline Parameters params{
      FileBase::params,
      {{"BPP",
        CBCCSTR("bits per pixel (HDR images loading and such!)"),
        {BPPEnumInfo}}}};

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 1:
      _bpp = BPP(value.payload.enumValue);
      break;
    default:
      FileBase::setParam(index, value);
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 1:
      return Var::Enum(_bpp, CoreCC, 'ibpp');
    default:
      return FileBase::getParam(index);
    }
  }

  CBVar _output{};
  BPP _bpp{BPP::u8};

  void cleanup() {
    if (_output.valueType == Image && _output.payload.imageValue.data) {
      stbi_image_free(_output.payload.imageValue.data);
      _output = Var::Empty;
    }

    FileBase::cleanup();
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    std::string filename;
    if (!getFilename(context, filename)) {
      throw ActivationError("File not found!");
    }

    if (_output.valueType == Image && _output.payload.imageValue.data) {
      stbi_image_free(_output.payload.imageValue.data);
      _output = Var::Empty;
    }

    _output.valueType = Image;
    int x, y, n;
    if (_bpp == BPP::u8) {
      _output.payload.imageValue.data =
          (uint8_t *)stbi_load(filename.c_str(), &x, &y, &n, 0);
      if (!_output.payload.imageValue.data) {
        throw ActivationError("Failed to load image file");
      }
    } else if (_bpp == BPP::u16) {
      _output.payload.imageValue.data =
          (uint8_t *)stbi_load_16(filename.c_str(), &x, &y, &n, 0);
      if (!_output.payload.imageValue.data) {
        throw ActivationError("Failed to load image file");
      }
    } else {
      _output.payload.imageValue.data =
          (uint8_t *)stbi_loadf(filename.c_str(), &x, &y, &n, 0);
      if (!_output.payload.imageValue.data) {
        throw ActivationError("Failed to load image file");
      }
    }
    _output.payload.imageValue.width = uint16_t(x);
    _output.payload.imageValue.height = uint16_t(y);
    _output.payload.imageValue.channels = uint16_t(n);
    switch (_bpp) {
    case BPP::u16:
      _output.payload.imageValue.flags = CBIMAGE_FLAGS_16BITS_INT;
      break;
    case BPP::f32:
      _output.payload.imageValue.flags = CBIMAGE_FLAGS_32BITS_FLOAT;
      break;
    default:
      _output.payload.imageValue.flags = 0;
      break;
    }
    return _output;
  }
};

struct WritePNG : public FileBase {
  static CBTypesInfo inputTypes() { return CoreInfo::ImageType; }
  static CBTypesInfo outputTypes() { return CoreInfo::ImageType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto pixsize = 1;
    if ((input.payload.imageValue.flags & CBIMAGE_FLAGS_16BITS_INT) ==
        CBIMAGE_FLAGS_16BITS_INT)
      pixsize = 2;
    else if ((input.payload.imageValue.flags & CBIMAGE_FLAGS_32BITS_FLOAT) ==
             CBIMAGE_FLAGS_32BITS_FLOAT)
      pixsize = 4;

    if (pixsize != 1) {
      throw ActivationError("Bits per pixel must be 8");
    }

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
  REGISTER_CBLOCK("FromBytes", FromBytes);
  REGISTER_CBLOCK("ToBytes", ToBytes);
}
}; // namespace chainblocks
