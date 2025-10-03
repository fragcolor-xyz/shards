/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include <boost/filesystem/operations.hpp>
#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/utility.hpp>
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <fcntl.h>
#include <limits>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <boost/filesystem.hpp>
#include <system_error>
namespace fs = boost::filesystem;
using ErrorCode = boost::system::error_code;

namespace shards {
namespace FS {

struct FileNotFoundException : public std::runtime_error {
  FileNotFoundException(const std::string &err) : std::runtime_error(err) {}
};

// Path security helper
// Validates that a path is within the allowed basePath by resolving symlinks and checking containment
static void validateBasePath(const fs::path &path, const fs::path &basePath) {
  if (basePath.empty())
    return;

  ErrorCode ec;

  // Resolve basePath to its canonical form (must exist)
  auto canonicalBase = fs::canonical(basePath, ec);
  if (ec) {
    throw ActivationError(fmt::format("BasePath does not exist or cannot be resolved: {}", basePath.string()));
  }

  // For the path, try to resolve it canonically
  // If it doesn't exist (e.g., for Write operations), resolve the parent directory
  fs::path resolvedPath;
  auto canonicalPath = fs::canonical(path, ec);

  if (ec) {
    // Path doesn't exist, try resolving parent directory
    auto parent = path.parent_path();
    if (parent.empty()) {
      parent = fs::current_path();
    }

    auto canonicalParent = fs::canonical(parent, ec);
    if (ec) {
      throw ActivationError(fmt::format("Path parent directory does not exist: {}", parent.string()));
    }

    // Combine canonical parent with filename
    resolvedPath = canonicalParent / path.filename();
  } else {
    resolvedPath = canonicalPath;
  }

  SHLOG_TRACE("validateBasePath: path='{}' basePath='{}'", path.string(), basePath.string());
  SHLOG_TRACE("validateBasePath: resolved path='{}' canonical base='{}'", resolvedPath.string(), canonicalBase.string());

  // Check if resolved path is within canonical base
  // Use mismatch to find where paths diverge
  auto pathIter = resolvedPath.begin();
  auto baseIter = canonicalBase.begin();

  while (baseIter != canonicalBase.end()) {
    if (pathIter == resolvedPath.end() || *pathIter != *baseIter) {
      throw ActivationError("Path is outside allowed BasePath");
    }
    ++pathIter;
    ++baseIter;
  }

  SHLOG_TRACE("validateBasePath: path is within basePath");
}

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

    fs::path p(SHSTRING_PREFER_SHSTRVIEW(input));

    if (!fs::exists(p)) {
      throw ActivationError(fmt::format("FS.Iterate, path {} does not exist.", p));
    }

    if (_recursive) {
      auto dIterator = fs::recursive_directory_iterator(p);
      for (auto &subP : dIterator) {
        auto &path = subP.path();
        auto str = path.string();
#ifdef _WIN32
        boost::replace_all(str, "\\", "/");
#endif
        _strings.push_back(str);
      }
    } else {
      auto dIterator = fs::directory_iterator(p);
      for (auto &subP : dIterator) {
        auto &path = subP.path();
        auto str = path.string();
#ifdef _WIN32
        boost::replace_all(str, "\\", "/");
#endif
        _strings.push_back(str);
      }
    }

    shards::arrayResize(_storage, 0);
    for (auto &sRef : _strings) {
      shards::arrayPush(_storage, Var(sRef));
    }

    return Var(_storage);
  }
};

struct Join {
  fs::path _buffer;
  std::string _result;

  static SHTypesInfo inputTypes() { return CoreInfo::StringSeqType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    _buffer.clear();

    for (auto &v : input) {
      fs::path path(SHSTRING_PREFER_SHSTRVIEW(v));
      _buffer /= path;
    }
    _result = _buffer.string();
    return Var(_result);
  }
};

struct Extension {
  std::string _output;

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    _output.clear();
    fs::path p(SHSTRING_PREFER_SHSTRVIEW(input));
    auto ext = p.extension();
    _output.assign(ext.string());
    return Var(_output);
  }
};

