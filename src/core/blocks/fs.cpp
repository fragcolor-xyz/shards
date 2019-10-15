#include "shared.hpp"
#include <filesystem>

namespace fs = std::filesystem;

namespace chainblocks {
namespace FS {
struct Iterate {
  CBSeq _storage = nullptr;
  std::vector<std::string> _strings;

  void destroy() {
    if (_storage) {
      stbds_arrfree(_storage);
    }
  }

  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::strInfo); }
  static CBTypesInfo outputTypes() {
    return CBTypesInfo(SharedTypes::strSeqInfo);
  }

  bool _recursive = true;

  static inline ParamsInfo params = ParamsInfo(ParamsInfo::Param(
      "Recursive",
      "If the iteration should be recursive, following sub-directories.",
      CBTypesInfo(CoreInfo::boolInfo)));
  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _recursive = bool(Var(value));
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_recursive);
    default:
      return Empty;
    }
  }

#ifdef _WIN32
  void replaceAll(std::string &str, const std::string &from,
                  const std::string &to) {
    if (from.empty())
      return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
      str.replace(start_pos, from.length(), to);
      start_pos += to.length();
    }
  }
#endif

  CBVar activate(CBContext *context, const CBVar &input) {
    stbds_arrsetlen(_storage, 0);
    _strings.clear();

    if (_recursive) {
      fs::path p(input.payload.stringValue);
      auto diterator = fs::recursive_directory_iterator(p);
      for (auto &subp : diterator) {
        auto &path = subp.path();
        auto str = path.string();
#ifdef _WIN32
        replaceAll(str, "\\", "/");
#endif
        _strings.push_back(str);
      }
    } else {
      fs::path p(input.payload.stringValue);
      auto diterator = fs::directory_iterator(p);
      for (auto &subp : diterator) {
        auto &path = subp.path();
        auto str = path.string();
#ifdef _WIN32
        replaceAll(str, "\\", "/");
#endif
        _strings.push_back(str);
      }
    }

    return Var(_storage, _strings);
  }
};

RUNTIME_BLOCK(FS, Iterate);
RUNTIME_BLOCK_inputTypes(Iterate);
RUNTIME_BLOCK_outputTypes(Iterate);
RUNTIME_BLOCK_parameters(Iterate);
RUNTIME_BLOCK_setParam(Iterate);
RUNTIME_BLOCK_getParam(Iterate);
RUNTIME_BLOCK_activate(Iterate);
RUNTIME_BLOCK_destroy(Iterate);
RUNTIME_BLOCK_END(Iterate);

struct Extension {
  std::string _output;

  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::strInfo); }
  static CBTypesInfo outputTypes() { return CBTypesInfo(SharedTypes::strInfo); }

  CBVar activate(CBContext *context, const CBVar &input) {
    _output.clear();
    fs::path p(input.payload.stringValue);
    auto ext = p.extension();
    _output.assign(ext.string());
    return Var(_output);
  }
};

RUNTIME_BLOCK(FS, Extension);
RUNTIME_BLOCK_inputTypes(Extension);
RUNTIME_BLOCK_outputTypes(Extension);
RUNTIME_BLOCK_activate(Extension);
RUNTIME_BLOCK_END(Extension);

struct IsFile {
  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::strInfo); }
  static CBTypesInfo outputTypes() {
    return CBTypesInfo(SharedTypes::boolInfo);
  }
  CBVar activate(CBContext *context, const CBVar &input) {
    fs::path p(input.payload.stringValue);
    if (fs::exists(p) && !fs::is_directory(p)) {
      return True;
    } else {
      return False;
    }
  }
};

RUNTIME_BLOCK(FS, IsFile);
RUNTIME_BLOCK_inputTypes(IsFile);
RUNTIME_BLOCK_outputTypes(IsFile);
RUNTIME_BLOCK_activate(IsFile);
RUNTIME_BLOCK_END(IsFile);

struct IsDirectory {
  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::strInfo); }
  static CBTypesInfo outputTypes() {
    return CBTypesInfo(SharedTypes::boolInfo);
  }
  CBVar activate(CBContext *context, const CBVar &input) {
    fs::path p(input.payload.stringValue);
    if (fs::exists(p) && fs::is_directory(p)) {
      return True;
    } else {
      return False;
    }
  }
};

RUNTIME_BLOCK(FS, IsDirectory);
RUNTIME_BLOCK_inputTypes(IsDirectory);
RUNTIME_BLOCK_outputTypes(IsDirectory);
RUNTIME_BLOCK_activate(IsDirectory);
RUNTIME_BLOCK_END(IsDirectory);

struct Filename {
  std::string _output;
  bool _noExt = false;

  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::strInfo); }
  static CBTypesInfo outputTypes() { return CBTypesInfo(SharedTypes::strInfo); }

  static inline ParamsInfo params = ParamsInfo(ParamsInfo::Param(
      "NoExtension", "If the extension should be stripped from the result.",
      CBTypesInfo(CoreInfo::boolInfo)));
  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _noExt = bool(Var(value));
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_noExt);
    default:
      return Empty;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    _output.clear();
    fs::path p(input.payload.stringValue);
    p = p.filename();
    if (_noExt && p.has_extension()) {
      p = p.replace_extension("");
    }
    _output.assign(p.string());
    return Var(_output);
  }
};

