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

  CBVar activate(CBContext *context, const CBVar &input) {
    stbds_arrsetlen(_storage, 0);
    _strings.clear();

    if (_recursive) {
      fs::path p(input.payload.stringValue);
      auto diterator = fs::recursive_directory_iterator(p);
      for (auto &subp : diterator) {
        auto &path = subp.path();
        auto str = path.string();
        _strings.push_back(str);
      }
    } else {
      fs::path p(input.payload.stringValue);
      auto diterator = fs::directory_iterator(p);
      for (auto &subp : diterator) {
        auto &path = subp.path();
        auto str = path.string();
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
}; // namespace FS

void registerFSBlocks() {
  REGISTER_BLOCK(FS, Iterate);
  REGISTER_BLOCK(FS, Extension);
  REGISTER_BLOCK(FS, Filename);
}
}; // namespace chainblocks