struct ReplaceExtension {
  std::string _output;  

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  PARAM_PARAMVAR(_newExtension, "NewExtension", "The new extension to replace the existing extension with", {CoreInfo::StringOrStringVar});
  PARAM_IMPL(PARAM_IMPL_FOR(_newExtension));

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    if (_newExtension.isNone()) {
      throw ComposeError("NewExtension parameter is required and cannot be None");
    }
    return data.inputType;
  }
  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    _output.clear();
    fs::path p(SHSTRING_PREFER_SHSTRVIEW(input));
    p = p.replace_extension(SHSTRING_PREFER_SHSTRVIEW(_newExtension.get()));
    _output.assign(p.string());
    return Var(_output);
  }
};
struct IsFile {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }
  SHVar activate(SHContext *context, const SHVar &input) {
    fs::path p(SHSTRING_PREFER_SHSTRVIEW(input));
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
    fs::path p(SHSTRING_PREFER_SHSTRVIEW(input));
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

  PARAM_PARAMVAR(_basePath, "BasePath", "Optional base path - restricts operations to this directory", CoreInfo::StringStringVarOrNone);
  PARAM_VAR(_followSymlinks, "FollowSymlinks", "Follow symbolic links (default: true). Set to false to reject symlinks", {CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_basePath), PARAM_IMPL_FOR(_followSymlinks));

  Remove() { _followSymlinks = Var(true); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }
  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    fs::path p(SHSTRING_PREFER_SHSTRVIEW(input));

    // Security check via BasePath
    auto basePath = _basePath.get();
    if (basePath.valueType == SHType::String) {
      validateBasePath(p, fs::path(SHSTRING_PREFER_SHSTRVIEW(basePath)));
    }

    if (!_followSymlinks.payload.boolValue && fs::exists(p) && fs::is_symlink(p)) {
      throw ActivationError("FS.Remove, path is a symlink and FollowSymlinks is disabled");
    }

    if (fs::exists(p)) {
      return Var(fs::remove(p));
    } else {
      return Var::False;
    }
  }
};

struct RemoveAll {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::IntType; }

  PARAM_PARAMVAR(_basePath, "BasePath", "Optional base path - restricts operations to this directory", CoreInfo::StringStringVarOrNone);
  PARAM_VAR(_followSymlinks, "FollowSymlinks", "Follow symbolic links (default: true). Set to false to reject symlinks", {CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_basePath), PARAM_IMPL_FOR(_followSymlinks));

  RemoveAll() { _followSymlinks = Var(true); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }
  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    fs::path p(SHSTRING_PREFER_SHSTRVIEW(input));

    // Security check via BasePath
    auto basePath = _basePath.get();
    if (basePath.valueType == SHType::String) {
      validateBasePath(p, fs::path(SHSTRING_PREFER_SHSTRVIEW(basePath)));
    }

    if (!_followSymlinks.payload.boolValue && fs::exists(p) && fs::is_symlink(p)) {
      throw ActivationError("FS.RemoveAll, path is a symlink and FollowSymlinks is disabled");
    }

    if (fs::exists(p)) {
      return Var(SHInt(fs::remove_all(p)));
    } else {
      return Var(0);
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
    fs::path p(SHSTRING_PREFER_SHSTRVIEW(input));
    p = p.filename();
    if (_noExt && p.has_extension()) {
      p = p.replace_extension("");
    }
    _output.assign(p.string());
    return Var(_output);
  }
};

struct RelativeTo {
  std::string _output;
  fs::path _cachedBasePath;
  fs::path _cachedPath;
  bool _noExt = false;

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  PARAM_PARAMVAR(_basePath, "BasePath", "The base path to make the input path relative to", {CoreInfo::StringOrStringVar});
  PARAM_IMPL(PARAM_IMPL_FOR(_basePath));

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    if (_basePath.isNone()) {
      throw ComposeError("BasePath parameter is required and cannot be None");
    }
    return outputTypes().elements[0];
  }
  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    _output.clear();

    _cachedPath = SHSTRING_PREFER_SHSTRVIEW(input);
    _cachedBasePath = SHSTRING_PREFER_SHSTRVIEW(_basePath.get());

    _output.assign(fs::relative(_cachedPath, _cachedBasePath).string());
    return Var(_output);
  }
};

