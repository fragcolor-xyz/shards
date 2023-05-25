/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "imaging.cpp"
#include <shards/core/shared.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <future>
#include <stb_image.h>
#include <stb_image_write.h>
#include <string>

namespace fs = boost::filesystem;

namespace shards {
struct FileBase {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static inline Parameters params{{"File", SHCCSTR("The file to read/write from."), {CoreInfo::StringStringVarOrNone}}};

  ParamVar _filename{};
  OwnedVar _currentFileName{};

  void cleanup() { _filename.cleanup(); }
  void warmup(SHContext *context) { _filename.warmup(context); }

  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _filename = value;
      cleanup();
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    auto res = SHVar();
    switch (index) {
    case 0:
      return _filename;
    default:
      break;
    }
    return res;
  }

  bool getFilename(SHContext *context, std::string &filename, bool checkExists = true) {
    auto &ctxFile = _filename.get();
    if (ctxFile.valueType != SHType::String)
      return false;

    filename = SHSTRVIEW(ctxFile);
    _currentFileName = _filename.get();

    // if absolute we are fine to begin with
    fs::path fp(filename);
    if (checkExists) {
      return fs::exists(fp);
    } else {
      return true;
    }
  }
};

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

  void cleanup() {
    destroyVar(_output);
    _fileStream = {};
    FileBase::cleanup();
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

  void cleanup() { _buffer.clear(); }

  struct Writer {
    std::vector<uint8_t> &_buffer;
    Writer(std::vector<uint8_t> &stream) : _buffer(stream) {}
    void operator()(const uint8_t *buf, size_t size) { _buffer.insert(_buffer.end(), buf, buf + size); }
  };

  SHVar activate(SHContext *context, const SHVar &input) {
    Writer s(_buffer);
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

  struct Reader {
    const SHVar &_bytesVar;
    size_t _offset;
    Reader(const SHVar &var) : _bytesVar(var), _offset(0) {}
    void operator()(uint8_t *buf, size_t size) {
      if (_bytesVar.payload.bytesSize < _offset + size) {
        throw ActivationError("FromBytes buffer underrun");
      }

      memcpy(buf, _bytesVar.payload.bytesValue + _offset, size);
      _offset += size;
    }
  };

  SHVar activate(SHContext *context, const SHVar &input) {
    Reader r(input);
    serial.reset();
    serial.deserialize(r, _output);
    return _output;
  }
};

struct LoadImage : public FileBase {
  enum class BPP { u8, u16, f32 };
  DECL_ENUM_INFO(BPP, BPP, 'ibpp');

  static SHTypesInfo inputTypes() { return CoreInfo::BytesOrAny; }
  static SHTypesInfo outputTypes() { return CoreInfo::ImageType; }

  static inline Parameters params{FileBase::params,
                                  {{"BPP", SHCCSTR("bits per pixel (HDR images loading and such!)"), {BPPEnumInfo::Type}}}};

  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 1:
      _bpp = BPP(value.payload.enumValue);
      break;
    default:
      FileBase::setParam(index, value);
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 1:
      return Var::Enum(_bpp, CoreCC, BPPEnumInfo::TypeId);
    default:
      return FileBase::getParam(index);
    }
  }

  SHVar _output{};
  BPP _bpp{BPP::u8};

  void cleanup() {
    if (_output.valueType == SHType::Image && _output.payload.imageValue.data) {
      stbi_image_free(_output.payload.imageValue.data);
      _output = Var::Empty;
    }

    FileBase::cleanup();
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    bool bytesInput = input.valueType == SHType::Bytes;

    // free the old image if we have one
    if (_output.valueType == SHType::Image && _output.payload.imageValue.data) {
      stbi_image_free(_output.payload.imageValue.data);
      _output = Var::Empty;
    }

    stbi_set_flip_vertically_on_load_thread(0);

    uint8_t *bytesValue;
    uint32_t bytesSize;
    bool is_png{false};

    // if we have a file input, load them into bytes form
    std::string filename;
    std::vector<uint8_t> buffer;

    if (!bytesInput) {
      // need a proper filename in this case
      if (!getFilename(context, filename)) {
        throw ActivationError(fmt::format("File not found: {}!", filename));
      }

      // load the file
      std::ifstream file(filename, std::ios::binary);
      if (!file) {
        throw ActivationError(fmt::format("Error opening file: {}!", filename));
      }

      // get length of file
      file.seekg(0, std::ios::end);
      std::streampos length = file.tellg();
      file.seekg(0, std::ios::beg);

      // read file into buffer
      buffer.resize(length);
      file.read(reinterpret_cast<char *>(buffer.data()), length);

      bytesValue = buffer.data();
      bytesSize = static_cast<uint32_t>(length);
    } else {
      // image already given in bytes form
      bytesValue = input.payload.bytesValue;
      bytesSize = input.payload.bytesSize;
    }

    // check if it's a png (Using same signature check as stbi__check_png_header)
    static const uint8_t png_sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    is_png = (std::memcmp(bytesValue, png_sig, 8) == 0);

    _output.valueType = SHType::Image;
    int x, y, n;
    switch (_bpp) {
    case BPP::u8:
      _output.payload.imageValue.data =
          reinterpret_cast<uint8_t *>(stbi_load_from_memory(bytesValue, static_cast<int>(bytesSize), &x, &y, &n, 0));
      break;
    case BPP::u16:
      _output.payload.imageValue.data =
          reinterpret_cast<uint8_t *>(stbi_load_16_from_memory(bytesValue, static_cast<int>(bytesSize), &x, &y, &n, 0));
      break;
    default:
      _output.payload.imageValue.data =
          reinterpret_cast<uint8_t *>(stbi_loadf_from_memory(bytesValue, static_cast<int>(bytesSize), &x, &y, &n, 0));
      break;
    }

    if (!_output.payload.imageValue.data) {
      throw ActivationError("Failed to load image file");
    }

    _output.payload.imageValue.width = uint16_t(x);
    _output.payload.imageValue.height = uint16_t(y);
    _output.payload.imageValue.channels = uint16_t(n);
    switch (_bpp) {
    case BPP::u16:
      _output.payload.imageValue.flags = SHIMAGE_FLAGS_16BITS_INT;
      break;
    case BPP::f32:
      _output.payload.imageValue.flags = SHIMAGE_FLAGS_32BITS_FLOAT;
      break;
    default:
      _output.payload.imageValue.flags = 0;
      break;
    }

    // Premultiply the alpha channel if it is a PNG
    auto pixsize = getPixelSize(_output);
    if (is_png) {
      // premultiply the alpha channel
      switch (pixsize) {
      case 1:
        Imaging::premultiplyAlpha<uint8_t>(_output, _output, _output.payload.imageValue.width, _output.payload.imageValue.height);
        break;
      case 2:
        Imaging::premultiplyAlpha<uint16_t>(_output, _output, _output.payload.imageValue.width,
                                            _output.payload.imageValue.height);
        break;
      case 4:
        Imaging::premultiplyAlpha<float>(_output, _output, _output.payload.imageValue.width, _output.payload.imageValue.height);
        break;
      }
    }

    _output.version++;
    return _output;
  }
};