RUNTIME_BLOCK(FS, Filename);
RUNTIME_BLOCK_inputTypes(Filename);
RUNTIME_BLOCK_outputTypes(Filename);
RUNTIME_BLOCK_parameters(Filename);
RUNTIME_BLOCK_setParam(Filename);
RUNTIME_BLOCK_getParam(Filename);
RUNTIME_BLOCK_activate(Filename);
RUNTIME_BLOCK_END(Filename);

struct Read {
  std::vector<uint8_t> _buffer;
  bool _binary = false;

  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::strInfo); }
  CBTypesInfo outputTypes() {
    if (_binary)
      return CBTypesInfo(SharedTypes::bytesInfo);
    else
      return CBTypesInfo(SharedTypes::strInfo);
  }

  static inline ParamsInfo params = ParamsInfo(ParamsInfo::Param(
      "Bytes", "If the output should be Bytes instead of String.",
      CBTypesInfo(CoreInfo::boolInfo)));
  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _binary = bool(Var(value));
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_binary);
    default:
      return Empty;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    _buffer.clear();
    fs::path p(input.payload.stringValue);
    if (!fs::exists(p)) {
      throw CBException("FS.Read, file does not exist.");
    }

    if (_binary) {
      std::ifstream file(p.string(), std::ios::binary);
      _buffer.assign(std::istreambuf_iterator<char>(file), {});
      return Var(&_buffer.front(), _buffer.size());
    } else {
      std::ifstream file(p.string(), std::ios::binary);
      _buffer.assign(std::istreambuf_iterator<char>(file), {});
      return Var((const char *)&_buffer.front());
    }
  }
};

RUNTIME_BLOCK(FS, Read);
RUNTIME_BLOCK_inputTypes(Read);
RUNTIME_BLOCK_outputTypes(Read);
RUNTIME_BLOCK_parameters(Read);
RUNTIME_BLOCK_setParam(Read);
RUNTIME_BLOCK_getParam(Read);
RUNTIME_BLOCK_activate(Read);
RUNTIME_BLOCK_END(Read);

struct Write {
  ContextableVar _contents{};
  bool _overwrite = false;
  bool _append = false;

  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::strInfo); }
  static CBTypesInfo outputTypes() { return CBTypesInfo(SharedTypes::strInfo); }

  static inline ParamsInfo params = ParamsInfo(
      ParamsInfo::Param("Contents", "The string or bytes to write to the file.",
                        CBTypesInfo(SharedTypes::ctxOrNoneInfo)),
      ParamsInfo::Param("Overwrite", "Overwrite the file if already exists.",
                        CBTypesInfo(SharedTypes::boolInfo)),
      ParamsInfo::Param("Append",
                        "If we should append Contents to an existing file.",
                        CBTypesInfo(SharedTypes::boolInfo)));
  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _contents.setParam(value);
      break;
    case 1:
      _overwrite = value.payload.boolValue;
      break;
    case 2:
      _append = value.payload.boolValue;
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _contents.getParam();
    case 1:
      return Var(_overwrite);
    case 2:
      return Var(_append);
    default:
      return Empty;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto contents = _contents.get(context);
    if (contents.valueType != None) {
      fs::path p(input.payload.stringValue);
      if (!_overwrite && !_append && fs::exists(p)) {
        throw CBException(
            "FS.Write, file already exists and overwrite flag is not on!.");
      }

      auto flags = std::ios::binary;
      if (_append) {
        flags |= std::ios::app;
      }
      std::ofstream file(p.string(), flags);
      if (contents.valueType == String) {
        file << contents.payload.stringValue;
      } else {
        file.write((const char *)contents.payload.bytesValue,
                   contents.payload.bytesSize);
      }
    }
    return input;
  }
};

RUNTIME_BLOCK(FS, Write);
RUNTIME_BLOCK_inputTypes(Write);
RUNTIME_BLOCK_outputTypes(Write);
RUNTIME_BLOCK_parameters(Write);
RUNTIME_BLOCK_setParam(Write);
RUNTIME_BLOCK_getParam(Write);
RUNTIME_BLOCK_activate(Write);
RUNTIME_BLOCK_END(Write);
}; // namespace FS

void registerFSBlocks() {
  REGISTER_BLOCK(FS, Iterate);
  REGISTER_BLOCK(FS, Extension);
  REGISTER_BLOCK(FS, Filename);
  REGISTER_BLOCK(FS, Read);
  REGISTER_BLOCK(FS, Write);
  REGISTER_BLOCK(FS, IsFile);
  REGISTER_BLOCK(FS, IsDirectory);
}
}; // namespace chainblocks