struct Parent {
  std::string _output;

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    _output.clear();
    fs::path p(SHSTRING_PREFER_SHSTRVIEW(input));
    p = p.parent_path();
    _output.assign(p.string());
    return Var(_output);
  }
};

struct Read {
  std::vector<uint8_t> _buffer;
  std::ifstream _file;
  std::string _currentPath;  // Track current file for chunked reading
  bool _chunking = false;

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() {
    static Types _types{CoreInfo::BytesType, CoreInfo::StringType};
    return _types;
  }

  PARAM_VAR(_binary, "Bytes", "If the output should be SHType::Bytes instead of SHType::String", {CoreInfo::BoolType});
  PARAM_VAR(_maxSize, "MaxSize", "Maximum file size in bytes (default: 0 = unlimited). Set to limit file size and prevent memory exhaustion", {CoreInfo::IntType});
  PARAM_VAR(_chunkSize, "ChunkSize", "If set, enables chunked reading. Returns ChunkSize bytes per activation. Returns empty when EOF", {CoreInfo::IntType});
  PARAM_PARAMVAR(_basePath, "BasePath", "Optional base path - restricts read operations to this directory", CoreInfo::StringStringVarOrNone);
  PARAM_VAR(_followSymlinks, "FollowSymlinks", "Follow symbolic links (default: true). Set to false to reject symlinks", {CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_binary), PARAM_IMPL_FOR(_maxSize), PARAM_IMPL_FOR(_chunkSize), PARAM_IMPL_FOR(_basePath), PARAM_IMPL_FOR(_followSymlinks));

  Read() {
    _binary = Var(false);
    _maxSize = Var(SHInt(0));
    _chunkSize = Var(SHInt(0));
    _followSymlinks = Var(true);
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    return _binary.payload.boolValue ? CoreInfo::BytesType : CoreInfo::StringType;
  }