struct WritePNG : public FileBase {
  static inline Types OutputTypes = {{CoreInfo::BytesType, CoreInfo::ImageType}};
  std::vector<uint8_t> _scratch;
  std::vector<uint8_t> _output;

  static SHTypesInfo inputTypes() { return CoreInfo::ImageType; }
  SHTypesInfo outputTypes() { return OutputTypes; }

  static void write_func(void *context, void *data, int size) {
    auto self = reinterpret_cast<WritePNG *>(context);
    self->_output.resize(size);
    memcpy(self->_output.data(), data, size);
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    // If param is none we output the bytes directly
    if (_filename->valueType == SHType::None) {
      return CoreInfo::BytesType;
    } else {
      return CoreInfo::ImageType;
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto pixsize = getPixelSize(input);

    std::string filename;
    if (_filename->valueType != SHType::None) {
      if (!getFilename(context, filename, false)) {
        throw ActivationError("Path does not exist!");
      }
    }

    int w = int(input.payload.imageValue.width);
    int h = int(input.payload.imageValue.height);
    int c = int(input.payload.imageValue.channels);

    // demultiply alpha if needed, limited to 4 channels
    if (c == 4 && (input.payload.imageValue.flags & SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA) == SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA) {
      _scratch.resize(w * h * 4 * pixsize);
      for (int ih = 0; ih < h; ih++) {
        for (int iw = 0; iw < w; iw++) {
          switch (pixsize) {
          case 1:
            Imaging::demultiplyAlpha<uint8_t>(input, _scratch, w, h);
            break;
          case 2:
            Imaging::demultiplyAlpha<uint16_t>(input, _scratch, w, h);
            break;
          case 4:
            Imaging::demultiplyAlpha<float>(input, _scratch, w, h);
            break;
          }
        }
      }

      // all done, write the file or buffer
      if (!filename.empty()) {
        if (0 == stbi_write_png(filename.c_str(), w, h, c, _scratch.data(), w * c))
          throw ActivationError("Failed to write PNG file.");
      } else {
        if (0 == stbi_write_png_to_func(write_func, this, w, h, c, _scratch.data(), w * c))
          throw ActivationError("Failed to write PNG file.");
        return Var(_output.data(), _output.size());
      }
    } else {
      // just write the file or buffer in this case straight
      if (!filename.empty()) {
        if (0 == stbi_write_png(filename.c_str(), w, h, c, input.payload.imageValue.data, w * c))
          throw ActivationError("Failed to write PNG file.");
      } else {
        if (0 == stbi_write_png_to_func(write_func, this, w, h, c, input.payload.imageValue.data, w * c))
          throw ActivationError("Failed to write PNG file.");
        return Var(_output.data(), _output.size());
      }
    }

    return input;
  }
};

SHARDS_REGISTER_FN(serialization) {
  REGISTER_ENUM(LoadImage::BPPEnumInfo);

  REGISTER_SHARD("WriteFile", WriteFile);
  REGISTER_SHARD("ReadFile", ReadFile);
  REGISTER_SHARD("LoadImage", LoadImage);
  REGISTER_SHARD("WritePNG", WritePNG);
  REGISTER_SHARD("FromBytes", FromBytes);
  REGISTER_SHARD("ToBytes", ToBytes);
}
}; // namespace shards
