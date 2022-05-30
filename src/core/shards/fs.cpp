/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "shared.hpp"
#include <boost/algorithm/string.hpp>
#include <fstream>

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
using ErrorCode = boost::system::error_code;

namespace shards {
namespace FS {
struct Iterate {
  SHSeq _storage = {};
  std::vector<std::string> _strings;

  void destroy() {
    if (_storage.elements) {
      shards::arrayFree(_storage);
    }
  }

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringSeqType; }

  bool _recursive = true;

  static inline ParamsInfo params = ParamsInfo(ParamsInfo::Param(
      "Recursive", SHCCSTR("If the iteration should be recursive, following sub-directories."), CoreInfo::BoolType));
  static SHParametersInfo parameters() { return SHParametersInfo(params); }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _recursive = bool(Var(value));
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_recursive);
    default:
      return Var::Empty;
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    shards::arrayResize(_storage, 0);
    _strings.clear();

    if (_recursive) {
      fs::path p(input.payload.stringValue);
      auto diterator = fs::recursive_directory_iterator(p);
      for (auto &subp : diterator) {
        auto &path = subp.path();
        auto str = path.string();
#ifdef _WIN32
        boost::replace_all(str, "\\", "/");
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
        boost::replace_all(str, "\\", "/");
#endif
        _strings.push_back(str);
      }
    }

    shards::arrayResize(_storage, 0);
    for (auto &sref : _strings) {
      shards::arrayPush(_storage, Var(sref.c_str()));
    }

    return Var(_storage);
  }
};

struct Extension {
  std::string _output;

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    _output.clear();
    fs::path p(input.payload.stringValue);
    auto ext = p.extension();
    _output.assign(ext.string());
    return Var(_output);
  }
};

struct IsFile {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }
  SHVar activate(SHContext *context, const SHVar &input) {
    fs::path p(input.payload.stringValue);
    if (fs::exists(p) && !fs::is_directory(p)) {
      return Var::True;
    } else {
      return Var::False;
    }
  }
};

struct IsDirectory {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }
  SHVar activate(SHContext *context, const SHVar &input) {
    fs::path p(input.payload.stringValue);
    if (fs::exists(p) && fs::is_directory(p)) {
      return Var::True;
    } else {
      return Var::False;
    }
  }
};

struct Remove {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }
  SHVar activate(SHContext *context, const SHVar &input) {
    fs::path p(input.payload.stringValue);
    if (fs::exists(p)) {
      return Var(fs::remove(p));
    } else {
      return Var::False;
    }
  }
};

struct Filename {
  std::string _output;
  bool _noExt = false;

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  static inline ParamsInfo params = ParamsInfo(
      ParamsInfo::Param("NoExtension", SHCCSTR("If the extension should be stripped from the result."), CoreInfo::BoolType));
  static SHParametersInfo parameters() { return SHParametersInfo(params); }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _noExt = bool(Var(value));
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_noExt);
    default:
      return Var::Empty;
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
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

struct Read {
  std::vector<uint8_t> _buffer;
  bool _binary = false;

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  SHTypesInfo outputTypes() {
    if (_binary)
      return CoreInfo::BytesType;
    else
      return CoreInfo::StringType;
  }

  static inline ParamsInfo params =
      ParamsInfo(ParamsInfo::Param("Bytes", SHCCSTR("If the output should be Bytes instead of String."), CoreInfo::BoolType));
  static SHParametersInfo parameters() { return SHParametersInfo(params); }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _binary = bool(Var(value));
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_binary);
    default:
      return Var::Empty;
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    _buffer.clear();
    fs::path p(input.payload.stringValue);
    if (!fs::exists(p)) {
      SHLOG_ERROR("File is missing: {}", p);
      throw ActivationError("FS.Read, file does not exist.");
    }

    if (_binary) {
      std::ifstream file(p.string(), std::ios::binary);
      _buffer.assign(std::istreambuf_iterator<char>(file), {});
      return Var(_buffer.data(), uint32_t(_buffer.size()));
    } else {
      std::ifstream file(p.string(), std::ios::binary);
      _buffer.assign(std::istreambuf_iterator<char>(file), {});
      _buffer.push_back(0);
      return Var((const char *)_buffer.data());
    }
  }
};