  void cleanup(SHContext *context) {
    if (_file.is_open()) {
      _file.close();
    }
    _chunking = false;
    _currentPath.clear();
    _buffer = {};
    PARAM_CLEANUP(context);
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    _buffer.clear();
    fs::path p(SHSTRING_PREFER_SHSTRVIEW(input));
    std::string pathStr = p.string();

    size_t chunkSize = _chunkSize.payload.intValue < 0 ? 0 : size_t(_chunkSize.payload.intValue);
    size_t maxSize = _maxSize.payload.intValue < 0 ? 0 : size_t(_maxSize.payload.intValue);

    // Check if path changed during chunked reading - this is an error
    if (_chunking && _currentPath != pathStr) {
      throw ActivationError(fmt::format("FS.Read: Cannot change file path during chunked reading. Current: {}, New: {}", _currentPath, pathStr));
    }

    // Security checks
    if (!_chunking) {
      auto basePath = _basePath.get();
      if (basePath.valueType == SHType::String) {
        validateBasePath(p, fs::path(SHSTRING_PREFER_SHSTRVIEW(basePath)));
      }

      if (!_followSymlinks.payload.boolValue && fs::exists(p) && fs::is_symlink(p)) {
        throw ActivationError("FS.Read, path is a symlink and FollowSymlinks is disabled");
      }
    }

    if (chunkSize > 0) {
      // Chunked reading mode - stateful
      if (!_file.is_open()) {
        // First activation or reset: validate and open file
        _currentPath = pathStr;
        if (!fs::exists(p)) {
          SHLOG_ERROR("File is missing: {}", p);
          throw FileNotFoundException("FS.Read, file does not exist.");
        }

        ErrorCode ec;
        auto fileSize = fs::file_size(p, ec);
        if (ec) {
          throw ActivationError("FS.Read, unable to determine file size.");
        }

        if (maxSize > 0 && fileSize > maxSize) {
          throw ActivationError("FS.Read, file size (" + std::to_string(fileSize) +
                                " bytes) exceeds MaxSize limit (" + std::to_string(maxSize) + " bytes).");
        }

        _file.open(p.string(), std::ios::binary);
        if (!_file.is_open()) {
          throw ActivationError("FS.Read, failed to open file for chunked reading.");
        }
        _chunking = true;
      }

      // Read next chunk
      _buffer.resize(chunkSize);
      _file.read(reinterpret_cast<char*>(_buffer.data()), chunkSize);
      auto bytesRead = _file.gcount();

      if (bytesRead == 0) {
        // EOF - close file and return empty
        _file.close();
        _chunking = false;
        _currentPath.clear();
        if (_binary.payload.boolValue) {
          return Var(_buffer.data(), 0);
        } else {
          return Var("", 0);
        }
      }

      _buffer.resize(bytesRead);

      if (_binary.payload.boolValue) {
        return Var(_buffer.data(), uint32_t(_buffer.size()));
      } else {
        _buffer.push_back(0);
        return Var((const char *)_buffer.data(), _buffer.size() - 1);
      }
    } else {
      // Normal mode: read entire file with size validation
      if (!fs::exists(p)) {
        SHLOG_ERROR("File is missing: {}", p);
        throw FileNotFoundException("FS.Read, file does not exist.");
      }

      ErrorCode ec;
      auto fileSize = fs::file_size(p, ec);
      if (ec) {
        throw ActivationError("FS.Read, unable to determine file size.");
      }

      if (maxSize > 0 && fileSize > maxSize) {
        throw ActivationError("FS.Read, file size (" + std::to_string(fileSize) +
                              " bytes) exceeds MaxSize limit (" + std::to_string(maxSize) + " bytes).");
      }

      if (_binary.payload.boolValue) {
        std::ifstream file(p.string(), std::ios::binary);
        _buffer.assign(std::istreambuf_iterator<char>(file), {});
        return Var(_buffer.data(), uint32_t(_buffer.size()));
      } else {
        std::ifstream file(p.string(), std::ios::binary);
        _buffer.assign(std::istreambuf_iterator<char>(file), {});
        _buffer.push_back(0);
        return Var((const char *)_buffer.data(), _buffer.size() - 1);
      }
    }
  }
};

struct Write {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  PARAM_PARAMVAR(_contents, "Contents", "The string or bytes to write as the file's contents",
                 {CoreInfo::StringType, CoreInfo::BytesType, CoreInfo::StringVarType, CoreInfo::BytesVarType, CoreInfo::NoneType});
  PARAM_VAR(_overwrite, "Overwrite", "Overwrite the file if it already exists", {CoreInfo::BoolType});
  PARAM_VAR(_append, "Append", "If we should append Contents to an existing file", {CoreInfo::BoolType});
  PARAM_PARAMVAR(_basePath, "BasePath", "Optional base path - restricts write operations to this directory", CoreInfo::StringStringVarOrNone);
  PARAM_IMPL(PARAM_IMPL_FOR(_contents), PARAM_IMPL_FOR(_overwrite), PARAM_IMPL_FOR(_append), PARAM_IMPL_FOR(_basePath));

