#ifndef B700B4F5_BFEE_4FA6_B32C_46A88F2B0E35
#define B700B4F5_BFEE_4FA6_B32C_46A88F2B0E35

#include <shards/core/shared.hpp>
#include <boost/filesystem.hpp>
#include <shards/core/ops_internal.hpp>

namespace fs = boost::filesystem;

namespace shards {
struct FileBase {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static inline Parameters params{{"File", SHCCSTR("The file to read/write from."), {CoreInfo::StringStringVarOrNone}}};

  ParamVar _filename{};
  OwnedVar _currentFileName{};

  void cleanup(SHContext *context) { _filename.cleanup(); }
  void warmup(SHContext *context) { _filename.warmup(context); }

  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _filename = value;
      cleanup(nullptr);
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
    if (ctxFile.valueType != SHType::String) {
      SHLOG_DEBUG("FileBase: File parameter is not a string. ({}/{})", ctxFile, *_filename);
      return false;
    }

    filename = SHSTRVIEW(ctxFile);
    _currentFileName = _filename.get();

    // if absolute we are fine to begin with
    fs::path fp(filename);
    if (checkExists) {
      bool e = fs::exists(fp);
      if (!e) {
        SHLOG_DEBUG("FileBase: File does not exist. ({}/{})", ctxFile, *_filename);
      }
      return e;
    } else {
      return true;
    }
  }
};

inline bool getPathChecked(std::string &outFilename, const SHVar &name, bool checkExists = true) {
  if (name.valueType != SHType::String) {
    SHLOG_DEBUG("FileBase: File parameter is not a string. ({})", name);
    return false;
  }

  outFilename = SHSTRVIEW(name);

  // if absolute we are fine to begin with
  fs::path fp(outFilename);
  if (checkExists) {
    bool e = fs::exists(fp);
    if (!e) {
      SHLOG_DEBUG("FileBase: File does not exist. ({})", name);
    }
    return e;
  } else {
    return true;
  }
}
} // namespace shards

#endif /* B700B4F5_BFEE_4FA6_B32C_46A88F2B0E35 */