struct Write {
  ParamVar _contents{};
  std::array<SHExposedTypeInfo, 2> _requiring;
  bool _overwrite = false;
  bool _append = false;

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  static inline Parameters params{
      {"Contents",
       SHCCSTR("The string or bytes to write as the file's contents."),
       {CoreInfo::StringType, CoreInfo::BytesType, CoreInfo::StringVarType, CoreInfo::BytesVarType, CoreInfo::NoneType}},
      {"Overwrite", SHCCSTR("Overwrite the file if it already exists."), {CoreInfo::BoolType}},
      {"Append", SHCCSTR("If we should append Contents to an existing file."), {CoreInfo::BoolType}}};

  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _contents = value;
      break;
    case 1:
      _overwrite = value.payload.boolValue;
      break;
    case 2:
      _append = value.payload.boolValue;
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _contents;
    case 1:
      return Var(_overwrite);
    case 2:
      return Var(_append);
    default:
      return Var::Empty;
    }
  }

  SHExposedTypesInfo requiredVariables() {
    if (_contents.isVariable()) {
      _requiring[0].name = _contents.variableName();
      _requiring[0].help = SHCCSTR("The required variable containing the data to be written.");
      _requiring[0].exposedType = CoreInfo::StringType;
      _requiring[1].name = _contents.variableName();
      _requiring[1].help = SHCCSTR("The required variable containing the data to be written.");
      _requiring[1].exposedType = CoreInfo::BytesType;
      return {_requiring.data(), 2, 0};
    } else {
      return {};
    }
  }

  void cleanup() { _contents.cleanup(); }
  void warmup(SHContext *context) { _contents.warmup(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto contents = _contents.get();
    if (contents.valueType != None) {
      fs::path p(input.payload.stringValue);
      if (!_overwrite && !_append && fs::exists(p)) {
        throw ActivationError("FS.Write, file already exists and overwrite flag is not on!.");
      }

      // make sure to create directories
      auto parent_path = p.parent_path();
      if (!parent_path.empty() && !fs::exists(parent_path))
        fs::create_directories(p.parent_path());

      std::ios::openmode flags = std::ios::binary;
      if (_append) {
        flags |= std::ios::app;
      }
      std::ofstream file(p.string(), flags);
      if (contents.valueType == String) {
        auto len = contents.payload.stringLen > 0 || contents.payload.stringValue == nullptr
                       ? contents.payload.stringLen
                       : strlen(contents.payload.stringValue);
        file.write((const char *)contents.payload.stringValue, len);
      } else {
        file.write((const char *)contents.payload.bytesValue, contents.payload.bytesSize);
      }
    }
    return input;
  }
};

struct Copy {
  enum class OverBehavior { Fail, Skip, Overwrite, Update };
  static inline EnumInfo<OverBehavior> OverWEnum{"IfExists", CoreCC, 'fsow'};
  static inline Type OverWEnumType{{SHType::Enum, {.enumeration = {CoreCC, 'fsow'}}}};

  ParamVar _destination{};
  OverBehavior _overwrite{OverBehavior::Fail};

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  static inline ParamsInfo params =
      ParamsInfo(ParamsInfo::Param("Destination", SHCCSTR("The destination path, can be a file or a directory."),
                                   CoreInfo::StringStringVarOrNone),
                 ParamsInfo::Param("Behavior", SHCCSTR("What to do when the destination already exists."), OverWEnumType));
  static SHParametersInfo parameters() { return SHParametersInfo(params); }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _destination = value;
      break;
    case 1:
      _overwrite = OverBehavior(value.payload.enumValue);
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _destination;
    case 1:
      return Var::Enum(_overwrite, CoreCC, 'fsow');
    default:
      return Var::Empty;
    }
  }

  void cleanup() { _destination.cleanup(); }
  void warmup(SHContext *context) { _destination.warmup(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    const auto src = fs::path(input.payload.stringValue);
    if (!fs::exists(src))
      throw ActivationError("Source path does not exist.");

    fs::copy_options options{};

    switch (_overwrite) {
    case OverBehavior::Fail:
      break;
    case OverBehavior::Skip:
      options |= fs::copy_options::skip_existing;
      break;
    case OverBehavior::Overwrite:
      options |= fs::copy_options::overwrite_existing;
      break;
    case OverBehavior::Update:
      options |= fs::copy_options::update_existing;
      break;
    }

    const auto dstVar = _destination.get();
    if (dstVar.valueType != String && dstVar.valueType != Path)
      throw ActivationError("Destination is not a valid");
    const auto dst = fs::path(dstVar.payload.stringValue);

    ErrorCode err;
    if (fs::is_regular_file(src) && (!fs::exists(dst) || fs::is_regular_file(dst))) {
      fs::copy_file(src, dst, options, err);
      if (err) {
        SHLOG_ERROR("copy error: {}", err.message());
        throw ActivationError("Copy failed.");
      }
    } else {
      options |= fs::copy_options::recursive;
      fs::copy(src, dst, options, err);
      if (err) {
        SHLOG_ERROR("copy error: {}", err.message());
        throw ActivationError("Copy failed.");
      }
    }

    return input;
  }
};
}; // namespace FS

void registerFSShards() {
  REGISTER_SHARD("FS.Iterate", FS::Iterate);
  REGISTER_SHARD("FS.Extension", FS::Extension);
  REGISTER_SHARD("FS.Filename", FS::Filename);
  REGISTER_SHARD("FS.Read", FS::Read);
  REGISTER_SHARD("FS.Write", FS::Write);
  REGISTER_SHARD("FS.IsFile", FS::IsFile);
  REGISTER_SHARD("FS.IsDirectory", FS::IsDirectory);
  REGISTER_SHARD("FS.Copy", FS::Copy);
  REGISTER_SHARD("FS.Remove", FS::Remove);
}
}; // namespace shards