  Write() {
    _overwrite = Var(false);
    _append = Var(false);
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }
  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto contents = _contents.get();
    if (contents.valueType != SHType::None) {
      fs::path p(SHSTRING_PREFER_SHSTRVIEW(input));

      // Security check via BasePath
      auto basePath = _basePath.get();
      if (basePath.valueType == SHType::String) {
        validateBasePath(p, fs::path(SHSTRING_PREFER_SHSTRVIEW(basePath)));
      }

      if (!_overwrite.payload.boolValue && !_append.payload.boolValue && fs::exists(p)) {
        throw ActivationError("FS.Write, file already exists and overwrite flag is not on!.");
      }

      // make sure to create directories
      auto parent_path = p.parent_path();
      if (!parent_path.empty() && !fs::exists(parent_path))
        fs::create_directories(p.parent_path());

      std::ios::openmode flags = std::ios::binary;
      if (_append.payload.boolValue) {
        flags |= std::ios::app;
      }
      std::ofstream file(p.string(), flags);
      if (contents.valueType == SHType::String) {
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
  enum class IfExists { Fail, Skip, Overwrite, Update };
  DECL_ENUM_INFO(IfExists, IfExists,
                 "Action to take when a destination file already exists during a copy operation. Determines whether to fail, "
                 "skip, overwrite, or update the file.",
                 'fsow');

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  PARAM_PARAMVAR(_destination, "Destination", "The destination path, can be a file or a directory", CoreInfo::StringStringVarOrNone);
  PARAM_VAR(_overwrite, "Behavior", "What to do when the destination already exists", {IfExistsEnumInfo::Type});
  PARAM_PARAMVAR(_basePath, "BasePath", "Optional base path - restricts operations to this directory", CoreInfo::StringStringVarOrNone);
  PARAM_VAR(_followSymlinks, "FollowSymlinks", "Follow symbolic links (default: true). Set to false to reject symlinks", {CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_destination), PARAM_IMPL_FOR(_overwrite), PARAM_IMPL_FOR(_basePath), PARAM_IMPL_FOR(_followSymlinks));

  Copy() {
    _overwrite = Var::Enum(IfExists::Fail, CoreCC, IfExistsEnumInfo::TypeId);
    _followSymlinks = Var(true);
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }
  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    const auto src = fs::path(SHSTRING_PREFER_SHSTRVIEW(input));
    if (!fs::exists(src))
      throw FileNotFoundException("Source path does not exist.");

    // Security checks via BasePath for source
    auto basePath = _basePath.get();
    if (basePath.valueType == SHType::String) {
      validateBasePath(src, fs::path(SHSTRING_PREFER_SHSTRVIEW(basePath)));
    }

    if (!_followSymlinks.payload.boolValue && fs::is_symlink(src)) {
      throw ActivationError("FS.Copy, source is a symlink and FollowSymlinks is disabled");
    }

    fs::copy_options options{};

    auto behavior = IfExists(_overwrite.payload.enumValue);
    switch (behavior) {
    case IfExists::Fail:
      break;
    case IfExists::Skip:
      options |= fs::copy_options::skip_existing;
      break;
    case IfExists::Overwrite:
      options |= fs::copy_options::overwrite_existing;
      break;
    case IfExists::Update:
      options |= fs::copy_options::update_existing;
      break;
    }

    const auto dstVar = _destination.get();
    if (dstVar.valueType != SHType::String && dstVar.valueType != SHType::Path)
      throw ActivationError("Destination is not a valid");
    const auto dst = fs::path(SHSTRING_PREFER_SHSTRVIEW(dstVar));

    // Security checks via BasePath for destination
    if (basePath.valueType == SHType::String) {
      validateBasePath(dst, fs::path(SHSTRING_PREFER_SHSTRVIEW(basePath)));
    }

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

struct LastWriteTime {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::IntType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    fs::path p(SHSTRING_PREFER_SHSTRVIEW(input));
    ErrorCode ec;
    auto lwt = fs::last_write_time(p, ec);
    if (ec.failed()) {
      throw FileNotFoundException(fmt::format("FS.LastWriteTime, file {} does not exist.", p));
    }
    return Var(static_cast<int64_t>(lwt));
  }
};

struct SetWriteTime {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  PARAM_PARAMVAR(_time, "Time", "The new time to set as last write time", {CoreInfo::IntOrIntVar});
  PARAM_IMPL(PARAM_IMPL_FOR(_time));

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    if (_time.isNone()) {
      throw ComposeError("Time parameter is required and cannot be None");
    }
    return data.inputType;
  }
  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    fs::path p(SHSTRING_PREFER_SHSTRVIEW(input));
    auto time = _time.get();
    ErrorCode ec;
    fs::last_write_time(p, time.payload.intValue, ec);
    if (ec.failed()) {
      throw FileNotFoundException(fmt::format("FS.SetWriteTime, file {} does not exist.", p));
    }
    return input;
  }
};

struct CreateDirectories {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  PARAM_PARAMVAR(_basePath, "BasePath", "Optional base path - restricts operations to this directory", CoreInfo::StringStringVarOrNone);
  PARAM_IMPL(PARAM_IMPL_FOR(_basePath));

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return data.inputType;
  }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }
  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    fs::path p(SHSTRING_PREFER_SHSTRVIEW(input));

    // Security check via BasePath
    auto basePath = _basePath.get();
    if (basePath.valueType == SHType::String) {
      validateBasePath(p, fs::path(SHSTRING_PREFER_SHSTRVIEW(basePath)));
    }

    if (!fs::exists(p)) {
      if (!fs::create_directories(p)) {
        throw ActivationError(fmt::format("FS.CreateDirectories, failed to create directories for {}.", p));
      }
    }
    return input;
  }
};

struct Absolute {
  std::string _output;

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  PARAM_PARAMVAR(_canonical, "Canonical", "Whether to canonicalize the path, the file should exist for this to work",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_canonical));

  Absolute() { _canonical = Var(false); }

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return data.inputType;
  }
  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    _output.clear();
    fs::path p(SHSTRING_PREFER_SHSTRVIEW(input));
    fs::path basePath = fs::current_path();
    p = _canonical.get().payload.boolValue ? fs::canonical(p) : fs::absolute(p, basePath);
    _output.assign(p.string());

    return Var(_output);
  }
};

struct IsAbsolute {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    fs::path p(SHSTRING_PREFER_SHSTRVIEW(input));
    return Var(p.is_absolute());
  }
};

struct Normalize {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  static SHOptionalString help() {
    return SHCCSTR("Returns a normalized path by removing redundant separators and up-level references");
  }

  std::string _output;

  SHVar activate(SHContext *context, const SHVar &input) {
    _output.clear();
    fs::path p(SHSTRING_PREFER_SHSTRVIEW(input));
    _output = p.lexically_normal().string();
    return Var(_output);
  }
};

struct Rename {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  PARAM_PARAMVAR(_newName, "NewName", "The new name for the file", CoreInfo::StringOrStringVar);
  PARAM_PARAMVAR(_basePathParam, "BasePath", "Optional base path - restricts operations to this directory", CoreInfo::StringStringVarOrNone);
  PARAM_IMPL(PARAM_IMPL_FOR(_newName), PARAM_IMPL_FOR(_basePathParam));

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    if (_newName.isNone()) {
      throw ComposeError("NewName parameter is required and cannot be None");
    }
    return data.inputType;
  }
  
  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    fs::path p(SHSTRING_PREFER_SHSTRVIEW(input));

    // Security checks via BasePath for source
    auto basePath = _basePathParam.get();
    if (basePath.valueType == SHType::String) {
      validateBasePath(p, fs::path(SHSTRING_PREFER_SHSTRVIEW(basePath)));
    }

    // check exists
    if (!fs::exists(p)) {
      throw ActivationError(fmt::format("FS.Rename, file {} does not exist.", p));
    }

    auto newName = SHSTRING_PREFER_SHSTRVIEW(_newName.get());
    fs::path newPath(newName);

    // Security checks via BasePath for destination
    if (basePath.valueType == SHType::String) {
      validateBasePath(newPath, fs::path(SHSTRING_PREFER_SHSTRVIEW(basePath)));
    }

    // check if exists
    if (fs::exists(newPath)) {
      throw ActivationError(fmt::format("FS.Rename, file {} already exists.", newName));
    }
    fs::rename(p, newPath);
    return input;
  }
};

struct Is {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }

  static SHOptionalString help() { return SHCCSTR("Returns true if two paths refer to the same file system object"); }

  PARAM_PARAMVAR(_other, "Other", "Path to compare with", {CoreInfo::StringOrStringVar});
  PARAM_IMPL(PARAM_IMPL_FOR(_other));

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    if (_other.isNone()) {
      throw ComposeError("Other parameter is required and cannot be None");
    }
    return data.inputType;
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    fs::path p1(SHSTRING_PREFER_SHSTRVIEW(input));
    fs::path p2(SHSTRING_PREFER_SHSTRVIEW(_other.get()));
    return Var(fs::equivalent(p1, p2));
  }
};

struct IsAny {
  static SHTypesInfo inputTypes() { return CoreInfo::StringSeqType; }
  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }

  static SHOptionalString help() {
    return SHCCSTR("Returns true if the input sequence of paths contains a path equivalent to the given path");
  }

  PARAM_PARAMVAR(_other, "Other", "Path to compare with", {CoreInfo::StringOrStringVar});
  PARAM_IMPL(PARAM_IMPL_FOR(_other));

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    if (_other.isNone()) {
      throw ComposeError("Other parameter is required and cannot be None");
    }
    return CoreInfo::BoolType;
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto p2 = fs::path(SHSTRING_PREFER_SHSTRVIEW(_other.get()));
    for (size_t i = 0; i < input.payload.seqValue.len; i++) {
      auto p1 = fs::path(SHSTRING_PREFER_SHSTRVIEW(input.payload.seqValue.elements[i]));
      if (fs::equivalent(p1, p2)) {
        return Var(true);
      }
    }
    return Var(false);
  }
};

struct IsNotAny {
  static SHTypesInfo inputTypes() { return CoreInfo::StringSeqType; }
  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }

  static SHOptionalString help() {
    return SHCCSTR("Returns true if the input sequence of paths does not contain a path equivalent to the given path");
  }

  PARAM_PARAMVAR(_other, "Other", "Path to compare with", {CoreInfo::StringOrStringVar});
  PARAM_IMPL(PARAM_IMPL_FOR(_other));

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    if (_other.isNone()) {
      throw ComposeError("Other parameter is required and cannot be None");
    }
    return CoreInfo::BoolType;
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto p2 = fs::path(SHSTRING_PREFER_SHSTRVIEW(_other.get()));
    for (size_t i = 0; i < input.payload.seqValue.len; i++) {
      auto p1 = fs::path(SHSTRING_PREFER_SHSTRVIEW(input.payload.seqValue.elements[i]));
      if (fs::equivalent(p1, p2)) {
        return Var(false);
      }
    }
    return Var(true);
  }
};

SHARDS_REGISTER_FN(fs) {
  REGISTER_ENUM(Copy::IfExistsEnumInfo);

  REGISTER_SHARD("FS.Join", Join);
  REGISTER_SHARD("FS.Iterate", Iterate);
  REGISTER_SHARD("FS.Extension", Extension);
  REGISTER_SHARD("FS.ReplaceExtension", ReplaceExtension);
  REGISTER_SHARD("FS.Filename", Filename);
  REGISTER_SHARD("FS.RelativeTo", RelativeTo);
  REGISTER_SHARD("FS.Parent", Parent);
  REGISTER_SHARD("FS.Read", Read);
  REGISTER_SHARD("FS.Write", Write);
  REGISTER_SHARD("FS.IsFile", IsFile);
  REGISTER_SHARD("FS.IsDirectory", IsDirectory);
  REGISTER_SHARD("FS.Copy", Copy);
  REGISTER_SHARD("FS.Remove", Remove);
  REGISTER_SHARD("FS.RemoveAll", RemoveAll);
  REGISTER_SHARD("FS.LastWriteTime", LastWriteTime);
  REGISTER_SHARD("FS.SetWriteTime", SetWriteTime);
  REGISTER_SHARD("FS.CreateDirectories", CreateDirectories);
  REGISTER_SHARD("FS.Absolute", Absolute);
  REGISTER_SHARD("FS.IsAbsolute", IsAbsolute);
  REGISTER_SHARD("FS.Normalize", Normalize);
  REGISTER_SHARD("FS.Rename", Rename);
  REGISTER_SHARD("FS.Is", Is);
  REGISTER_SHARD("FS.IsAny", IsAny);
  REGISTER_SHARD("FS.IsNotAny", IsNotAny);
}

}; // namespace FS
}; // namespace shards
